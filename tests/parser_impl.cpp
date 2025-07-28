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

#include <hexutils.h>
#include <string.h>

#include <cstdint>
#include <string>

#include "bech32.h"
#include "coin.h"
#include "crypto_helper.h"
#include "gtest/gtest.h"
#include "hexutils.h"
#include "parser.h"
#include "parser_common.h"
#include "parser_txdef.h"
#include "segwit_addr.h"
#include "zxerror.h"

extern "C" {
#include "ripemd160.h"
}
using namespace std;

// char PARSER_KEY[16384];
// char PARSER_VALUE[16384];

// TEST(SCALE, ReadBytes) {
//     parser_context_t ctx;
//     parser_tx_t tx_obj;
//     parser_error_t err;
//     uint8_t buffer[1000];
//     auto bufferLen = parseHexString(
//         buffer, sizeof(buffer),
//         "00000000001100000007000000000000000000000000000000000000000000000000000000000000000000000001ac005d00000000000000000"
//         "0000000000000000000070000000000000020000000000007012afffbfffb000000000000000000000000000000000001ac00ff4d0000000000"
//         "00000000000000000000000000000000000000ffb119b404c1356b6bfdb80045e27ba13c3789b5b3684f001da071cd4e6db09c00000000");

//     err = parser_parse(&ctx, buffer, bufferLen, &tx_obj);
//     if (err != parser_ok) {
//         printf("error in parser_parse: %s\n", parser_getErrorDescription(err));
//     }

//     err = parser_validate(&ctx);
//     if (err != parser_ok) {
//         printf("error in parser_parse: %s\n", parser_getErrorDescription(err));
//     }

//     uint8_t num_items;
//     err = parser_getNumItems(&ctx, &num_items);
//     if (err != parser_ok) {
//         printf("error in parser_parse: %s\n", parser_getErrorDescription(err));
//     }

//     (void)fprintf(stderr, "----------------------------------------------\n");

//     for (uint8_t i = 0; i < num_items; i += 1) {
//         uint8_t page_idx = 0;
//         uint8_t page_count = 1;
//         while (page_idx < page_count) {
//             err = parser_getItem(&ctx, i, PARSER_KEY, sizeof(PARSER_KEY), PARSER_VALUE, sizeof(PARSER_VALUE), page_idx,
//                                  &page_count);

//             //            (void)fprintf(stderr, "%s = %s\n", PARSER_KEY, PARSER_VALUE);

//             if (err != parser_ok) {
//                 (void)fprintf(stderr, "error getting item %u at page index %u: %s\n", (unsigned)i, (unsigned)page_idx,
//                               parser_getErrorDescription(err));
//                 // assert(false);
//             }

//             page_idx += 1;
//         }
//     }
// }

TEST(Address, FlareAddress) {
    const char compressedPubkey[] = "0226525208673808e006c9efbc1bce812b21c67aa286eb550b3c0dd208095cc3a7";

    uint8_t pubkey[PK_LEN_SECP256K1] = {0};
    parseHexString(pubkey, sizeof(pubkey), compressedPubkey);

    uint8_t hash[32] = {0};
    crypto_sha256(pubkey, PK_LEN_SECP256K1, hash, 32);

    uint8_t hash2[20] = {0};
    ripemd160(hash, 32, hash2);

    const char bech32_hrp[] = "flare";
    char address[100] = {0};
    const zxerr_t err = bech32EncodeFromBytes(address, sizeof(address), bech32_hrp, hash2, 20, 1, BECH32_ENCODING_BECH32);
    EXPECT_EQ(err, zxerr_ok);

    const std::string flare_address(address, address + strnlen(address, sizeof(address)));
    EXPECT_EQ(flare_address, "flare1yh62d5xdyzu5w2nc6qpyymsjzqc5qzaumcf0jy");
}

TEST(Address, CostonAddress) {
    const char compressedPubkey[] = "0226525208673808e006c9efbc1bce812b21c67aa286eb550b3c0dd208095cc3a7";

    uint8_t pubkey[PK_LEN_SECP256K1] = {0};
    parseHexString(pubkey, sizeof(pubkey), compressedPubkey);

    uint8_t hash[32] = {0};
    crypto_sha256(pubkey, PK_LEN_SECP256K1, hash, 32);

    uint8_t hash2[20] = {0};
    ripemd160(hash, 32, hash2);

    const char bech32_hrp[] = "costwo";
    char address[100] = {0};
    const zxerr_t err = bech32EncodeFromBytes(address, sizeof(address), bech32_hrp, hash2, 20, 1, BECH32_ENCODING_BECH32);
    EXPECT_EQ(err, zxerr_ok);

    const std::string flare_address(address, address + strnlen(address, sizeof(address)));
    EXPECT_EQ(flare_address, "costwo1yh62d5xdyzu5w2nc6qpyymsjzqc5qzaurksye9");
}
