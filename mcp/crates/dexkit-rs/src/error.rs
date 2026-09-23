use schemars::JsonSchema;
use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema, thiserror::Error)]
#[error("{code}: {message}")]
#[serde(rename_all = "camelCase")]
pub struct Error {
    pub category: String,
    pub code: String,
    pub message: String,
    pub retryable: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub native: Option<Box<NativeError>>,
}
#[derive(Clone, Debug, Serialize, Deserialize, JsonSchema)]
#[serde(rename_all = "camelCase")]
pub struct NativeError {
    pub error: u8,
    pub phase: u8,
    pub detail: u32,
    pub dex_id: Option<u32>,
    pub member_id: Option<u32>,
    pub member_kind: u8,
    pub container_byte_offset: Option<String>,
    pub code_unit_offset: Option<u32>,
}
impl Error {
    pub fn new(category: &str, code: &str, message: impl Into<String>) -> Self {
        let mut message = message.into();
        if message.len() > 4096 {
            let mut end = 4096;
            while !message.is_char_boundary(end) {
                end -= 1;
            }
            message.truncate(end);
            message.push_str("...");
        }
        Self {
            category: category.into(),
            code: code.into(),
            message,
            retryable: false,
            native: None,
        }
    }
    pub fn invalid(message: impl Into<String>) -> Self {
        Self::new("invalid_request", "INVALID_ARGUMENT", message)
    }
    pub fn limit(message: impl Into<String>) -> Self {
        Self::new("resource_limit", "LIMIT_EXCEEDED", message)
    }
    pub fn encoding(error: impl std::fmt::Display) -> Self {
        Self::new("unsupported", "STRING_ENCODING", error.to_string())
    }
    pub(crate) fn native(status: i32) -> Self {
        match status {
            1 => Self::new(
                "invalid_input",
                "INVALID_INPUT",
                "Invalid or unsupported DEX/APK input",
            ),
            2 => Self::new(
                "invalid_request",
                "INVALID_NATIVE_QUERY",
                "Native query verification failed",
            ),
            3 => Self::new(
                "invalid_request",
                "INVALID_ENTITY",
                "Unknown native entity or operation",
            ),
            5 => Self::limit("Native input or result budget exceeded"),
            _ => Self::new(
                "native_failure",
                "NATIVE_FAILURE",
                "Core could not complete the operation",
            ),
        }
    }
}
impl From<planus::Error> for Error {
    fn from(error: planus::Error) -> Self {
        Self::new("native_failure", "INVALID_NATIVE_RESULT", error.to_string())
    }
}
impl From<crate::encoding::EncodingError> for Error {
    fn from(error: crate::encoding::EncodingError) -> Self {
        Self::encoding(error)
    }
}
pub type Result<T> = std::result::Result<T, Error>;
