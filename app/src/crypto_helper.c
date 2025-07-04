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
#include "crypto_helper.h"

#include "bech32.h"
#include "coin.h"
#include "zxformat.h"

#if defined(LEDGER_SPECIFIC)
#include "cx.h"
#else
#include "picohash.h"
#define CX_SHA256_SIZE 32
#define CX_RIPEMD160_SIZE 20
#endif

uint8_t bech32_hrp_len;
char bech32_hrp[MAX_BECH32_HRP_LEN + 1];

zxerr_t crypto_sha256(const uint8_t *input, uint16_t inputLen, uint8_t *output, uint16_t outputLen) {
    if (input == NULL || output == NULL || outputLen < CX_SHA256_SIZE) {
        return zxerr_encoding_failed;
    }

    MEMZERO(output, outputLen);

#if defined(LEDGER_SPECIFIC)
    cx_sha256_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    cx_sha256_init_no_throw(&ctx);
    CHECK_CX_OK(cx_hash_no_throw(&ctx.header, CX_LAST, input, inputLen, output, CX_SHA256_SIZE));
#else
    picohash_ctx_t ctx;
    picohash_init_sha256(&ctx);
    picohash_update(&ctx, input, inputLen);
    picohash_final(&ctx, output);
#endif
    return zxerr_ok;
}

zxerr_t ripemd160_32(uint8_t *out, uint8_t *in) {
#if defined(LEDGER_SPECIFIC)
    cx_ripemd160_t rip160 = {0};
    cx_ripemd160_init(&rip160);
    CHECK_CX_OK(cx_hash_no_throw(&rip160.header, CX_LAST, in, CX_SHA256_SIZE, out, CX_RIPEMD160_SIZE));
#endif
    return zxerr_ok;
}

uint8_t crypto_encodePubkey(const uint8_t *pubkey, char *out, uint16_t out_len) {
    if (pubkey == NULL || out == NULL) {
        return 0;
    }

    // Hash it
    uint8_t hashed1_pk[CX_SHA256_SIZE] = {0};
    crypto_sha256(pubkey, PK_LEN_SECP256K1, hashed1_pk, CX_SHA256_SIZE);

    uint8_t hashed2_pk[CX_RIPEMD160_SIZE] = {0};
    CHECK_ZXERR(ripemd160_32(hashed2_pk, hashed1_pk))

    CHECK_ZXERR(bech32EncodeFromBytes(out, out_len, bech32_hrp, hashed2_pk, CX_RIPEMD160_SIZE, 1, BECH32_ENCODING_BECH32))

    return strlen(out);
}
