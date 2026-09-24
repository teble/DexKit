//! Canonical path policy for a trusted local server. The caller must keep input
//! files and their paths unchanged until the corresponding instance is closed.
use crate::error::{Error, Result};
use std::path::{Path, PathBuf};

/// Startup resource policy. Zero disables the corresponding byte ceiling.
/// DEX bytes do not include Core indexes or query results.
#[derive(Clone, Copy, Debug)]
pub struct InputLimits {
    pub max_input_bytes: u64,
    pub max_dex_bytes: u64,
}
impl Default for InputLimits {
    fn default() -> Self {
        Self {
            max_input_bytes: 0,
            max_dex_bytes: 512 * 1024 * 1024,
        }
    }
}

pub(crate) struct Input {
    pub path: PathBuf,
    pub byte_length: u64,
}
impl Input {
    pub fn prepare(path: PathBuf, limit: u64) -> Result<Self> {
        let metadata = path.metadata().map_err(io)?;
        if !metadata.is_file() {
            return Err(Error::invalid("Input must be a regular DEX or APK file"));
        }
        let len = metadata.len();
        if limit != 0 && len > limit {
            return Err(Error::limit(format!(
                "Input is {len} bytes; configured limit is {limit} bytes. Adjust --max-input-mib (0 disables it)."
            )));
        }
        if len == 0 || len > isize::MAX as u64 {
            return Err(Error::new(
                "invalid_input",
                "INVALID_INPUT",
                "Empty input or input exceeds the platform mapping range",
            ));
        }
        Ok(Self {
            path,
            byte_length: len,
        })
    }
}

pub(crate) struct Root {
    path: PathBuf,
}
fn io(error: std::io::Error) -> Error {
    Error::new("io", "IO_ERROR", error.to_string())
}
impl Root {
    pub fn new(path: PathBuf) -> Result<Self> {
        let path = path.canonicalize().map_err(io)?;
        if !path.is_dir() {
            return Err(Error::invalid("Input root must be a directory"));
        }
        Ok(Self { path })
    }
    pub fn resolve(roots: &[Self], path: &str) -> Result<PathBuf> {
        let canonical = Path::new(path).canonicalize().map_err(io)?;
        if !roots.iter().any(|root| canonical.starts_with(&root.path)) {
            return Err(Error::new(
                "invalid_request",
                "PATH_NOT_ALLOWED",
                "Input is outside configured --allow-root directories",
            ));
        }
        Ok(canonical)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn input_budget_and_regular_file_validation() {
        let directory = tempfile::tempdir().unwrap();
        let path = directory.path().join("input");
        std::fs::write(&path, vec![b'x'; 65537]).unwrap();
        let rejected = Input::prepare(path.clone(), 65536);
        assert!(matches!(rejected, Err(e) if e.code == "LIMIT_EXCEEDED"));
        for limit in [65537, 0] {
            let input = Input::prepare(path.clone(), limit).unwrap();
            assert_eq!(input.byte_length, 65537);
            assert_eq!(input.path, path);
        }
        assert!(Input::prepare(directory.path().into(), 0).is_err());
        std::fs::write(&path, []).unwrap();
        assert!(matches!(Input::prepare(path, 0), Err(e) if e.code == "INVALID_INPUT"));
    }

    #[cfg(unix)]
    #[test]
    fn allowed_links_and_fifo() {
        use std::{
            ffi::CString,
            os::unix::{ffi::OsStrExt, fs::symlink},
        };
        let directory = tempfile::tempdir().unwrap();
        let outside = tempfile::tempdir().unwrap();
        std::fs::write(directory.path().join("file"), "inside").unwrap();
        std::fs::write(outside.path().join("file"), "outside").unwrap();
        symlink("file", directory.path().join("link")).unwrap();
        symlink(outside.path().join("file"), directory.path().join("escape")).unwrap();
        let roots = [Root::new(directory.path().into()).unwrap()];
        let resolved =
            Root::resolve(&roots, directory.path().join("link").to_str().unwrap()).unwrap();
        assert_eq!(
            resolved,
            directory.path().join("file").canonicalize().unwrap()
        );
        assert_eq!(
            Root::resolve(&roots, directory.path().join("escape").to_str().unwrap())
                .unwrap_err()
                .code,
            "PATH_NOT_ALLOWED"
        );
        let fifo = directory.path().join("fifo");
        let name = CString::new(fifo.as_os_str().as_bytes()).unwrap();
        assert_eq!(unsafe { libc::mkfifo(name.as_ptr(), 0o600) }, 0);
        assert!(Input::prepare(Root::resolve(&roots, fifo.to_str().unwrap()).unwrap(), 0).is_err());
    }
}
