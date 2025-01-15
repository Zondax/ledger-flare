/*******************************************************************************
 *   (c) 2018 - 2023 Zondax AG
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

#define CLA 0x58

typedef enum {
    BECH32 = 0,
    UNSUPPORTED = 0xFF,
} address_encoding_e;

#define INS_SIGN_HASH 0x3
#define MAX_BIP32_PATH 10
#define HDPATH_LEN_DEFAULT 5

#define HDPATH_2_DEFAULT (0x80000000u | 0u)
#define HDPATH_3_DEFAULT (0u)
#define HDPATH_4_DEFAULT (0u)

#define PK_LEN_SECP256K1_UNCOMPRESSED 65u

#define PK_LEN_SECP256K1 33u
#define VIEW_ADDRESS_OFFSET_SECP256K1 PK_LEN_SECP256K1
// omit the pubkey + 1-byte pubkey len + 1-byte address len
#define VIEW_ADDRESS_OFFSET_ETH (SECP256K1_PK_LEN + 1 + 1)
#define SK_LEN_25519 64u
#define SCALAR_LEN_ED25519 32u
#define SIG_PLUS_TYPE_LEN 65u

#define MAX_SIGN_SIZE 256u
#define BLAKE2B_DIGEST_SIZE 32u

#define COIN_AMOUNT_DECIMAL_PLACES 6
#define COIN_TICKER "FLR "

#define SECP256K1_SK_LEN 64u
#define SECP256K1_PK_LEN 65u
#define ETH_ADDR_LEN 20u

#define COIN_AMOUNT_DECIMAL 18

#define MENU_MAIN_APP_LINE1 "Flare Network"
#define MENU_MAIN_APP_LINE2 "Ready"
#define MENU_MAIN_APP_LINE2_SECRET "???"
#define APPVERSION_LINE1 "Flare"
#define APPVERSION_LINE2 "v" APPVERSION

#ifdef __cplusplus
}
#endif
