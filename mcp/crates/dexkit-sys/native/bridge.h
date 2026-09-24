// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Private static ABI. Calls are synchronous and externally serialized. Inputs
// are borrowed for one call, and buffers use dk_free. Open borrows a regular
// input FD for mmap; the input must not be modified during the call. All loaded
// DEX bytes are independently owned on success; no input mapping escapes.
typedef struct DkBuffer { uint8_t *data; size_t size; } DkBuffer;
typedef struct DkStatus {
    uint64_t dex_offset;
    uint32_t code_offset, detail, dex_id, member_id;
    uint8_t error, phase, member_kind;
} DkStatus;
// max_dex_bytes == 0 disables the DEX byte ceiling. Status 7: DEX byte limit;
// status 8: input mmap failed; status 9: input size changed. Other status codes
// match dk_call. expected_size is checked against the held descriptor before mmap.
// Zero threads selects Core's automatic default; positive values override it.
uint32_t dk_default_thread_count(void);
int dk_open(int input_fd, uint64_t expected_size, uint64_t max_dex_bytes, uint32_t threads,
    void **context, uint32_t *dex_count);
void dk_close(void *context);
void dk_free(DkBuffer buffer);
// Internal operation enum is mirrored in Rust. Only the typed adapter builds
// queries: FBS structural verification alone is not full semantic validation.
int dk_call(void *context, uint32_t operation, uint64_t id,
    const uint8_t *query, size_t size, DkBuffer *output);
int dk_smali(void *context, uint64_t id, uint8_t is_method, uint8_t strict,
    uint32_t max_output, DkBuffer *output, DkStatus *status);
#ifdef __cplusplus
}
#endif
