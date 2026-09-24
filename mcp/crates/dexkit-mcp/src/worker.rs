//! Private worker protocol, never exposed as an MCP tool or a file-polling API.
use dexkit_rs::{
    error::{Error, Result},
    service::AnalysisService,
    InputLimits,
};
use serde::{Deserialize, Serialize};
use serde_json::{json, Value};
use std::{
    io::{self, BufRead, BufReader, Read, Write},
    path::PathBuf,
    process::{Child, Command, Stdio},
    sync::{mpsc, Arc, Mutex},
};
use tokio::sync::oneshot;

const FRAME_LIMIT: usize = 2 * 1024 * 1024;
#[derive(Serialize, Deserialize)]
#[serde(tag = "op", rename_all = "camelCase", deny_unknown_fields)]
pub enum Request {
    Call { name: String, arguments: Value },
    Resource { uri: String },
}
struct Job {
    request: Request,
    reply: oneshot::Sender<Result<Value>>,
}
pub struct Worker {
    sender: mpsc::SyncSender<Job>,
    child: Arc<Mutex<Option<Child>>>,
}
fn died() -> Error {
    Error::new(
        "native_failure",
        "WORKER_EXITED",
        "Native worker exited; all handles are invalid. Restart the MCP server.",
    )
}
impl Worker {
    pub fn start(roots: &[PathBuf], limits: InputLimits) -> io::Result<Self> {
        let mut command = Command::new(std::env::current_exe()?);
        command
            .arg("--worker")
            .arg("--max-input-mib")
            .arg((limits.max_input_bytes / (1024 * 1024)).to_string())
            .arg("--max-dex-mib")
            .arg((limits.max_dex_bytes / (1024 * 1024)).to_string());
        for root in roots {
            command.arg("--allow-root").arg(root);
        }
        let mut child = command
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .stderr(Stdio::inherit())
            .spawn()?;
        let mut input = child.stdin.take().unwrap();
        let mut output = BufReader::new(child.stdout.take().unwrap());
        let child = Arc::new(Mutex::new(Some(child)));
        let (sender, receiver) = mpsc::sync_channel::<Job>(8);
        let owner = child.clone();
        let thread = std::thread::Builder::new()
            .name("dexkit-native-worker".into())
            .spawn(move || {
                while let Ok(job) = receiver.recv() {
                    // A queued cancelled request has no native side effects.
                    if job.reply.is_closed() {
                        continue;
                    }
                    let result = (|| -> io::Result<Value> {
                        serde_json::to_writer(&mut input, &job.request)?;
                        input.write_all(b"\n")?;
                        input.flush()?;
                        let line = read_frame(&mut output)?.ok_or_else(|| {
                            io::Error::new(io::ErrorKind::UnexpectedEof, "worker closed")
                        })?;
                        Ok(serde_json::from_slice(&line)?)
                    })();
                    let broken = result.is_err();
                    if let Err(error) = &result {
                        eprintln!("Native worker failed: {error}");
                    }
                    let _ = job.reply.send(result.map_err(|_| died()));
                    if broken {
                        shutdown_child(&owner);
                        break;
                    }
                }
                shutdown_child(&owner);
            });
        if let Err(error) = thread {
            shutdown_child(&child);
            return Err(error);
        }
        Ok(Self { sender, child })
    }
    pub async fn request(&self, request: Request) -> Result<Value> {
        let (reply, receive) = oneshot::channel();
        self.sender
            .try_send(Job { request, reply })
            .map_err(|e| match e {
                mpsc::TrySendError::Full(_) => {
                    let mut error =
                        Error::new("resource_limit", "BUSY", "Native request queue is full");
                    error.retryable = true;
                    error
                }
                mpsc::TrySendError::Disconnected(_) => died(),
            })?;
        receive.await.map_err(|_| died())?
    }
    pub fn shutdown(&self) {
        shutdown_child(&self.child);
    }
}
impl Drop for Worker {
    fn drop(&mut self) {
        self.shutdown();
    }
}
fn shutdown_child(owner: &Mutex<Option<Child>>) {
    if let Ok(mut guard) = owner.lock() {
        if let Some(mut child) = guard.take() {
            let _ = child.kill();
            let _ = child.wait();
        }
    }
}
pub fn run(
    roots: Vec<PathBuf>,
    limits: InputLimits,
) -> std::result::Result<(), Box<dyn std::error::Error>> {
    // Do this before any native work or threads. FD 1 belongs permanently to
    // diagnostics; only the duplicated descriptor carries worker responses.
    let mut protocol = crate::isolate_stdout()?;
    let mut service = AnalysisService::with_input_limits(roots, limits)?;
    let mut input = BufReader::new(io::stdin().lock());
    while let Some(line) = read_frame(&mut input)? {
        let request: Request = serde_json::from_slice(&line)?;
        let response = match request {
            Request::Call { name, arguments } => service.call(&name, arguments),
            Request::Resource { uri } => match service.resource(&uri) {
                Ok(text) => json!({"ok":true,"data":{"text":text}}),
                Err(error) => json!({"ok":false,"error":error}),
            },
        };
        serde_json::to_writer(&mut protocol, &response)?;
        protocol.write_all(b"\n")?;
        protocol.flush()?;
    }
    Ok(())
}
fn read_frame(input: &mut impl BufRead) -> io::Result<Option<Vec<u8>>> {
    let mut line = Vec::new();
    input
        .take((FRAME_LIMIT + 1) as u64)
        .read_until(b'\n', &mut line)?;
    if line.is_empty() {
        return Ok(None);
    }
    if line.len() > FRAME_LIMIT || line.last() != Some(&b'\n') {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Worker frame too long or incomplete",
        ));
    }
    Ok(Some(line))
}
