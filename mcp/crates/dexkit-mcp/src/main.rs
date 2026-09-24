mod catalog;
mod discovery;
mod http;
mod server;
mod worker;

use dexkit_rs::InputLimits;
use rmcp::ServiceExt;
use std::{
    fs::File,
    io,
    net::SocketAddr,
    os::fd::FromRawFd,
    path::PathBuf,
    pin::Pin,
    sync::Arc,
    task::{Context, Poll},
};
use tokio::io::{AsyncRead, ReadBuf};

fn main() {
    if let Err(error) = run() {
        eprintln!("dexkit-mcp: {error}");
        std::process::exit(1);
    }
}
fn run() -> Result<(), Box<dyn std::error::Error>> {
    let mut roots = Vec::new();
    let mut input_limits = InputLimits::default();
    let mut worker_mode = false;
    let mut http_mode = false;
    let mut listen: Option<SocketAddr> = None;
    let mut args = std::env::args_os().skip(1);
    while let Some(arg) = args.next() {
        match arg.to_str() {
            Some("--allow-root") => roots.push(PathBuf::from(
                args.next().ok_or("--allow-root requires a directory")?,
            )),
            Some("--worker") => worker_mode = true,
            Some("--max-input-mib") => {
                input_limits.max_input_bytes = parse_mib(args.next(), "--max-input-mib")?;
            }
            Some("--max-dex-mib") => {
                input_limits.max_dex_bytes = parse_mib(args.next(), "--max-dex-mib")?;
            }
            Some("--transport") => {
                http_mode = match args.next().as_deref().and_then(|s| s.to_str()) {
                    Some("http") => true,
                    Some("stdio") => false,
                    _ => return Err("--transport requires http or stdio".into()),
                };
            }
            Some("--listen") => {
                listen = Some(
                    args.next()
                        .ok_or("--listen requires IP:PORT")?
                        .to_str()
                        .ok_or("--listen requires an IP address")?
                        .parse()?,
                );
            }
            Some("--dump-schema") => {
                println!("{}", serde_json::to_string_pretty(&catalog::tools())?);
                return Ok(());
            }
            Some("--version") => {
                println!("dexkit-mcp {}", env!("CARGO_PKG_VERSION"));
                return Ok(());
            }
            Some("--help") => {
                println!("dexkit-mcp [--transport stdio|http] [--allow-root DIRECTORY]...\n\n--transport http  Serve Streamable HTTP at /mcp.\n--listen IP:PORT  HTTP loopback address (default 127.0.0.1:7331; port 0 selects a free port).\n--transport stdio Standard MCP pipes (default, for existing client configurations).\n--allow-root DIR  Allowed input directory; repeatable. Default: current directory.\n--max-input-mib N Input file limit in MiB (default 0: unlimited).\n--max-dex-mib N   Raw/total uncompressed DEX limit per input in MiB (default 512; 0: unlimited).\n--dump-schema     Print tool contracts.\n--version         Print version.");
                return Ok(());
            }
            _ => return Err(format!("Unknown argument: {arg:?}").into()),
        }
    }
    if listen.is_some() && !http_mode {
        return Err("--listen requires --transport http".into());
    }
    if worker_mode && http_mode {
        return Err("Internal worker cannot serve HTTP".into());
    }
    let listen = listen.unwrap_or_else(|| SocketAddr::from(([127, 0, 0, 1], 7331)));
    if http_mode && !listen.ip().is_loopback() {
        return Err("HTTP currently accepts only a loopback --listen address".into());
    }
    if roots.is_empty() {
        roots.push(std::env::current_dir()?);
    }
    roots = roots
        .into_iter()
        .map(|r| r.canonicalize())
        .collect::<io::Result<Vec<_>>>()?;
    if roots.iter().any(|r| !r.is_dir()) {
        return Err("--allow-root must be a directory".into());
    }
    if worker_mode {
        return worker::run(roots, input_limits);
    }
    let protocol = if http_mode {
        None
    } else {
        Some(isolate_stdout()?)
    };
    let worker = Arc::new(worker::Worker::start(&roots, input_limits)?);
    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()?;
    let result = runtime.block_on(async {
        if http_mode {
            return http::run(worker.clone(), listen).await;
        }
        let input = BoundedInput {
            inner: tokio::io::stdin(),
            line_bytes: 0,
        };
        let output = tokio::fs::File::from_std(protocol.expect("stdio protocol FD"));
        let service = server::Server::new(worker.clone())
            .serve((input, output))
            .await?;
        service.waiting().await?;
        Ok::<(), Box<dyn std::error::Error>>(())
    });
    worker.shutdown();
    result
}

fn parse_mib(
    value: Option<std::ffi::OsString>,
    flag: &str,
) -> Result<u64, Box<dyn std::error::Error>> {
    // A u32 MiB count also keeps capability integers exact in JSON/JavaScript.
    let mib = value
        .as_deref()
        .and_then(|s| s.to_str())
        .filter(|s| !s.is_empty() && s.bytes().all(|c| c.is_ascii_digit()))
        .and_then(|s| s.parse::<u32>().ok())
        .ok_or_else(|| format!("{flag} requires an integer from 0 to {} (MiB)", u32::MAX))?;
    Ok(u64::from(mib) * 1024 * 1024)
}

pub(crate) fn isolate_stdout() -> io::Result<File> {
    // SAFETY: called once at process startup before threads/native execution.
    // Own the duplicated descriptor; permanently route FD 1 to stderr. Never
    // redirect around individual calls or bypass Rust UTF-8 validity.
    unsafe {
        let protocol = libc::fcntl(libc::STDOUT_FILENO, libc::F_DUPFD_CLOEXEC, 3);
        if protocol < 0 {
            return Err(io::Error::last_os_error());
        }
        let file = File::from_raw_fd(protocol);
        if libc::dup2(libc::STDERR_FILENO, libc::STDOUT_FILENO) < 0 {
            return Err(io::Error::last_os_error());
        }
        Ok(file)
    }
}
struct BoundedInput<R> {
    inner: R,
    line_bytes: usize,
}
impl<R: AsyncRead + Unpin> AsyncRead for BoundedInput<R> {
    fn poll_read(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
        buf: &mut ReadBuf<'_>,
    ) -> Poll<io::Result<()>> {
        let this = self.get_mut();
        let before = buf.filled().len();
        let result = Pin::new(&mut this.inner).poll_read(cx, buf);
        if let Poll::Ready(Ok(())) = result {
            for byte in &buf.filled()[before..] {
                if *byte == b'\n' {
                    this.line_bytes = 0;
                } else {
                    this.line_bytes += 1;
                }
                if this.line_bytes > 2 * 1024 * 1024 {
                    // AsyncRead must not expose new bytes together with Err.
                    // Tokio's read_to_end enforces this contract as well.
                    buf.set_filled(before);
                    return Poll::Ready(Err(io::Error::new(
                        io::ErrorKind::InvalidData,
                        "MCP frame exceeds 2 MiB",
                    )));
                }
            }
        }
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use tokio::io::AsyncReadExt;

    #[test]
    fn stdio_isolation_child() {
        if std::env::var_os("DEXKIT_TEST_STDOUT_CHILD").is_none() {
            return;
        }
        let mut protocol = isolate_stdout().unwrap();
        unsafe {
            libc::puts(c"native stdout diagnostic".as_ptr());
            libc::fflush(std::ptr::null_mut());
        }
        protocol.write_all(b"{\"protocol\":true}\n").unwrap();
        protocol.flush().unwrap();
        std::process::exit(0);
    }

    #[test]
    fn native_stdout_cannot_contaminate_protocol_fd() {
        let result = std::process::Command::new(std::env::current_exe().unwrap())
            .args(["--exact", "tests::stdio_isolation_child", "--nocapture"])
            .env("DEXKIT_TEST_STDOUT_CHILD", "1")
            .output()
            .unwrap();
        assert!(result.status.success());
        let stdout = String::from_utf8(result.stdout).unwrap();
        let stderr = String::from_utf8(result.stderr).unwrap();
        assert!(stdout.contains("{\"protocol\":true}"));
        assert!(!stdout.contains("native stdout diagnostic"));
        assert!(stderr.contains("native stdout diagnostic"));
    }

    #[test]
    fn frame_budget_is_per_line_and_rejects_unterminated_oversize() {
        tokio::runtime::Builder::new_current_thread()
            .build()
            .unwrap()
            .block_on(async {
                let bytes = vec![b'x'; 2 * 1024 * 1024 + 1];
                let mut input = BoundedInput {
                    inner: bytes.as_slice(),
                    line_bytes: 0,
                };
                assert!(input.read_to_end(&mut Vec::new()).await.is_err());
                let mut bytes = vec![b'x'; 1_500_000];
                bytes.push(b'\n');
                let bytes = bytes.repeat(2);
                let mut input = BoundedInput {
                    inner: bytes.as_slice(),
                    line_bytes: 0,
                };
                assert_eq!(
                    input.read_to_end(&mut Vec::new()).await.unwrap(),
                    bytes.len()
                );
            });
    }
}
