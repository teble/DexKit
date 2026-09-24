//! Verify the actual endpoint with the official MCP HTTP client, not only raw requests.
use rmcp::{
    model::{CallToolRequestParams, ClientConfig, ProtocolVersion},
    transport::{
        streamable_http_client::StreamableHttpClientTransportConfig, StreamableHttpClientTransport,
    },
    ClientLifecycleMode, ClientServiceExt,
};
use serde_json::json;

#[tokio::main(flavor = "current_thread")]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut args = std::env::args().skip(1);
    let url = args.next().ok_or("Expected endpoint URL")?;
    let path = args.next().ok_or("Expected DEX path")?;
    let client = ClientConfig::default()
        .serve_with_lifecycle(
            StreamableHttpClientTransport::with_client(
                reqwest::Client::builder().no_proxy().build()?,
                StreamableHttpClientTransportConfig::with_uri(url),
            ),
            ClientLifecycleMode::Discover {
                preferred_versions: vec![ProtocolVersion::V_2026_07_28],
            },
        )
        .await?;
    assert_eq!(client.list_tools(None).await?.tools.len(), 12);
    let opened = client
        .call_tool(
            CallToolRequestParams::new("dexkit_open")
                .with_arguments(json!({"path":path}).as_object().unwrap().clone()),
        )
        .await?;
    let opened = opened
        .structured_content
        .ok_or("Missing structured open reply")?;
    assert_eq!(opened["ok"], true, "{opened}");
    let instance = opened["data"]["instanceId"].clone();
    let found = client
        .call_tool(
            CallToolRequestParams::new("dexkit_find_classes").with_arguments(
                json!({"instanceId":instance,"query":{}})
                    .as_object()
                    .unwrap()
                    .clone(),
            ),
        )
        .await?;
    assert_eq!(
        found.structured_content.unwrap()["data"]["resultSet"]["totalItems"],
        "1"
    );
    let closed = client
        .call_tool(
            CallToolRequestParams::new("dexkit_close")
                .with_arguments(json!({"instanceId":instance}).as_object().unwrap().clone()),
        )
        .await?;
    assert_eq!(closed.structured_content.unwrap()["ok"], true);
    client.cancel().await?;
    println!("Official SDK HTTP discovery/open/find/close passed");
    Ok(())
}
