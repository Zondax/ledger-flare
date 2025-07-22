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

#include "parser_impl_evm_specific.h"

#include <stdint.h>

#include "app_mode.h"
#include "coin.h"
#include "evm_erc20.h"
#include "evm_utils.h"
#include "zxformat.h"

#define SUPPORTED_NETWORKS_EVM_LEN 4
#define FLARE_MAINNET_CHAINID 14
#define COSTON_CHAINID 16
#define SONG_BIRD_CHAINID 19
#define COSTON2_CHAINID 114
#define TMP_DATA_ARRAY_SIZE 40
#define ERC20_TRANSFER_OFFSET 4 + 12

const uint64_t supported_networks_evm[SUPPORTED_NETWORKS_EVM_LEN] = {FLARE_MAINNET_CHAINID, COSTON_CHAINID,
                                                                     SONG_BIRD_CHAINID, COSTON2_CHAINID};

const uint8_t supported_networks_evm_len = SUPPORTED_NETWORKS_EVM_LEN;

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

const uint8_t supportedTokensSize = sizeof(supportedTokens) / sizeof(supportedTokens[0]);

static parser_error_t getNetworkName(uint64_t chainId, char *outVal, uint16_t outValLen) {
    switch (chainId) {
        case FLARE_MAINNET_CHAINID:
            snprintf(outVal, outValLen, "Flare");
            break;
        case COSTON_CHAINID:
            snprintf(outVal, outValLen, "Coston Flare");
            break;
        case SONG_BIRD_CHAINID:
            snprintf(outVal, outValLen, "Songbird");
            break;
        case COSTON2_CHAINID:
            snprintf(outVal, outValLen, "Coston2 Flare");
            break;
        default:
            return parser_invalid_chain_id;
    }
    return parser_ok;
}

parser_error_t printERC20TransferAppSpecific(const parser_context_t *ctx, const eth_tx_t *ethTxObj, uint8_t displayIdx,
                                             char *outKey, uint16_t outKeyLen, char *outVal, uint16_t outValLen,
                                             uint8_t pageIdx, uint8_t *pageCount) {
    if (ctx == NULL || ethTxObj == NULL || outKey == NULL || outVal == NULL || pageCount == NULL) {
        return parser_unexpected_error;
    }

    if ((ethTxObj->tx_type == legacy || ethTxObj->tx_type == eip2930) && displayIdx >= 5) {
        displayIdx += 2;
    }

    if (ethTxObj->tx_type == eip1559 && displayIdx >= 7) {
        displayIdx++;
    }

    char data_array[TMP_DATA_ARRAY_SIZE] = {0};
    switch (displayIdx) {
        case 0:
            snprintf(outKey, outKeyLen, "Receiver");
            rlp_t to = {
                .kind = RLP_KIND_STRING, .ptr = (ethTxObj->tx.data.ptr + ERC20_TRANSFER_OFFSET), .rlpLen = ETH_ADDRESS_LEN};
            CHECK_ERROR(printEVMAddress(&to, outVal, outValLen, pageIdx, pageCount));
            break;

        case 1:
            snprintf(outKey, outKeyLen, "Contract");
            rlp_t contractAddress = {.kind = RLP_KIND_STRING, .ptr = ethTxObj->tx.to.ptr, .rlpLen = ETH_ADDRESS_LEN};
            CHECK_ERROR(printEVMAddress(&contractAddress, outVal, outValLen, pageIdx, pageCount));
            break;
        case 2:
            snprintf(outKey, outKeyLen, "Coin asset");
            CHECK_ERROR(getNetworkName(ethTxObj->chainId.chain_id_decoded, outVal, outValLen));
            break;
        case 3:
            snprintf(outKey, outKeyLen, "Amount");
            CHECK_ERROR(printERC20Value(ethTxObj, outVal, outValLen, pageIdx, pageCount));
            break;

        case 4:
            snprintf(outKey, outKeyLen, "Nonce");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.nonce, outVal, outValLen, pageIdx, pageCount));
            break;

        case 5:
            snprintf(outKey, outKeyLen, "Max Priority Fee");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.max_priority_fee_per_gas, outVal, outValLen, pageIdx, pageCount));
            break;

        case 6:
            snprintf(outKey, outKeyLen, "Max Fee");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.max_fee_per_gas, outVal, outValLen, pageIdx, pageCount));
            break;

        case 7:
            snprintf(outKey, outKeyLen, "Gas price");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.gasPrice, outVal, outValLen, pageIdx, pageCount));
            break;

        case 8:
            snprintf(outKey, outKeyLen, "Gas limit");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.gasLimit, outVal, outValLen, pageIdx, pageCount));
            break;

        case 9:
            snprintf(outKey, outKeyLen, "Value");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.value, outVal, outValLen, pageIdx, pageCount));
            break;

        case 10:
            snprintf(outKey, outKeyLen, "Data");
            array_to_hexstr(data_array, sizeof(data_array), ethTxObj->tx.data.ptr,
                            ethTxObj->tx.data.rlpLen > DATA_BYTES_TO_PRINT ? DATA_BYTES_TO_PRINT : ethTxObj->tx.data.rlpLen);

            if (ethTxObj->tx.data.rlpLen > DATA_BYTES_TO_PRINT) {
                snprintf(data_array + (2 * DATA_BYTES_TO_PRINT), 4, "...");
            }

            pageString(outVal, outValLen, data_array, pageIdx, pageCount);
            break;

        case 11:
            CHECK_ERROR(printEthHash(ctx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount));
            break;

        default:
            return parser_display_page_out_of_range;
    }

    return parser_ok;
}

parser_error_t getNumItemsEthAppSpecific(eth_tx_t *ethTxObj, uint8_t *numItems) {
    if (numItems == NULL || ethTxObj == NULL) {
        return parser_unexpected_error;
    }
    // Verify that tx is ERC20
    if (validateERC20(ethTxObj)) {
        if (ethTxObj->tx_type == legacy || ethTxObj->tx_type == eip2930) {
            *numItems = 10;
        } else {
            *numItems = 11;
        }
        return parser_ok;
    }

    if (ethTxObj->tx_type == legacy || ethTxObj->tx_type == eip2930) {
        *numItems = 6;
    } else {
        *numItems = 7;
    }

    *numItems += ((ethTxObj->tx.data.rlpLen != 0) ? 1 : 0) + ((ethTxObj->tx.to.rlpLen != 0) ? 1 : 0);

    return parser_ok;
}

parser_error_t printGenericAppSpecific(const parser_context_t *ctx, const eth_tx_t *ethTxObj, uint8_t displayIdx,
                                       char *outKey, uint16_t outKeyLen, char *outVal, uint16_t outValLen, uint8_t pageIdx,
                                       uint8_t *pageCount) {
    if (ctx == NULL || ethTxObj == NULL) {
        return parser_unexpected_error;
    }

    char data_array[TMP_DATA_ARRAY_SIZE] = {0};

    if ((displayIdx >= 3 && ethTxObj->tx.data.rlpLen == 0) || ethTxObj->tx.to.rlpLen == 0) {
        displayIdx += 1;
    }

    if (ethTxObj->tx_type == eip1559 && displayIdx >= 7) {
        displayIdx++;
    }

    if ((ethTxObj->tx_type == legacy || ethTxObj->tx_type == eip2930) && displayIdx >= 4) {
        displayIdx += 2;
    }

    switch (displayIdx) {
        case 0:
            snprintf(outKey, outKeyLen, "To");
            rlp_t contractAddress = {.kind = RLP_KIND_STRING, .ptr = ethTxObj->tx.to.ptr, .rlpLen = ETH_ADDRESS_LEN};
            CHECK_ERROR(printEVMAddress(&contractAddress, outVal, outValLen, pageIdx, pageCount));
            break;
        case 1:
            snprintf(outKey, outKeyLen, "Coin asset");
            CHECK_ERROR(getNetworkName(ethTxObj->chainId.chain_id_decoded, outVal, outValLen));
            break;
        case 2:
            snprintf(outKey, outKeyLen, "Value");
            printBigIntFixedPoint(ethTxObj->tx.value.ptr, ethTxObj->tx.value.rlpLen, outVal, outValLen, pageIdx, pageCount,
                                  COIN_AMOUNT_DECIMAL);
            break;

        case 3:
            snprintf(outKey, outKeyLen, "Data");
            array_to_hexstr(data_array, sizeof(data_array), ethTxObj->tx.data.ptr,
                            ethTxObj->tx.data.rlpLen > DATA_BYTES_TO_PRINT ? DATA_BYTES_TO_PRINT : ethTxObj->tx.data.rlpLen);

            if (ethTxObj->tx.data.rlpLen > DATA_BYTES_TO_PRINT) {
                snprintf(data_array + (2 * DATA_BYTES_TO_PRINT), 4, "...");
            }

            pageString(outVal, outValLen, data_array, pageIdx, pageCount);
            break;

        case 4:
            snprintf(outKey, outKeyLen, "Max Priority Fee");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.max_priority_fee_per_gas, outVal, outValLen, pageIdx, pageCount));
            break;

        case 5:
            snprintf(outKey, outKeyLen, "Max Fee");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.max_fee_per_gas, outVal, outValLen, pageIdx, pageCount));
            break;

        case 6:
            snprintf(outKey, outKeyLen, "Gas limit");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.gasLimit, outVal, outValLen, pageIdx, pageCount));
            break;

        case 7:
            snprintf(outKey, outKeyLen, "Gas price");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.gasPrice, outVal, outValLen, pageIdx, pageCount));
            break;

        case 8:
            snprintf(outKey, outKeyLen, "Nonce");
            CHECK_ERROR(printRLPNumber(&ethTxObj->tx.nonce, outVal, outValLen, pageIdx, pageCount));
            break;

        case 9:
            CHECK_ERROR(printEthHash(ctx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount));
            break;

        default:
            return parser_display_page_out_of_range;
    }

    return parser_ok;
}
