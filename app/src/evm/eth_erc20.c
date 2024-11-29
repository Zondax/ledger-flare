/*******************************************************************************
 *  (c) 2018 - 2023 Zondax AG
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

#include "eth_erc20.h"

#include "zxformat.h"

// Prefix is calculated as: keccak256("transfer(address,uint256)") = 0xa9059cbb
const uint8_t ERC20_TRANSFER_PREFIX[] = {0xa9, 0x05, 0x9c, 0xbb};

#define DECIMAL_BASE 10
const erc20_tokens_t supportedTokens[] = {
    {{0x1D, 0x80, 0xc4, 0x9B, 0xbB, 0xCd, 0x1C, 0x09, 0x11, 0x34,
      0x66, 0x56, 0xB5, 0x29, 0xDF, 0x9E, 0x5c, 0x2F, 0x78, 0x3d},
     "WFLR ",
     18},
    {{0x02, 0xf0, 0x82, 0x6e, 0xf6, 0xad, 0x10, 0x7c, 0xfc, 0x86,
      0x11, 0x52, 0xb3, 0x2b, 0x52, 0xfd, 0x11, 0xba, 0xb9, 0xed},
     "WSGB ",
     18},

};

parser_error_t getERC20Token(const eth_tx_t *ethObj, char tokenSymbol[MAX_SYMBOL_LEN], uint8_t *decimals) {
    if (ethObj == NULL || tokenSymbol == NULL || decimals == NULL || ethObj->tx.data.rlpLen != ERC20_DATA_LENGTH ||
        memcmp(ethObj->tx.data.ptr, ERC20_TRANSFER_PREFIX, 4) != 0) {
        return parser_unexpected_value;
    }

    // Check if token is in the list
    const uint8_t supportedTokensSize = sizeof(supportedTokens) / sizeof(supportedTokens[0]);
    for (uint8_t i = 0; i < supportedTokensSize; i++) {
        if (memcmp(ethObj->tx.to.ptr, supportedTokens[i].address, ETH_ADDRESS_LEN) == 0) {
            // Set symbol and decimals
            snprintf(tokenSymbol, 10, "%s", (char *)PIC(supportedTokens[i].symbol));
            *decimals = supportedTokens[i].decimals;
            return parser_ok;
        }
    }

    // WNAT token
    snprintf(tokenSymbol, 10, "WNAT ");
    *decimals = 0;
    return parser_ok;
}
parser_error_t printERC20Value(const eth_tx_t *ethObj, char *outVal, uint16_t outValLen, uint8_t pageIdx,
                               uint8_t *pageCount) {
    if (ethObj == NULL || outVal == NULL || pageCount == NULL) {
        return parser_unexpected_error;
    }

    // [identifier (4) | token contract (12 + 20) | value (32)]
    char tokenSymbol[10] = {0};
    uint8_t decimals = 0;
    CHECK_ERROR(getERC20Token(ethObj, tokenSymbol, &decimals))

    uint256_t value = {0};
    const uint8_t *valuePtr = ethObj->tx.data.ptr + SELECTOR_LENGTH + BIGINT_LENGTH;
    parser_context_t tmpCtx = {.buffer = valuePtr, .bufferLen = BIGINT_LENGTH, .offset = 0};
    CHECK_ERROR(readu256BE(&tmpCtx, &value));

    char bufferUI[100] = {0};
    if (!tostring256(&value, DECIMAL_BASE, bufferUI, sizeof(bufferUI))) {
        return parser_unexpected_error;
    }

    // Add symbol, add decimals, page number
    if (intstr_to_fpstr_inplace(bufferUI, sizeof(bufferUI), decimals) == 0) {
        return parser_unexpected_value;
    }

    if (z_str3join(bufferUI, sizeof(bufferUI), tokenSymbol, NULL) != zxerr_ok) {
        return parser_unexpected_buffer_end;
    }

    number_inplace_trimming(bufferUI, 1);
    pageString(outVal, outValLen, bufferUI, pageIdx, pageCount);

    return parser_ok;
}

bool validateERC20(eth_tx_t *ethObj) {
    // Check that data start with ERC20 prefix
    if (ethObj == NULL || ethObj->tx.to.rlpLen != ETH_ADDRESS_LEN || ethObj->tx.data.ptr == NULL ||
        ethObj->tx.data.rlpLen != ERC20_DATA_LENGTH ||
        memcmp(ethObj->tx.data.ptr, ERC20_TRANSFER_PREFIX, sizeof(ERC20_TRANSFER_PREFIX)) != 0) {
        return false;
    }
    return true;
}
