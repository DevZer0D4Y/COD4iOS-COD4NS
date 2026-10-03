#pragma once

#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Runs a digest known-answer check and an upstream Huffman round trip.
// Returns 0 on success, bit 0 for a digest failure, bit 1 for a Huffman failure.
// Suitable for the native app's startup diagnostics; does not start the engine.
int KisakCore_RunSelfTests(void);

// Legacy game-content checksum, not a cryptographic security API.
// A null input is valid only when length == 0. Output must have 16 bytes.
bool KisakCore_MD4(const uint8_t *input, size_t length, uint8_t output[16]);

#ifdef __cplusplus
}
#endif
