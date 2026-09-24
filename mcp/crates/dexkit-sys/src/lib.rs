//! Private, synchronous static ABI. All functions require exclusive access to
//! a live context. The safe adapter owns contexts and validates query semantics.
use std::ffi::{c_char, c_void};

#[repr(C)]
#[derive(Default)]
pub struct Buffer {
    pub data: *mut u8,
    pub size: usize,
}
#[repr(C)]
#[derive(Default, Clone, Copy, Debug)]
pub struct SmaliStatus {
    pub dex_offset: u64,
    pub code_offset: u32,
    pub detail: u32,
    pub dex_id: u32,
    pub member_id: u32,
    pub error: u8,
    pub phase: u8,
    pub member_kind: u8,
}
unsafe extern "C" {
    pub fn dk_default_thread_count() -> u32;
    pub fn dk_open(
        path: *const c_char,
        max_dex_bytes: u64,
        threads: u32,
        context: *mut *mut c_void,
        dex_count: *mut u32,
    ) -> i32;
    pub fn dk_close(context: *mut c_void);
    pub fn dk_free(buffer: Buffer);
    pub fn dk_call(
        context: *mut c_void,
        operation: u32,
        id: u64,
        query: *const u8,
        size: usize,
        output: *mut Buffer,
    ) -> i32;
    pub fn dk_smali(
        context: *mut c_void,
        id: u64,
        is_method: u8,
        strict: u8,
        max_output: u32,
        output: *mut Buffer,
        status: *mut SmaliStatus,
    ) -> i32;
}
