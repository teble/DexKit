//! Standard Streamable HTTP transport for one local, trusted-user server.
//! Application instance IDs outlive individual HTTP requests. They are not
//! transport session IDs or authentication credentials.
use crate::{server::Server, worker::Worker};
use axum::{
    body::{Body, Bytes},
    extract::{Request, State},
    http::{Response, StatusCode},
    middleware::{self, Next},
    routing::any,
    Router,
};
use http_body::{Body as HttpBody, Frame, SizeHint};
use rmcp::transport::streamable_http_server::{
    session::never::NeverSessionManager, StreamableHttpServerConfig, StreamableHttpService,
};
use std::{
    future::{Future, IntoFuture},
    net::SocketAddr,
    pin::Pin,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
    task::{Context, Poll},
    time::Duration,
};
use tokio::sync::{OwnedSemaphorePermit, Semaphore};
use tokio_util::sync::CancellationToken;

const MAX_HTTP_STREAMS: usize = 16;

pub async fn run(
    worker: Arc<Worker>,
    address: SocketAddr,
) -> Result<(), Box<dyn std::error::Error>> {
    let listener = tokio::net::TcpListener::bind(address).await?;
    let address = listener.local_addr()?;
    let shutdown = CancellationToken::new();
    let mut config = StreamableHttpServerConfig::default()
        .with_allowed_hosts([address.to_string(), format!("localhost:{}", address.port())])
        .with_allowed_origins([
            format!("http://{address}"),
            format!("http://localhost:{}", address.port()),
        ])
        .enforce_origin_validation();
    // No transport session is necessary: state belongs to the worker's
    // explicit instanceId API, which is shared across requests/connections.
    config.legacy_session_mode = false;
    config.json_response = false; // Request-scoped SSE supports disconnect cancellation.
    config.sse_retry = None;
    config.sse_keep_alive = Some(Duration::from_secs(15));
    config.max_request_body_bytes = 2 * 1024 * 1024;
    config.cancellation_token = shutdown.clone();
    let app = router(Server::new(worker), config);
    // Register before announcing readiness, so an immediate stop is handled.
    let terminate = tokio::signal::unix::signal(tokio::signal::unix::SignalKind::terminate())?;
    let interrupt = tokio::signal::unix::signal(tokio::signal::unix::SignalKind::interrupt())?;
    eprintln!("dexkit-mcp listening on http://{address}/mcp");
    let serving = axum::serve(listener, app)
        .with_graceful_shutdown(shutdown.clone().cancelled_owned())
        .into_future();
    tokio::pin!(serving);
    tokio::select! {
        result = &mut serving => result?,
        _ = termination_signal(terminate, interrupt) => {
            shutdown.cancel();
            // A client that stops reading must not prevent process shutdown.
            if let Ok(result) = tokio::time::timeout(Duration::from_secs(3), &mut serving).await {
                result?;
            }
        }
    }
    Ok(())
}

fn router<S: rmcp::ServerHandler + Clone>(server: S, config: StreamableHttpServerConfig) -> Router {
    let endpoint = any(move |request: Request| {
        let server = server.clone();
        let config = config.clone();
        async move {
            // rmcp 3.4.0 retains unknown tool names in its schema cache. Scope
            // that transport cache to this request; the immutable catalogue,
            // application worker and global request limit remain shared.
            let transport = StreamableHttpService::new(
                move || Ok(server.clone()),
                Arc::new(NeverSessionManager::default()),
                config,
            );
            transport.handle(request).await
        }
    });
    Router::new()
        .route("/mcp", endpoint)
        .layer(middleware::from_fn_with_state(
            Arc::new(Semaphore::new(MAX_HTTP_STREAMS)),
            limit_streams,
        ))
}

async fn termination_signal(
    mut terminate: tokio::signal::unix::Signal,
    mut interrupt: tokio::signal::unix::Signal,
) {
    tokio::select! {
        _ = interrupt.recv() => (),
        _ = terminate.recv() => (),
    }
}

async fn limit_streams(
    State(limit): State<Arc<Semaphore>>,
    request: Request,
    next: Next,
) -> Response<Body> {
    let permit = match limit.try_acquire_owned() {
        Ok(permit) => permit,
        Err(_) => {
            return Response::builder()
                .status(StatusCode::SERVICE_UNAVAILABLE)
                .header("retry-after", "1")
                .body(Body::from("HTTP stream capacity reached"))
                .unwrap()
        }
    };
    // Only time reading the body. In modern MCP the SDK may wait for the first
    // handler response before sending headers, so timing next.run would also
    // impose an unintended native query deadline.
    let (parts, body) = request.into_parts();
    let timed_out = Arc::new(AtomicBool::new(false));
    let body = Body::new(TimedBody {
        inner: Box::pin(body),
        deadline: Box::pin(tokio::time::sleep(Duration::from_secs(10))),
        timed_out: timed_out.clone(),
        ended: false,
    });
    let response = next.run(Request::from_parts(parts, body)).await;
    if timed_out.load(Ordering::Relaxed) {
        return Response::builder()
            .status(StatusCode::REQUEST_TIMEOUT)
            .body(Body::from("HTTP request body timed out"))
            .unwrap();
    }
    let (parts, body) = response.into_parts();
    Response::from_parts(
        parts,
        Body::new(LimitedBody {
            inner: Box::pin(body),
            _permit: permit,
        }),
    )
}

struct TimedBody {
    inner: Pin<Box<Body>>,
    deadline: Pin<Box<tokio::time::Sleep>>,
    timed_out: Arc<AtomicBool>,
    ended: bool,
}
impl HttpBody for TimedBody {
    type Data = Bytes;
    type Error = axum::Error;
    fn poll_frame(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Result<Frame<Bytes>, Self::Error>>> {
        let this = self.get_mut();
        if this.ended {
            return Poll::Ready(None);
        }
        if this.deadline.as_mut().poll(cx).is_ready() {
            this.ended = true;
            this.timed_out.store(true, Ordering::Relaxed);
            return Poll::Ready(Some(Err(axum::Error::new(std::io::Error::new(
                std::io::ErrorKind::TimedOut,
                "HTTP body timed out",
            )))));
        }
        let result = this.inner.as_mut().poll_frame(cx);
        if matches!(result, Poll::Ready(None)) {
            this.ended = true;
        }
        result
    }
    fn is_end_stream(&self) -> bool {
        self.ended || self.inner.is_end_stream()
    }
    fn size_hint(&self) -> SizeHint {
        self.inner.size_hint()
    }
}

// Hold the capacity permit until the response stream ends or disconnects,
// rather than releasing it when SSE response headers become available.
struct LimitedBody {
    inner: Pin<Box<Body>>,
    _permit: OwnedSemaphorePermit,
}
impl HttpBody for LimitedBody {
    type Data = Bytes;
    type Error = axum::Error;
    fn poll_frame(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Result<Frame<Bytes>, Self::Error>>> {
        self.get_mut().inner.as_mut().poll_frame(cx)
    }
    fn is_end_stream(&self) -> bool {
        self.inner.is_end_stream()
    }
    fn size_hint(&self) -> SizeHint {
        self.inner.size_hint()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use rmcp::{
        model::{CallToolRequestParams, CallToolResponse, ServerCapabilities, ServerConfig, Tool},
        service::RequestContext,
        ErrorData, RoleServer, ServerHandler,
    };
    use serde_json::json;
    use std::sync::atomic::AtomicUsize;
    use tower::ServiceExt;

    #[derive(Clone)]
    struct LookupCounter(Arc<AtomicUsize>);
    impl ServerHandler for LookupCounter {
        fn get_info(&self) -> ServerConfig {
            ServerConfig::new(ServerCapabilities::builder().enable_tools().build())
        }
        fn get_tool(&self, _: &str) -> Option<Tool> {
            self.0.fetch_add(1, Ordering::Relaxed);
            None
        }
        async fn call_tool(
            &self,
            _: CallToolRequestParams,
            _: RequestContext<RoleServer>,
        ) -> Result<CallToolResponse, ErrorData> {
            Err(ErrorData::invalid_params("Unknown test tool", None))
        }
    }
    fn unknown_request(name: &str, header_name: &str) -> Request {
        Request::builder().method("POST").uri("/mcp")
            .header("host", "localhost").header("content-type", "application/json")
            .header("accept", "application/json, text/event-stream")
            .header("mcp-protocol-version", "2026-07-28")
            .header("mcp-method", "tools/call").header("mcp-name", header_name)
            .body(Body::from(serde_json::to_vec(&json!({
                "jsonrpc":"2.0","id":1,"method":"tools/call","params":{
                    "name":name,"arguments":{},"_meta":{
                        "io.modelcontextprotocol/protocolVersion":"2026-07-28",
                        "io.modelcontextprotocol/clientInfo":{"name":"cache-test","version":"1"},
                        "io.modelcontextprotocol/clientCapabilities":{}
                    }
                }
            })).unwrap())).unwrap()
    }
    #[tokio::test]
    async fn unknown_schema_cache_does_not_survive_requests() {
        let calls = Arc::new(AtomicUsize::new(0));
        let mut config = StreamableHttpServerConfig::default();
        config.legacy_session_mode = false;
        config.json_response = true;
        let app = router(LookupCounter(calls.clone()), config);
        // A persistent SDK service only looks up each missing name once. Every
        // request here must consult the same catalogue again, proving that its
        // negative cache did not survive the previous request.
        for (index, name) in [
            "missing_a",
            "missing_b",
            "missing_a",
            "missing_c",
            "missing_b",
        ]
        .into_iter()
        .enumerate()
        {
            let response = app
                .clone()
                .oneshot(unknown_request(name, name))
                .await
                .unwrap();
            assert_eq!(response.status(), StatusCode::BAD_REQUEST);
            let _ = axum::body::to_bytes(response.into_body(), 65536)
                .await
                .unwrap();
            assert_eq!(calls.load(Ordering::Relaxed), index + 1);
        }
        let response = app
            .oneshot(unknown_request("missing_a", "different_header"))
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::BAD_REQUEST);
        let bytes = axum::body::to_bytes(response.into_body(), 65536)
            .await
            .unwrap();
        let error: serde_json::Value = serde_json::from_slice(&bytes).unwrap();
        assert_eq!(error["error"]["code"], -32020);
    }
}
