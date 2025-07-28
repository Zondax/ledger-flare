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

#include "parser_impl.h"

#include "app_mode.h"
#include "parser_impl_common.h"
#include "tx_cchain.h"
#include "tx_pchain.h"
#include "zxmacros.h"

const char *parser_getErrorDescription(parser_error_t err) {
    switch (err) {
        case parser_ok:
            return "No error";
        case parser_no_data:
            return "No more data";
        case parser_init_context_empty:
            return "Initialized empty context";
        case parser_unexpected_buffer_end:
            return "Unexpected buffer end";
        case parser_unexpected_version:
            return "Unexpected version";
        case parser_unexpected_characters:
            return "Unexpected characters";
        case parser_unexpected_field:
            return "Unexpected field";
        case parser_duplicated_field:
            return "Unexpected duplicated field";
        case parser_value_out_of_range:
            return "Value out of range";
        case parser_unexpected_chain:
            return "Unexpected chain";
        case parser_missing_field:
            return "missing field";
        case parser_display_idx_out_of_range:
            return "display index out of range";
        case parser_display_page_out_of_range:
            return "display page out of range";

        /* Generic errors */
        case parser_unexpected_error:
            return "Unexpected error";

        /* Coin generic */
        case parser_unexpected_type:
            return "Unexpected type";
        case parser_unexpected_method:
            return "Unexpected method";
        case parser_unexpected_value:
            return "Unexpected value";
        case parser_unexpected_number_items:
            return "Unexpected number of items";
        case parser_invalid_address:
            return "Invalid address";
        case parser_unknown_transaction:
            return "Unknown transaction";

        /* Utils specific */
        case parser_unexpected_data_len:
            return "Unexpected data length";
        case parser_invalid_codec:
            return "Invalid codec";
        case parser_unexpected_network:
            return "Unexpected network";
        case parser_unexpected_type_id:
            return "Unexpected type id";
        case parser_unexpected_threshold:
            return "Unexpected threshold";
        case parser_unexpected_n_address_zero:
            return "Unexpected n_address zero";
        case parser_unexpected_unparsed_bytes:
            return "Unexpected unparsed bytes";
        case parser_invalid_time_stamp:
            return "Invalid time stamp";
        case parser_invalid_stake_amount:
            return "Invalid stake amount";
        case parser_unexpected_output_locked:
            return "Unexpected output locked";
        case parser_unsupported_tx:
            return "Unsupported transaction";
        case parser_blindsign_mode_required:
            return "Blind sign mode required";

        case parser_invalid_rs_values:
            return "Invalid RS values";
        case parser_invalid_chain_id:
            return "Invalid chain id";

        default:
            return "Unrecognized error code";
    }
}

static parser_error_t parser_map_tx_type(parser_context_t *c, parser_tx_t *v) {
    if (v == NULL) {
        return parser_init_context_empty;
    }

    uint32_t raw_tx_type = 0;
    CHECK_ERROR(read_u32(c, &raw_tx_type))

    switch (raw_tx_type) {
        case BASE_TX:
            v->tx_type = base_tx;
            break;
        case P_CHAIN_EXPORT_TX:
            v->tx_type = p_export_tx;
            break;
        case P_CHAIN_IMPORT_TX:
            v->tx_type = p_import_tx;
            break;
        case C_CHAIN_EXPORT_TX:
            v->tx_type = c_export_tx;
            break;
        case C_CHAIN_IMPORT_TX:
            v->tx_type = c_import_tx;
            break;
        case ADD_PERMISSIONLESS_DELEGATOR_TX:
            v->tx_type = add_permissionless_delegator_tx;
            break;
        case ADD_PERMISSIONLESS_VALIDATOR_TX:
            v->tx_type = add_permissionless_validator_tx;
            break;
        default:
            return parser_unknown_transaction;
            break;
    }
    return parser_ok;
}

static parser_error_t parser_get_network_id(parser_context_t *c, parser_tx_t *v) {
    if (v == NULL) {
        return parser_init_context_empty;
    }

    uint32_t netword_id = 0;
    CHECK_ERROR(read_u32(c, &netword_id))

    switch (netword_id) {
        case MAINNET_ID:
            v->network_id = mainnet;
            break;
        case COSTON_ID:
            v->network_id = coston;
            break;
        case COSTON2_ID:
            v->network_id = coston2;
            break;
        case SONGBIRD_ID:
            v->network_id = songbird;
            break;
        default:
            return parser_unexpected_network;
    }
    return parser_ok;
}

static parser_error_t parser_verify_codec(parser_context_t *ctx) {
    uint16_t codec = 0;
    CHECK_ERROR(read_u16(ctx, &codec));
    if (codec != 0) {
        return parser_invalid_codec;
    }
    return parser_ok;
}

parser_error_t _read(parser_context_t *ctx, parser_tx_t *v) {
    if (ctx == NULL || v == NULL) {
        return parser_init_context_empty;
    }

    CHECK_ERROR(parser_verify_codec(ctx))

    // Read Tx type raw value
    CHECK_ERROR(parser_map_tx_type(ctx, v));

    // Get Network Id and Chain ID
    CHECK_ERROR(parser_get_network_id(ctx, v));
    CHECK_ERROR(parser_get_chain_id(ctx, v));

    if (v->chain_id == c_chain) {
        CHECK_ERROR(parser_cchain(ctx, v));
    } else {
        CHECK_ERROR(parser_pchain(ctx, v));
    }

    if (ctx->offset != ctx->bufferLen) {
        return parser_unexpected_unparsed_bytes;
    }

    return parser_ok;
}

parser_error_t getNumItems(const parser_context_t *ctx, uint8_t *numItems) {
    *numItems = 0;
    const uint8_t expertModeHashField = app_mode_expert() ? 1 : 0;
    switch (ctx->tx_obj->tx_type) {
        case base_tx:
            // Tx + fee + Amounts(= n_outs) + Addresses
            *numItems = 2 + ctx->tx_obj->tx.base_tx.base_secp_outs.n_addrs + ctx->tx_obj->tx.base_tx.base_secp_outs.n_outs +
                        expertModeHashField;
            break;
        case p_export_tx:
            // Tx + fee + Amounts(= n_outs) + Addresses
            *numItems = 2 + ctx->tx_obj->tx.p_export_tx.secp_outs.n_addrs + ctx->tx_obj->tx.p_export_tx.secp_outs.n_outs +
                        expertModeHashField;
            break;
        case p_import_tx:
            // Tx + fee + Amounts(= n_outs) + Addresses
            *numItems = 2 + ctx->tx_obj->tx.p_import_tx.base_secp_outs.n_addrs +
                        ctx->tx_obj->tx.p_import_tx.base_secp_outs.n_outs + expertModeHashField;
            break;
        case c_export_tx:
            // Tx + fee + Amounts(= n_outs) + Addresses
            *numItems = 2 + ctx->tx_obj->tx.c_export_tx.secp_outs.n_addrs + ctx->tx_obj->tx.c_export_tx.secp_outs.n_outs +
                        expertModeHashField;
            break;
        case c_import_tx:
            // Tx + fee + (amount + address) * n_outs
            *numItems = 2 + (2 * ctx->tx_obj->tx.c_import_tx.evm_outs.n_outs) + expertModeHashField;
            break;
        case add_permissionless_delegator_tx:
            *numItems = 6 + expertModeHashField;
            break;
        case add_permissionless_validator_tx:
            *numItems = 7 + expertModeHashField;
            break;
        default:
            break;
    }

    if (*numItems == 0) {
        return parser_unexpected_number_items;
    }
    return parser_ok;
}
