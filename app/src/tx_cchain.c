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
#include "tx_cchain.h"

#include "common/parser_common.h"
#include "parser_impl_common.h"
#include "parser_print_common.h"
#include "zxformat.h"
#include "zxmacros.h"

static parser_error_t parser_handle_cchain_export(parser_context_t *c, parser_tx_t *v) {
    // Get destination chain
    CHECK_ERROR(checkAvailableBytes(c, BLOCKCHAIN_ID_LEN));
    v->tx.c_export_tx.destination_chain = c->buffer + c->offset;
    if (!MEMCMP(v->tx.c_export_tx.destination_chain, v->blockchain_id, BLOCKCHAIN_ID_LEN)) {
        return parser_unexpected_chain;
    }
    CHECK_ERROR(verifyBytes(c, BLOCKCHAIN_ID_LEN));

    // Get number of inputs
    CHECK_ERROR(read_u32(c, &v->tx.c_export_tx.evm_inputs.n_ins));
    if (v->tx.c_export_tx.evm_inputs.n_ins > MAX_INPUTS || v->tx.c_export_tx.evm_inputs.n_ins == 0) {
        return parser_unexpected_number_items;
    }

    // Pointer to inputs
    CHECK_ERROR(verifyContext(c));
    v->tx.c_export_tx.evm_inputs.ins = c->buffer + c->offset;
    CHECK_ERROR(parse_evm_inputs(c, &v->tx.c_export_tx.evm_inputs));

    // Get number of outputs
    CHECK_ERROR(read_u32(c, &v->tx.c_export_tx.secp_outs.n_outs));
    if (v->tx.c_export_tx.secp_outs.n_outs > MAX_OUTPUTS) {
        return parser_unexpected_number_items;
    }

    // Pointer to outputs
    if (v->tx.c_export_tx.secp_outs.n_outs > 0) {
        CHECK_ERROR(verifyContext(c));
        v->tx.c_export_tx.secp_outs.outs = c->buffer + c->offset;
        v->tx.c_export_tx.secp_outs.outs_offset = c->offset;
        CHECK_ERROR(parse_transferable_secp_output(c, &v->tx.c_export_tx.secp_outs, false));
    }

    return parser_ok;
}

static parser_error_t parser_handle_cchain_import(parser_context_t *c, parser_tx_t *v) {
    // Get source chain
    CHECK_ERROR(checkAvailableBytes(c, BLOCKCHAIN_ID_LEN));
    v->tx.c_import_tx.source_chain = c->buffer + c->offset;
    if (!MEMCMP(v->tx.c_import_tx.source_chain, v->blockchain_id, BLOCKCHAIN_ID_LEN)) {
        return parser_unexpected_chain;
    }
    CHECK_ERROR(verifyBytes(c, BLOCKCHAIN_ID_LEN));

    // Get number of inputs
    CHECK_ERROR(read_u32(c, &v->tx.c_import_tx.secp_inputs.n_ins));
    if (v->tx.c_import_tx.secp_inputs.n_ins > MAX_INPUTS) {
        return parser_unexpected_number_items;
    }

    // Pointer to inputs
    CHECK_ERROR(verifyContext(c));
    v->tx.c_import_tx.secp_inputs.ins = c->buffer + c->offset;
    CHECK_ERROR(parse_transferable_secp_input(c, &v->tx.c_import_tx.secp_inputs));

    // Get number of outputs
    CHECK_ERROR(read_u32(c, &v->tx.c_import_tx.evm_outs.n_outs));
    if (v->tx.c_import_tx.evm_outs.n_outs > MAX_OUTPUTS) {
        return parser_unexpected_number_items;
    }

    // Pointer to outputs
    if (v->tx.c_import_tx.evm_outs.n_outs > 0) {
        CHECK_ERROR(verifyContext(c));
        v->tx.c_import_tx.evm_outs.outs = c->buffer + c->offset;
        v->tx.c_import_tx.evm_outs.outs_offset = c->offset;
        CHECK_ERROR(parse_evm_output(c, &v->tx.c_import_tx.evm_outs));
    }

    return parser_ok;
}

parser_error_t parser_cchain(parser_context_t *c, parser_tx_t *v) {
    switch (v->tx_type) {
        case c_export_tx:
            return parser_handle_cchain_export(c, v);
            break;
        case c_import_tx:
            return parser_handle_cchain_import(c, v);
            break;
        default:
            return parser_unexpected_type;
            break;
    }

    return parser_ok;
}

parser_error_t print_c_export_tx(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen,
                                 char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    if (displayIdx == 0) {
        snprintf(outKey, outKeyLen, "Export");
        char chain = 0;
        CHECK_ERROR(parser_get_chain_alias(ctx->tx_obj->tx.c_export_tx.destination_chain, &chain));
        snprintf(outVal, outValLen, "C to %c chain", chain);
        return parser_ok;
    }

    // print ampount and addresses
    if (displayIdx <= ctx->tx_obj->tx.c_export_tx.secp_outs.n_addrs + ctx->tx_obj->tx.c_export_tx.secp_outs.n_outs) {
        parser_context_t output_ctx = {.buffer = ctx->tx_obj->tx.c_export_tx.secp_outs.outs,
                                       .bufferLen = ctx->bufferLen - ctx->tx_obj->tx.c_export_tx.secp_outs.outs_offset + 1,
                                       .offset = 0,
                                       .tx_obj = NULL};

        uint8_t inner_displayIdx = displayIdx - 1;
        uint8_t element_idx = 0;
        uint64_t amount = 0;
        uint8_t address[ADDRESS_LEN] = {0};

        // check which output cointains the displayIdx we want
        CHECK_ERROR(parser_get_secp_output_for_index(&output_ctx, ctx->tx_obj->tx.c_export_tx.secp_outs, inner_displayIdx,
                                                     &amount, address, &element_idx));
        if (!element_idx) {
            snprintf(outKey, outKeyLen, "Amount");
            CHECK_ERROR(printAmount64(amount, AMOUNT_DECIMAL_PLACES, ctx->tx_obj->network_id, outVal, outValLen, pageIdx,
                                      pageCount));
        } else {
            snprintf(outKey, outKeyLen, "Address");
            CHECK_ERROR(printAddress(address, ctx->tx_obj->network_id, outVal, outValLen, pageIdx, pageCount));
        }
        return parser_ok;
    }

    if (displayIdx == ctx->tx_obj->tx.c_export_tx.secp_outs.n_addrs + ctx->tx_obj->tx.c_export_tx.secp_outs.n_outs + 1) {
        snprintf(outKey, outKeyLen, "Fee");
        if (ctx->tx_obj->tx.c_export_tx.secp_outs.out_sum > ctx->tx_obj->tx.c_export_tx.evm_inputs.in_sum) {
            // Prevent underflow
            return parser_unexpected_value;
        }
        uint64_t fee = ctx->tx_obj->tx.c_export_tx.evm_inputs.in_sum - ctx->tx_obj->tx.c_export_tx.secp_outs.out_sum;
        CHECK_ERROR(
            printAmount64(fee, AMOUNT_DECIMAL_PLACES, ctx->tx_obj->network_id, outVal, outValLen, pageIdx, pageCount));
        return parser_ok;
    }

    if (displayIdx == ctx->tx_obj->tx.c_export_tx.secp_outs.n_addrs + ctx->tx_obj->tx.c_export_tx.secp_outs.n_outs + 1 + 1) {
        snprintf(outKey, outKeyLen, "Hash");
        printHash(ctx, outVal, outValLen, pageIdx, pageCount);
        return parser_ok;
    }

    return parser_display_idx_out_of_range;
}

parser_error_t print_c_import_tx(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen,
                                 char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    if (displayIdx == 0) {
        snprintf(outKey, outKeyLen, "Import");
        char chain = 0;
        CHECK_ERROR(parser_get_chain_alias(ctx->tx_obj->tx.c_import_tx.source_chain, &chain));
        snprintf(outVal, outValLen, "C from %c chain", chain);
        return parser_ok;
    }

    // print ampount and addresses
    if (displayIdx <= 2 * (ctx->tx_obj->tx.c_import_tx.evm_outs.n_outs)) {
        parser_context_t output_ctx = {.buffer = ctx->tx_obj->tx.c_import_tx.evm_outs.outs,
                                       .bufferLen = ctx->bufferLen - ctx->tx_obj->tx.c_import_tx.evm_outs.outs_offset + 1,
                                       .offset = 0,
                                       .tx_obj = NULL};

        // Get output index for
        uint8_t out_index = (displayIdx - 1) / 2;
        uint64_t amount = 0;
        uint8_t address[ADDRESS_LEN] = {0};

        // check which output cointains the displayIdx we want
        CHECK_ERROR(
            parser_get_evm_output_index(&output_ctx, ctx->tx_obj->tx.c_import_tx.evm_outs, out_index, &amount, address));
        if ((displayIdx - 1) % 2 == 0) {
            snprintf(outKey, outKeyLen, "Amount");
            CHECK_ERROR(printAmount64(amount, AMOUNT_DECIMAL_PLACES, ctx->tx_obj->network_id, outVal, outValLen, pageIdx,
                                      pageCount));
        } else {
            snprintf(outKey, outKeyLen, "Address");
            char tmp_buffer[100] = {0};
            tmp_buffer[0] = '0';
            tmp_buffer[1] = 'x';
            if (array_to_hexstr(tmp_buffer + 2, sizeof(tmp_buffer) - 2, address, ADDRESS_LEN) == 0) {
                return parser_unexpected_data_len;
            }
            pageString(outVal, outValLen, tmp_buffer, pageIdx, pageCount);
        }
        return parser_ok;
    }

    if (displayIdx == (2 * ctx->tx_obj->tx.c_import_tx.evm_outs.n_outs) + 1) {
        snprintf(outKey, outKeyLen, "Fee");
        if (ctx->tx_obj->tx.c_import_tx.secp_inputs.in_sum < ctx->tx_obj->tx.c_import_tx.evm_outs.out_sum) {
            // Prevent underflow
            return parser_unexpected_value;
        }
        uint64_t fee = ctx->tx_obj->tx.c_import_tx.secp_inputs.in_sum - ctx->tx_obj->tx.c_import_tx.evm_outs.out_sum;
        CHECK_ERROR(
            printAmount64(fee, AMOUNT_DECIMAL_PLACES, ctx->tx_obj->network_id, outVal, outValLen, pageIdx, pageCount));
        return parser_ok;
    }

    if (displayIdx == (2 * ctx->tx_obj->tx.c_import_tx.evm_outs.n_outs) + 1 + 1) {
        snprintf(outKey, outKeyLen, "Hash");
        printHash(ctx, outVal, outValLen, pageIdx, pageCount);
        return parser_ok;
    }

    return parser_display_idx_out_of_range;
}
