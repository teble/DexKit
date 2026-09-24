use crate::{
    encoding::{decode, WireStr},
    error::{Error, NativeError, Result},
    generated::dexkit::schema as fb,
    input::Input,
};
use dexkit_sys as sys;
use planus::{ReadAsRoot, SliceWithStartOffset, TableRead};
use std::{ffi::c_void, os::fd::AsRawFd, ptr::NonNull};

pub(crate) struct Native {
    pointer: NonNull<c_void>,
    pub dex_count: u32,
}
// No Send/Sync implementations. The service creates and uses this owner on
// its dedicated native execution thread/process; no borrowed result escapes.
#[derive(Default)]
struct Buffer(sys::Buffer);
impl Drop for Buffer {
    fn drop(&mut self) {
        // SAFETY: this is exactly one buffer returned by this static ABI.
        unsafe {
            sys::dk_free(sys::Buffer {
                data: self.0.data,
                size: self.0.size,
            })
        }
    }
}
impl Buffer {
    fn copy(&self) -> Result<Vec<u8>> {
        if self.0.size == 0 {
            return Ok(Vec::new());
        }
        if self.0.data.is_null() || self.0.size > 64 * 1024 * 1024 {
            return Err(Error::native(4));
        }
        // SAFETY: successful ABI calls initialize size bytes and retain the
        // allocation until Buffer::drop. Return a Rust-owned copy.
        Ok(unsafe { std::slice::from_raw_parts(self.0.data, self.0.size) }.to_vec())
    }
}
impl Native {
    pub fn open(input: &Input, max_dex_bytes: u64) -> Result<Self> {
        let mut pointer = std::ptr::null_mut();
        let mut dex_count = 0;
        // SAFETY: the FD is a held regular file, borrowed only for this call.
        // Native owns all loaded DEX bytes before returning; no input mapping
        // escapes. As with Core's file loader, concurrent source mutation is
        // unsupported (detected changes are checked before publishing handles).
        let status = unsafe {
            sys::dk_open(
                input.file.as_raw_fd(),
                input.byte_length,
                max_dex_bytes,
                &mut pointer,
                &mut dex_count,
            )
        };
        if status == 7 {
            return Err(Error::limit(format!(
                "DEX bytes exceed configured limit of {max_dex_bytes} bytes. Adjust --max-dex-mib (0 disables it)."
            )));
        }
        if status == 8 {
            return Err(Error::new(
                "io",
                "INPUT_MAP_FAILED",
                "Could not map the input file",
            ));
        }
        if status == 9 {
            let mut error = Error::new(
                "io",
                "INPUT_CHANGED",
                "Input changed while opening; retry after its writer finishes",
            );
            error.retryable = true;
            return Err(error);
        }
        if status != 0 {
            return Err(Error::native(status));
        }
        Ok(Self {
            pointer: NonNull::new(pointer).ok_or_else(|| Error::native(4))?,
            dex_count,
        })
    }
    pub fn call(&mut self, operation: u32, id: u64, query: &[u8]) -> Result<Vec<u8>> {
        if query.len() > 1024 * 1024 {
            return Err(Error::limit("Encoded query exceeds 1 MiB"));
        }
        let mut buffer = Buffer::default();
        // SAFETY: exclusive live context, borrowed query, paired buffer owner.
        let status = unsafe {
            sys::dk_call(
                self.pointer.as_ptr(),
                operation,
                id,
                query.as_ptr(),
                query.len(),
                &mut buffer.0,
            )
        };
        if status != 0 {
            return Err(Error::native(status));
        }
        buffer.copy()
    }
    pub fn classes(&mut self, query: fb::FindClass) -> Result<Vec<fb::ClassMeta>> {
        let bytes = self.call(1, 0, &encode(&query))?;
        let rows: fb::ClassMetaArrayHolder =
            fb::ClassMetaArrayHolderRef::read_as_root(&bytes)?.try_into()?;
        Ok(rows.classes.unwrap_or_default())
    }
    pub fn methods(&mut self, query: fb::FindMethod) -> Result<Vec<fb::MethodMeta>> {
        let bytes = self.call(2, 0, &encode(&query))?;
        Self::decode_methods(&bytes)
    }
    pub fn fields(&mut self, query: fb::FindField) -> Result<Vec<fb::FieldMeta>> {
        let bytes = self.call(3, 0, &encode(&query))?;
        let rows: fb::FieldMetaArrayHolder =
            fb::FieldMetaArrayHolderRef::read_as_root(&bytes)?.try_into()?;
        Ok(rows.fields.unwrap_or_default())
    }
    pub fn decode_methods(bytes: &[u8]) -> Result<Vec<fb::MethodMeta>> {
        let rows: fb::MethodMetaArrayHolder =
            fb::MethodMetaArrayHolderRef::read_as_root(bytes)?.try_into()?;
        Ok(rows.methods.unwrap_or_default())
    }
    pub fn annotations(&mut self, op: u32, id: u64) -> Result<Vec<fb::AnnotationMeta>> {
        let bytes = self.call(op, id, &[])?;
        let rows: fb::AnnotationMetaArrayHolder =
            fb::AnnotationMetaArrayHolderRef::read_as_root(&bytes)?.try_into()?;
        Ok(rows.annotations.unwrap_or_default())
    }
    pub fn strings(&mut self, id: u64) -> Result<Vec<String>> {
        let bytes = self.call(12, id, &[])?;
        let values: planus::Vector<planus::Result<&WireStr>> = TableRead::from_buffer(
            SliceWithStartOffset {
                buffer: &bytes,
                offset_from_start: 0,
            },
            0,
        )
        .map_err(|e| Error::new("native_failure", "INVALID_NATIVE_RESULT", format!("{e}")))?;
        values.iter().map(|s| Ok(decode(s?.as_bytes())?)).collect()
    }
    pub fn numbers(&mut self, id: u64) -> Result<Vec<fb::UsingNumberMeta>> {
        let bytes = self.call(7, id, &[])?;
        let rows: fb::UsingNumberMetaArrayHolder =
            fb::UsingNumberMetaArrayHolderRef::read_as_root(&bytes)?.try_into()?;
        Ok(rows.items.unwrap_or_default())
    }
    pub fn smali(
        &mut self,
        id: u64,
        method: bool,
        strict: bool,
        max_output: u32,
    ) -> Result<String> {
        let mut out = Buffer::default();
        let mut detail = sys::SmaliStatus::default();
        // SAFETY: exclusive live context; ABI checks entity ownership and
        // options; result and diagnostic have initialized storage.
        let status = unsafe {
            sys::dk_smali(
                self.pointer.as_ptr(),
                id,
                method.into(),
                strict.into(),
                max_output,
                &mut out.0,
                &mut detail,
            )
        };
        if status == 6 {
            let mut error = Error::new(
                "native_failure",
                "SMALI_ERROR",
                "Native smali generation failed",
            );
            error.native = Some(Box::new(NativeError {
                error: detail.error,
                phase: detail.phase,
                detail: detail.detail,
                dex_id: (detail.dex_id != u32::MAX).then_some(detail.dex_id),
                member_id: (detail.member_id != u32::MAX).then_some(detail.member_id),
                member_kind: detail.member_kind,
                container_byte_offset: (detail.dex_offset != u64::MAX)
                    .then(|| detail.dex_offset.to_string()),
                code_unit_offset: (detail.code_offset != u32::MAX).then_some(detail.code_offset),
            }));
            return Err(error);
        }
        if status != 0 {
            return Err(Error::native(status));
        }
        String::from_utf8(out.copy()?).map_err(Error::encoding)
    }
}
impl Drop for Native {
    fn drop(&mut self) {
        // SAFETY: unique owner, no active calls or outstanding native borrows.
        unsafe { sys::dk_close(self.pointer.as_ptr()) }
    }
}
fn encode<T>(value: impl planus::WriteAsOffset<T>) -> Vec<u8> {
    planus::Builder::new().finish(value, None).to_vec()
}
