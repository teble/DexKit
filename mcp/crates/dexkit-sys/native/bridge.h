// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Private static ABI. Calls are synchronous and externally serialized. Inputs
// are borrowed for one call, open copies all DEX bytes, and buffers use dk_free.
typedef struct DkBuffer { uint8_t *data; size_t size; } DkBuffer;
typedef struct DkStatus {
    uint64_t dex_offset;
    uint32_t code_offset, detail, dex_id, member_id;
    uint8_t error, phase, member_kind;
} DkStatus;
int dk_open(const uint8_t *input, size_t size, void **context, uint32_t *dex_count);
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
