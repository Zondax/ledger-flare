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

TEST(SCALE, ReadBytes) {
    parser_context_t ctx;
    parser_tx_t tx_obj;
    parser_error_t err;
    uint8_t buffer[100];
    auto bufferLen = parseHexString(buffer, sizeof(buffer),
                                    "45"
                                    "123456"
                                    "12345678901234567890");

    parser_parse(&ctx, buffer, bufferLen, &tx_obj);

    // uint8_t bytesArray[100] = {0};
    // err = _readBytes(&ctx, bytesArray, 1);
    // EXPECT_EQ(err, parser_ok) << parser_getErrorDescription(err);
    // EXPECT_EQ(bytesArray[0], 0x45);

    // uint8_t testArray[3] = {0x12, 0x34, 0x56};
    // err = _readBytes(&ctx, bytesArray+1, 3);
    // EXPECT_EQ(err, parser_ok) << parser_getErrorDescription(err);
    // for (uint8_t i = 0; i < 3; i++) {
    //     EXPECT_EQ(testArray[i], bytesArray[i+1]);
    // }

    // uint8_t testArray2[10] = {0x12, 0x34, 0x56, 0x78, 0x90, 0x12, 0x34, 0x56, 0x78, 0x90};
    // err = _readBytes(&ctx, bytesArray+4, 10);
    // EXPECT_EQ(err, parser_ok) << parser_getErrorDescription(err);
    // for (uint8_t i = 0; i < 10; i++) {
    //     EXPECT_EQ(testArray2[i], bytesArray[i+4]);
    // }
}

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
