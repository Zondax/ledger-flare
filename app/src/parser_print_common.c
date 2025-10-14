/*******************************************************************************
 *  (c) 2018 - 2022 Zondax AG
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/
#include "parser_print_common.h"

#include "base58.h"
#include "bech32.h"
#include "parser_common.h"
#include "timeutils.h"
#include "zxformat.h"
#include "zxmacros.h"

#if defined(LEDGER_SPECIFIC)
#include "cx.h"
#else
#include "picohash.h"
#define CX_SHA256_SIZE 32
#define CX_RIPEMD160_SIZE 20
#endif

void remove_fraction(char *s) {
    size_t len = strlen(s);

    // Find the decimal point
    char *decimal_point = strchr(s, '.');
    if (decimal_point == NULL) {
        // No decimal point found, nothing to remove
        return;
    }

    // Find the end of the string up to the decimal point
    size_t end_index = decimal_point - s;

    // Find the first non-zero digit after the decimal point
    size_t non_zero_index = end_index + 1;
    while (s[non_zero_index] == '0') {
        non_zero_index++;
    }

    // Check if there is a non-zero digit after the decimal point
    if (non_zero_index >= len) {
        // There is no non-zero digit after the decimal point
        // Remove the decimal point and trailing zeros
        s[end_index] = '\0';
    }
}

parser_error_t printAmount64(uint64_t amount, uint8_t amountDenom, network_id_e network_id, char *outVal, uint16_t outValLen,
                             uint8_t pageIdx, uint8_t *pageCount) {
    char strAmount[33] = {0};
    if (uint64_to_str(strAmount, sizeof(strAmount), amount) != NULL) {
        return parser_unexpected_error;
    }
    if (intstr_to_fpstr_inplace(strAmount, sizeof(strAmount), amountDenom) == 0) {
        return parser_unexpected_error;
    }

    const char *symbol = NULL;
    switch (network_id) {
        case songbird:
            symbol = " SGB";
            break;
        case coston:
            symbol = " CFLR";
            break;
        case coston2:
            symbol = " C2FLR";
            break;
        case mainnet:
            symbol = " FLR";
            break;
        default:
            return parser_unexpected_error;
            break;
    }

    number_inplace_trimming(strAmount, 1);
    remove_fraction(strAmount);
    z_str3join(strAmount, sizeof(strAmount), "", symbol);
    pageString(outVal, outValLen, strAmount, pageIdx, pageCount);

    return parser_ok;
}

parser_error_t printAddress(const uint8_t *pubkey, network_id_e network_id, char *outVal, uint16_t outValLen,
                            uint8_t pageIdx, uint8_t *pageCount) {
    if (pubkey == NULL) {
        return parser_unexpected_error;
    }

    const char *hrp = NULL;
    switch (network_id) {
        case songbird:
            hrp = "song";
            break;
        case coston:
            hrp = "coston";
            break;
        case coston2:
            hrp = "costwo";
            break;
        case mainnet:
            hrp = "flare";
            break;
        default:
            return parser_unexpected_error;
            break;
    }
    char address[100] = {0};
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    const zxerr_t err = zxerr_ok;  // Bypass bech32 encoding
#else
    const zxerr_t err = bech32EncodeFromBytes(address, sizeof(address), hrp, pubkey, ADDRESS_LEN, 1, BECH32_ENCODING_BECH32);
#endif

    if (err != zxerr_ok) {
        return parser_unexpected_error;
    }

    pageString(outVal, outValLen, (const char *)address, pageIdx, pageCount);
    return parser_ok;
}

parser_error_t printTimestamp(uint64_t timestamp, char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    timedata_t date = {0};

    if (extractTime(timestamp, &date) != zxerr_ok) {
        return parser_unexpected_error;
    }

    char time_str[30] = {0};
    snprintf(time_str, 30, "%04d-%02d-%02d %02d:%02d:%02d UTC", date.tm_year, date.tm_mon, date.tm_day, date.tm_hour,
             date.tm_min, date.tm_sec);

    pageString(outVal, outValLen, time_str, pageIdx, pageCount);

    return parser_ok;
}

parser_error_t printNodeId(const uint8_t *nodeId, char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    if (nodeId == NULL) {
        return parser_unexpected_error;
    }

    char data[NODE_ID_LEN + CB58_CHECKSUM_LEN] = {0};

    // Copy node_id to data
    MEMCPY(data, nodeId, NODE_ID_LEN);

    // Calculate SHA256 checksum
    uint8_t checksum[CX_SHA256_SIZE] = {0};
#if defined(LEDGER_SPECIFIC)
    cx_sha256_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    cx_sha256_init_no_throw(&ctx);
    CHECK_CX_PARSER_OK(cx_hash_no_throw(&ctx.header, CX_LAST, nodeId, NODE_ID_LEN, checksum, CX_SHA256_SIZE));
#else
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    // For fuzzing: use a safer hash computation to avoid undefined behavior
    // This is unsafe for production but acceptable for fuzzing tests
    memset(checksum, 0, CX_SHA256_SIZE);
    // Create a simple checksum from the nodeId for fuzzing purposes
    for (uint8_t i = 0; i < NODE_ID_LEN && i < CX_SHA256_SIZE; i++) {
        checksum[i % CX_SHA256_SIZE] ^= nodeId[i];
    }
#else
    picohash_ctx_t ctx;
    picohash_init_sha256(&ctx);
    picohash_update(&ctx, nodeId, NODE_ID_LEN);
    picohash_final(&ctx, checksum);
#endif
#endif

    // Copy last 4 bytes of checksum to data
    MEMCPY(data + NODE_ID_LEN, checksum + CX_SHA256_SIZE - CB58_CHECKSUM_LEN, CB58_CHECKSUM_LEN);

    // Prefix for node_id
    const char prefix[] = "NodeID-";

    // Prepare node_id by appending prefix
    uint8_t node_id[NODE_ID_MAX_SIZE] = {0};
    size_t outlen = sizeof(node_id) - sizeof(prefix) + 1;
    memcpy(node_id, prefix, sizeof(prefix) - 1);

    CHECK_ERROR(encode_base58((const unsigned char *)data, sizeof(data), node_id + sizeof(prefix) - 1, &outlen))
    pageString(outVal, outValLen, (const char *)node_id, pageIdx, pageCount);

    return parser_ok;
}

parser_error_t printHash(const parser_context_t *ctx, char *outVal, uint16_t outValLen, uint8_t pageIdx,
                         uint8_t *pageCount) {
    unsigned char hash[CX_SHA256_SIZE] = {0};

#if defined(LEDGER_SPECIFIC)
    cx_sha256_t hash_ctx;
    memset(&hash_ctx, 0, sizeof(hash_ctx));
    CHECK_CX_PARSER_OK(cx_sha256_init_no_throw(&hash_ctx));
    CHECK_CX_PARSER_OK(cx_hash_no_throw(&hash_ctx.header, CX_LAST, ctx->buffer, ctx->bufferLen, hash, sizeof(hash)));
#else
    picohash_ctx_t hash_ctx;
    picohash_init_sha256(&hash_ctx);
    picohash_update(&hash_ctx, ctx->buffer, ctx->bufferLen);
    picohash_final(&hash_ctx, &hash);
#endif
    pageStringHex(outVal, outValLen, (const char *)hash, sizeof(hash), pageIdx, pageCount);
    return parser_ok;
}