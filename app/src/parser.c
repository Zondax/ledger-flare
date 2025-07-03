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

#include "parser.h"

#include <bech32.h>
#include <stdio.h>
#include <zxformat.h>
#include <zxmacros.h>
#include <zxtypes.h>

#include "apdu_codes.h"
#include "app_mode.h"
#include "coin.h"
#include "crypto.h"
#include "crypto_helper.h"
#include "parser_impl_common.h"
#include "tx_cchain.h"
#include "tx_pchain.h"

parser_error_t parser_init_context(parser_context_t *ctx, const uint8_t *buffer, uint16_t bufferSize) {
    ctx->offset = 0;
    ctx->buffer = NULL;
    ctx->bufferLen = 0;

    if (bufferSize == 0 || buffer == NULL) {
        // Not available, use defaults
        return parser_init_context_empty;
    }

    ctx->buffer = buffer;
    ctx->bufferLen = bufferSize;
    return parser_ok;
}

parser_error_t parser_parse(parser_context_t *ctx, const uint8_t *data, size_t dataLen, parser_tx_t *tx_obj) {
    CHECK_ERROR(parser_init_context(ctx, data, dataLen))
    ctx->tx_obj = tx_obj;
    app_mode_skip_blindsign_ui();
    return _read(ctx, tx_obj);
}

parser_error_t parser_validate(parser_context_t *ctx) {
    // Iterate through all items to check that all can be shown and are valid
    uint8_t numItems = 0;
    CHECK_ERROR(parser_getNumItems(ctx, &numItems))

    char tmpKey[40] = {0};
    char tmpVal[40] = {0};

    for (uint8_t idx = 0; idx < numItems; idx++) {
        uint8_t pageCount = 0;
        CHECK_ERROR(parser_getItem(ctx, idx, tmpKey, sizeof(tmpKey), tmpVal, sizeof(tmpVal), 0, &pageCount))
    }
    return parser_ok;
}

parser_error_t parser_getNumItems(const parser_context_t *ctx, uint8_t *num_items) {
    CHECK_ERROR(getNumItems(ctx, num_items));
    return parser_ok;
}

parser_error_t cleanOutput(char *outKey, uint16_t outKeyLen, char *outVal, uint16_t outValLen) {
    if (outKey == NULL || outVal == NULL || outKeyLen == 0 || outValLen == 0) {
        return parser_no_data;
    };

    MEMZERO(outKey, outKeyLen);
    MEMZERO(outVal, outValLen);
    snprintf(outKey, outKeyLen, "?");
    snprintf(outVal, outValLen, " ");

    return parser_ok;
}

parser_error_t checkSanity(uint8_t numItems, uint8_t displayIdx) {
    if (displayIdx >= numItems) {
        return parser_display_idx_out_of_range;
    }
    return parser_ok;
}

parser_error_t _getItemFlr(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen, char *outVal,
                           uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    UNUSED(pageIdx);
    *pageCount = 1;
    uint8_t numItems = 0;
    CHECK_ERROR(parser_getNumItems(ctx, &numItems))
    CHECK_APP_CANARY()

    CHECK_ERROR(checkSanity(numItems, displayIdx))
    CHECK_ERROR(cleanOutput(outKey, outKeyLen, outVal, outValLen));

    switch (ctx->tx_obj->tx_type) {
        case base_tx:
            return print_base_tx(ctx, displayIdx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount);
        case p_export_tx:
            return print_p_export_tx(ctx, displayIdx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount);
        case p_import_tx:
            return print_p_import_tx(ctx, displayIdx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount);
        case c_export_tx:
            return print_c_export_tx(ctx, displayIdx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount);
        case c_import_tx:
            return print_c_import_tx(ctx, displayIdx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount);
        case add_permissionless_delegator_tx:
        case add_permissionless_validator_tx:
            return print_add_permissionless_del_val_tx(ctx, displayIdx, outKey, outKeyLen, outVal, outValLen, pageIdx,
                                                       pageCount);
        default:
            break;
    }

    return parser_display_idx_out_of_range;
}

parser_error_t parser_getItem(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen,
                              char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    return _getItemFlr(ctx, displayIdx, outKey, outKeyLen, outVal, outValLen, pageIdx, pageCount);
}
