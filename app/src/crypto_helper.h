/*******************************************************************************
 *   (c) 2018 - 2022 Zondax AG
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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "coin.h"
#include "parser_common.h"
#include "zxerror.h"

#define KECCAK_256_SIZE 32

#define CHECK_CX_OK(CALL)         \
    do {                          \
        cx_err_t __cx_err = CALL; \
        if (__cx_err != CX_OK) {  \
            return zxerr_unknown; \
        }                         \
    } while (0)

#define MAX_BECH32_HRP_LEN 83u
#define SELECTOR_LENGTH 4
#define BIGINT_LENGTH 32

extern uint8_t bech32_hrp_len;
extern char bech32_hrp[MAX_BECH32_HRP_LEN + 1];

uint8_t crypto_encodePubkey(const uint8_t *pubkey, char *out, uint16_t out_len);

zxerr_t crypto_sha256(const uint8_t *input, uint16_t inputLen, uint8_t *output, uint16_t outputLen);

zxerr_t ripemd160_32(uint8_t *out, uint8_t *in);
#ifdef __cplusplus
}
#endif
