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

#include "parser_impl_eth.h"

#include <stdio.h>
#include <zxmacros.h>

#include "app_mode.h"
#include "common/parser_common.h"
#include "crypto_eth.h"
#include "eth_erc20.h"
#include "eth_utils.h"
#include "parser_txdef.h"
#include "rlp.h"
#include "uint256.h"
#include "zxformat.h"

eth_tx_t eth_tx_obj;
#define FLARE_MAINNET_CHAINID 14
#define COSTON_CHAINID 16
#define SONG_BIRD_CHAINID 19
#define COSTON2_CHAINID 114
#define DATA_BYTES_TO_PRINT 10
#define TMP_DATA_ARRAY_SIZE 40
#define ERC20_TRANSFER_OFFSET 4 + 12
#define ETHEREUM_RECOVERY_OFFSET 27
#define EIP155_V_BASE 35

static parser_error_t readChainID(parser_context_t *ctx, rlp_t *chainId) {
    if (ctx == NULL || chainId == NULL) {
        return parser_unexpected_error;
    }

    CHECK_ERROR(rlp_read(ctx, chainId));
    uint64_t tmpChainId = 0;
    if (chainId->rlpLen > 0) {
        CHECK_ERROR(be_bytes_to_u64(chainId->ptr, chainId->rlpLen, &tmpChainId))
    } else if (chainId->kind == RLP_KIND_BYTE) {
        // case were the prefix is the byte itself
        tmpChainId = chainId->ptr[0];
    } else {
        return parser_unexpected_error;
    }

    // Check allowed values for chain id
    if (tmpChainId != FLARE_MAINNET_CHAINID && tmpChainId != COSTON_CHAINID && tmpChainId != SONG_BIRD_CHAINID &&
        tmpChainId != COSTON2_CHAINID) {
        return parser_invalid_chain_id;
    }

    chainId->chain_id_decoded = tmpChainId;

    return parser_ok;
}

static parser_error_t parse_legacy_tx(parser_context_t *ctx, eth_tx_t *tx_obj) {
    if (ctx == NULL || tx_obj == NULL) {
        return parser_unexpected_error;
    }

    CHECK_ERROR(rlp_read(ctx, &tx_obj->tx.nonce));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.gasPrice)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.gasLimit)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.to)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.value)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.data)));

    // Check for legacy no EIP155 which means no chain_id
    // There is not more data no eip155 compliant tx
    if (ctx->offset == ctx->bufferLen) {
        tx_obj->chainId.kind = RLP_KIND_BYTE;
        tx_obj->chainId.ptr = NULL;
        tx_obj->chainId.rlpLen = 0;
        return parser_ok;
    }

    // Otherwise legacy EIP155 in which case should come with empty r and s values
    // Transaction comes with a chainID so it is EIP155 compliant
    CHECK_ERROR(readChainID(ctx, &tx_obj->chainId));

    // Check R and S fields
    rlp_t sig_r = {0};
    CHECK_ERROR(rlp_read(ctx, &sig_r));

    rlp_t sig_s = {0};
    CHECK_ERROR(rlp_read(ctx, &sig_s));

    // R and S values should be either 0 or 0x80
    if ((sig_r.rlpLen == 0 && sig_s.rlpLen == 0) ||
        ((sig_r.rlpLen == 1 && sig_s.rlpLen == 1) && !(*sig_r.ptr | *sig_s.ptr))) {
        return parser_ok;
    }
    return parser_invalid_rs_values;
}

static parser_error_t parse_2930(parser_context_t *ctx, eth_tx_t *tx_obj) {
    if (ctx == NULL || tx_obj == NULL) {
        return parser_unexpected_error;
    }
    CHECK_ERROR(readChainID(ctx, &tx_obj->chainId));
    CHECK_ERROR(rlp_read(ctx, &tx_obj->tx.nonce));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.gasPrice)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.gasLimit)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.to)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.value)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.data)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.access_list)));

    // R and S fields should be empty
    if (ctx->offset < ctx->bufferLen) {
        return parser_unexpected_characters;
    }

    return parser_ok;
}

static parser_error_t parse_1559(parser_context_t *ctx, eth_tx_t *tx_obj) {
    if (ctx == NULL || tx_obj == NULL) {
        return parser_unexpected_error;
    }

    CHECK_ERROR(readChainID(ctx, &tx_obj->chainId));
    CHECK_ERROR(rlp_read(ctx, &tx_obj->tx.nonce));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.max_priority_fee_per_gas)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.max_fee_per_gas)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.gasLimit)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.to)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.value)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.data)));
    CHECK_ERROR(rlp_read(ctx, &(tx_obj->tx.access_list)));

    // R and S fields should be empty
    if (ctx->offset < ctx->bufferLen) {
        return parser_unexpected_characters;
    }

    return parser_ok;
}

static parser_error_t readTxnType(parser_context_t *ctx, eth_tx_type_e *type) {
    if (ctx == NULL || type == NULL || ctx->bufferLen == 0 || ctx->offset != 0) {
        return parser_unexpected_error;
    }
    // Check first byte:
    //    0x01 --> EIP2930
    //    0x02 --> EIP1559
    // >= 0xC0 --> Legacy
    uint8_t marker = *(ctx->buffer + ctx->offset);

    if (marker == eip2930 || marker == eip1559) {
        *type = (eth_tx_type_e)marker;
        ctx->offset++;
        return parser_ok;
    }

    // Legacy tx type is greater than or equal to 0xc0.
    if (marker < legacy) {
        return parser_unsupported_tx;
    }

    *type = legacy;
    return parser_ok;
}

parser_error_t _readEth(parser_context_t *ctx, eth_tx_t *tx_obj) {
    MEMZERO(&eth_tx_obj, sizeof(eth_tx_obj));
    CHECK_ERROR(readTxnType(ctx, &tx_obj->tx_type))
    // We expect a list with all the fields from the transaction
    rlp_t list = {0};
    CHECK_ERROR(rlp_read(ctx, &list));

    // Check that the first RLP element is a list
    if (list.kind != RLP_KIND_LIST) {
        return parser_unexpected_value;
    }

    // All bytes must be read
    if (ctx->offset != ctx->bufferLen) {
        return parser_unexpected_characters;
    }

    parser_context_t txCtx = {.buffer = list.ptr, .bufferLen = list.rlpLen, .offset = 0};
    switch (tx_obj->tx_type) {
        case eip1559: {
            return parse_1559(&txCtx, tx_obj);
        }

        case eip2930: {
            return parse_2930(&txCtx, tx_obj);
        }

        case legacy: {
            return parse_legacy_tx(&txCtx, tx_obj);
        }
    }
    return parser_unexpected_error;
}

parser_error_t _validateTxEth() {
    // If there is no data or erc20 transfer we will be able to print everything no blindsign required
    if (eth_tx_obj.tx.data.rlpLen == 0 || validateERC20(&eth_tx_obj)) {
        app_mode_skip_blindsign_ui();
        return parser_ok;
    } else if (!app_mode_blindsign()) {  // If it is not an ERC20 transfer or data is not empty require blindsinging if not
                                         // enable
        return parser_blindsign_required;
    }

    return parser_ok;
}

static parser_error_t printEthHash(const parser_context_t *ctx, char *outKey, uint16_t outKeyLen, char *outVal,
                                   uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    // we need to get keccak hash of the transaction data
    uint8_t hash[32] = {0};
#if defined(TARGET_NANOS) || defined(TARGET_NANOS2) || defined(TARGET_NANOX) || defined(TARGET_STAX) || defined(TARGET_FLEX)
    keccak_digest(ctx->buffer, ctx->bufferLen, hash, 32);
#endif

    // now get the hex string of the hash
    char hex[65] = {0};
    array_to_hexstr(hex, 65, hash, 32);

    snprintf(outKey, outKeyLen, "Eth-Hash");

    pageString(outVal, outValLen, hex, pageIdx, pageCount);

    return parser_ok;
}

static parser_error_t printERC20(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen,
                                 char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    if (outKey == NULL || outVal == NULL || pageCount == NULL) {
        return parser_unexpected_error;
    }
    MEMZERO(outKey, outKeyLen);
    MEMZERO(outVal, outValLen);
    *pageCount = 1;

    if ((eth_tx_obj.tx_type == legacy || eth_tx_obj.tx_type == eip2930) && displayIdx >= 5) {
        displayIdx += 2;
    }

    if (eth_tx_obj.tx_type == eip1559 && displayIdx >= 7) {
        displayIdx++;
    }

    char data_array[TMP_DATA_ARRAY_SIZE] = {0};
    switch (displayIdx) {
        case 0:
            snprintf(outKey, outKeyLen, "Receiver");
            rlp_t to = {
                .kind = RLP_KIND_STRING, .ptr = (eth_tx_obj.tx.data.ptr + ERC20_TRANSFER_OFFSET), .rlpLen = ETH_ADDRESS_LEN};
            CHECK_ERROR(printEVMAddress(&to, outVal, outValLen, pageIdx, pageCount));
            break;

        case 1:
            snprintf(outKey, outKeyLen, "Contract");
            rlp_t contractAddress = {.kind = RLP_KIND_STRING, .ptr = eth_tx_obj.tx.to.ptr, .rlpLen = ETH_ADDRESS_LEN};
            CHECK_ERROR(printEVMAddress(&contractAddress, outVal, outValLen, pageIdx, pageCount));
            break;
        case 2:
            snprintf(outKey, outKeyLen, "Coin asset");
            switch (eth_tx_obj.chainId.chain_id_decoded) {
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
            break;

        case 3:
            snprintf(outKey, outKeyLen, "Amount");
            CHECK_ERROR(printERC20Value(&eth_tx_obj, outVal, outValLen, pageIdx, pageCount));
            break;

        case 4:
            snprintf(outKey, outKeyLen, "Nonce");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.nonce, outVal, outValLen, pageIdx, pageCount));
            break;

        case 5:
            snprintf(outKey, outKeyLen, "Max Priority Fee");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.max_priority_fee_per_gas, outVal, outValLen, pageIdx, pageCount));
            break;

        case 6:
            snprintf(outKey, outKeyLen, "Max Fee");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.max_fee_per_gas, outVal, outValLen, pageIdx, pageCount));
            break;

        case 7:
            snprintf(outKey, outKeyLen, "Gas price");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.gasPrice, outVal, outValLen, pageIdx, pageCount));
            break;

        case 8:
            snprintf(outKey, outKeyLen, "Gas limit");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.gasLimit, outVal, outValLen, pageIdx, pageCount));
            break;

        case 9:
            snprintf(outKey, outKeyLen, "Value");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.value, outVal, outValLen, pageIdx, pageCount));
            break;

        case 10:
            snprintf(outKey, outKeyLen, "Data");
            array_to_hexstr(
                data_array, sizeof(data_array), eth_tx_obj.tx.data.ptr,
                eth_tx_obj.tx.data.rlpLen > DATA_BYTES_TO_PRINT ? DATA_BYTES_TO_PRINT : eth_tx_obj.tx.data.rlpLen);

            if (eth_tx_obj.tx.data.rlpLen > DATA_BYTES_TO_PRINT) {
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

static parser_error_t printGeneric(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen,
                                   char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    if (outKey == NULL || outVal == NULL || pageCount == NULL) {
        return parser_unexpected_error;
    }
    MEMZERO(outKey, outKeyLen);
    MEMZERO(outVal, outValLen);
    *pageCount = 1;

    char data_array[TMP_DATA_ARRAY_SIZE] = {0};

    if ((displayIdx >= 3 && eth_tx_obj.tx.data.rlpLen == 0) || eth_tx_obj.tx.to.rlpLen == 0) {
        displayIdx += 1;
    }

    if (eth_tx_obj.tx_type == eip1559 && displayIdx >= 7) {
        displayIdx++;
    }

    if ((eth_tx_obj.tx_type == legacy || eth_tx_obj.tx_type == eip2930) && displayIdx >= 4) {
        displayIdx += 2;
    }

    switch (displayIdx) {
        case 0:
            snprintf(outKey, outKeyLen, "To");
            rlp_t contractAddress = {.kind = RLP_KIND_STRING, .ptr = eth_tx_obj.tx.to.ptr, .rlpLen = ETH_ADDRESS_LEN};
            CHECK_ERROR(printEVMAddress(&contractAddress, outVal, outValLen, pageIdx, pageCount));
            break;
        case 1:
            snprintf(outKey, outKeyLen, "Coin asset");
            switch (eth_tx_obj.chainId.chain_id_decoded) {
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
            break;
        case 2:
            snprintf(outKey, outKeyLen, "Value");
            printBigIntFixedPoint(eth_tx_obj.tx.value.ptr, eth_tx_obj.tx.value.rlpLen, outVal, outValLen, pageIdx, pageCount,
                                  COIN_AMOUNT_DECIMAL);
            break;

        case 3:
            snprintf(outKey, outKeyLen, "Data");
            array_to_hexstr(
                data_array, sizeof(data_array), eth_tx_obj.tx.data.ptr,
                eth_tx_obj.tx.data.rlpLen > DATA_BYTES_TO_PRINT ? DATA_BYTES_TO_PRINT : eth_tx_obj.tx.data.rlpLen);

            if (eth_tx_obj.tx.data.rlpLen > DATA_BYTES_TO_PRINT) {
                snprintf(data_array + (2 * DATA_BYTES_TO_PRINT), 4, "...");
            }

            pageString(outVal, outValLen, data_array, pageIdx, pageCount);
            break;

        case 4:
            snprintf(outKey, outKeyLen, "Max Priority Fee");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.max_priority_fee_per_gas, outVal, outValLen, pageIdx, pageCount));
            break;

        case 5:
            snprintf(outKey, outKeyLen, "Max Fee");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.max_fee_per_gas, outVal, outValLen, pageIdx, pageCount));
            break;

        case 6:
            snprintf(outKey, outKeyLen, "Gas limit");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.gasLimit, outVal, outValLen, pageIdx, pageCount));
            break;

        case 7:
            snprintf(outKey, outKeyLen, "Gas price");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.gasPrice, outVal, outValLen, pageIdx, pageCount));
            break;

        case 8:
            snprintf(outKey, outKeyLen, "Nonce");
            CHECK_ERROR(printRLPNumber(&eth_tx_obj.tx.nonce, outVal, outValLen, pageIdx, pageCount));
            break;

        case 9:
            CHECK_ERROR(printEthHash(ctx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount));
            break;

        default:
            return parser_display_page_out_of_range;
    }

    return parser_ok;
}

parser_error_t _getItemEth(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen, char *outVal,
                           uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    // At the moment, clear signing is available only for ERC20 and transaction with empty data
    if (validateERC20(&eth_tx_obj)) {
        return printERC20(ctx, displayIdx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount);
    } else {
        return printGeneric(ctx, displayIdx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount);
    }

    return parser_blindsign_required;
}

// returns the number of items to display on the screen.
// Note: we might need to add a transaction state object,
// Defined with one parameter for now.
parser_error_t _getNumItemsEth(uint8_t *numItems) {
    if (numItems == NULL) {
        return parser_unexpected_error;
    }
    // Verify that tx is ERC20
    if (validateERC20(&eth_tx_obj)) {
        if (eth_tx_obj.tx_type == legacy || eth_tx_obj.tx_type == eip2930) {
            *numItems = 10;
        } else {
            *numItems = 11;
        }
        return parser_ok;
    }

    if (eth_tx_obj.tx_type == legacy || eth_tx_obj.tx_type == eip2930) {
        *numItems = 6;
    } else {
        *numItems = 7;
    }

    *numItems += ((eth_tx_obj.tx.data.rlpLen != 0) ? 1 : 0) + ((eth_tx_obj.tx.to.rlpLen != 0) ? 1 : 0);

    return parser_ok;
}

parser_error_t _computeV(parser_context_t *ctx, eth_tx_t *tx_obj, unsigned int info, uint8_t *v, bool personal_msg) {
    if (ctx == NULL || tx_obj == NULL || v == NULL) {
        return parser_unexpected_error;
    }

    uint8_t parity = (info & CX_ECCINFO_PARITY_ODD) == 1;

    /// No chain id in the transaction use ETHEREUM_RECOVERY_OFFSET
    if (personal_msg) {
        *v = ETHEREUM_RECOVERY_OFFSET + parity;
        return parser_ok;
    }

    uint8_t type = eth_tx_obj.tx_type;
    if (type == eip2930 || type == eip1559) {
        *v = parity;
        return parser_ok;
    }

    // we need chainID info
    // https://github.com/LedgerHQ/ledger-live/commit/b93a421866519b80fdd8a029caea97323eceae93
    uint64_t id = 0;
    if (tx_obj->chainId.rlpLen == 0) {
        id = tx_obj->chainId.ptr[0];
    } else {
        CHECK_ERROR(be_bytes_to_u64(tx_obj->chainId.ptr, tx_obj->chainId.rlpLen, &id));
    }

    uint32_t cv = EIP155_V_BASE + parity;
    cv = saturating_add_u32(cv, (uint32_t)id * 2);
    *v = (uint8_t)cv;

    return parser_ok;
}
