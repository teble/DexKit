//! Open beneath an already-held allowed directory without following replaced
//! path components. O_NONBLOCK lets us reject FIFOs before any blocking read.
use crate::error::{Error, Result};
use std::{
    ffi::CString,
    fs::{File, Metadata, OpenOptions},
    os::{
        fd::{AsRawFd, FromRawFd},
        unix::{ffi::OsStrExt, fs::MetadataExt, fs::OpenOptionsExt},
    },
    path::{Component, Path, PathBuf},
};

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
    pub file: File,
    pub byte_length: u64,
    metadata: Metadata,
}
impl Input {
    pub fn prepare(file: File, limit: u64) -> Result<Self> {
        let before = file.metadata().map_err(io)?;
        if !before.is_file() {
            return Err(Error::invalid("Input must be a regular DEX or APK file"));
        }
        let len = before.len();
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
        // Native maps this same held descriptor; the path is never reopened.
        // Keep the initial metadata for the check after native loading.
        Ok(Self {
            file,
            byte_length: len,
            metadata: before,
        })
    }
    pub fn verify_unchanged(&self) -> Result<()> {
        let after = self.file.metadata().map_err(io)?;
        if self.metadata.len() != after.len()
            || self.metadata.mtime() != after.mtime()
            || self.metadata.mtime_nsec() != after.mtime_nsec()
            || self.metadata.ctime() != after.ctime()
            || self.metadata.ctime_nsec() != after.ctime_nsec()
        {
            return Err(changed());
        }
        Ok(())
    }
}
fn changed() -> Error {
    let mut error = Error::new(
        "io",
        "INPUT_CHANGED",
        "Input changed while opening; retry after its writer finishes",
    );
    error.retryable = true;
    error
}

pub(crate) struct Root {
    path: PathBuf,
    directory: File,
}
fn io(error: std::io::Error) -> Error {
    Error::new("io", "IO_ERROR", error.to_string())
}
impl Root {
    pub fn new(path: PathBuf) -> Result<Self> {
        let path = path.canonicalize().map_err(io)?;
        let directory = OpenOptions::new()
            .read(true)
            .custom_flags(libc::O_DIRECTORY | libc::O_NOFOLLOW | libc::O_CLOEXEC)
            .open(&path)
            .map_err(io)?;
        Ok(Self { path, directory })
    }
    pub fn open(roots: &[Self], path: &str) -> Result<File> {
        // Normal symlinks are resolved first. Traversal after the allow check
        // is anchored to held directory handles and rejects newly introduced
        // symlinks at every step; no check-then-open path is used for the file.
        let canonical = Path::new(path).canonicalize().map_err(io)?;
        Self::open_canonical(roots, &canonical)
    }
    fn open_canonical(roots: &[Self], canonical: &Path) -> Result<File> {
        let root = roots
            .iter()
            .filter(|root| canonical.starts_with(&root.path))
            .max_by_key(|root| root.path.components().count())
            .ok_or_else(|| {
                Error::new(
                    "invalid_request",
                    "PATH_NOT_ALLOWED",
                    "Input is outside configured --allow-root directories",
                )
            })?;
        let relative = canonical
            .strip_prefix(&root.path)
            .map_err(|_| Error::invalid("Invalid input path"))?;
        let parts: Vec<_> = relative.components().collect();
        if parts.is_empty() {
            return Err(Error::invalid("Input must be a regular DEX or APK file"));
        }
        let mut parent = root.directory.try_clone().map_err(io)?;
        for (index, part) in parts.iter().enumerate() {
            let Component::Normal(name) = part else {
                return Err(Error::invalid("Invalid path component"));
            };
            let name =
                CString::new(name.as_bytes()).map_err(|_| Error::invalid("NUL in input path"))?;
            let last = index + 1 == parts.len();
            let flags = libc::O_RDONLY
                | libc::O_CLOEXEC
                | libc::O_NOFOLLOW
                | libc::O_NONBLOCK
                | if last { 0 } else { libc::O_DIRECTORY };
            // SAFETY: held live directory FD and NUL-terminated one-component
            // name. On success the returned FD is exclusively owned by File.
            let fd = unsafe { libc::openat(parent.as_raw_fd(), name.as_ptr(), flags) };
            if fd < 0 {
                return Err(io(std::io::Error::last_os_error()));
            }
            parent = unsafe { File::from_raw_fd(fd) };
        }
        if !parent.metadata().map_err(io)?.is_file() {
            return Err(Error::invalid("Input must be a regular DEX or APK file"));
        }
        Ok(parent)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{io::Read, os::unix::fs::symlink};

    #[test]
    fn input_budget_and_change_detection() {
        let directory = tempfile::tempdir().unwrap();
        let path = directory.path().join("input");
        let original = vec![b'x'; 65537];
        std::fs::write(&path, &original).unwrap();
        let rejected = Input::prepare(File::open(&path).unwrap(), 65536);
        assert!(matches!(rejected, Err(e) if e.code == "LIMIT_EXCEEDED"));
        for limit in [65537, 0] {
            let input = Input::prepare(File::open(&path).unwrap(), limit).unwrap();
            assert_eq!(input.byte_length, 65537);
            input.verify_unchanged().unwrap();
            std::fs::write(&path, b"changed").unwrap();
            assert_eq!(input.verify_unchanged().unwrap_err().code, "INPUT_CHANGED");
            assert!(
                matches!(crate::native::Native::open(&input, 0, 0), Err(e) if e.code == "INPUT_CHANGED")
            );
            std::fs::write(&path, []).unwrap();
            assert!(
                matches!(crate::native::Native::open(&input, 0, 0), Err(e) if e.code == "INPUT_CHANGED")
            );
            std::fs::write(&path, &original).unwrap();
        }
        std::fs::write(&path, []).unwrap();
        assert!(
            matches!(Input::prepare(File::open(&path).unwrap(), 0), Err(e) if e.code == "INVALID_INPUT")
        );
    }

    #[test]
    fn allowed_links_and_fifo() {
        let directory = tempfile::tempdir().unwrap();
        let outside = tempfile::tempdir().unwrap();
        std::fs::write(directory.path().join("file"), "inside").unwrap();
        std::fs::write(outside.path().join("file"), "outside").unwrap();
        symlink("file", directory.path().join("link")).unwrap();
        symlink(outside.path().join("file"), directory.path().join("escape")).unwrap();
        let roots = [Root::new(directory.path().into()).unwrap()];
        let mut text = String::new();
        Root::open(&roots, directory.path().join("link").to_str().unwrap())
            .unwrap()
            .read_to_string(&mut text)
            .unwrap();
        assert_eq!(text, "inside");
        assert_eq!(
            Root::open(&roots, directory.path().join("escape").to_str().unwrap())
                .unwrap_err()
                .code,
            "PATH_NOT_ALLOWED"
        );
        let fifo = directory.path().join("fifo");
        let name = CString::new(fifo.as_os_str().as_bytes()).unwrap();
        assert_eq!(unsafe { libc::mkfifo(name.as_ptr(), 0o600) }, 0);
        assert!(Root::open(&roots, fifo.to_str().unwrap()).is_err());
    }

    #[test]
    fn replaced_component_cannot_escape_held_root() {
        let directory = tempfile::tempdir().unwrap();
        let outside = tempfile::tempdir().unwrap();
        let sub = directory.path().join("sub");
        std::fs::create_dir(&sub).unwrap();
        std::fs::write(sub.join("file"), "inside").unwrap();
        std::fs::write(outside.path().join("file"), "outside").unwrap();
        let roots = [Root::new(directory.path().into()).unwrap()];
        let checked_path = sub.join("file").canonicalize().unwrap();
        std::fs::rename(&sub, directory.path().join("old")).unwrap();
        symlink(outside.path(), &sub).unwrap();
        assert!(Root::open_canonical(&roots, &checked_path).is_err());
    }
}
