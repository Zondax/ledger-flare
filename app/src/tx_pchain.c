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
#include "tx_pchain.h"

#include "app_mode.h"
#include "parser_impl_common.h"
#include "parser_print_common.h"
#include "zxformat.h"
#include "zxmacros.h"

static parser_error_t parser_base_tx(parser_context_t *c, transferable_in_secp_t *inputs, transferable_out_secp_t *outputs) {
    // Get outputs
    CHECK_ERROR(read_u32(c, &outputs->n_outs));
    if (outputs->n_outs > MAX_OUTPUTS) {
        return parser_unexpected_number_items;
    }

    // Pointer to outputs
    if (outputs->n_outs > 0) {
        CHECK_ERROR(verifyContext(c));
        outputs->outs = c->buffer + c->offset;
        CHECK_ERROR(parse_transferable_secp_output(c, outputs, true));
    }

    // Get inputs
    CHECK_ERROR(read_u32(c, &inputs->n_ins));
    if (inputs->n_ins > MAX_INPUTS) {
        return parser_unexpected_number_items;
    }

    // Pointer to inputs
    if (inputs->n_ins > 0) {
        CHECK_ERROR(verifyContext(c));
        inputs->ins = c->buffer + c->offset;
        CHECK_ERROR(parse_transferable_secp_input(c, inputs));
    }

    // Get Memo Len
    uint32_t memoLen = 0;
    CHECK_ERROR(read_u32(c, &memoLen));
    if (memoLen > MAX_MEMO_LEN) {
        return parser_unexpected_number_items;
    }

    return parser_ok;
}

parser_error_t parser_handle_base_tx(parser_context_t *c, parser_tx_t *v) {
    return parser_base_tx(c, &v->tx.base_tx.base_secp_ins, &v->tx.base_tx.base_secp_outs);
}

parser_error_t parser_handle_p_export_tx(parser_context_t *c, parser_tx_t *v) {
    // Parse base tx
    CHECK_ERROR(parser_base_tx(c, &v->tx.p_export_tx.base_secp_ins, &v->tx.p_export_tx.base_secp_outs));

    // Get destination chain
    CHECK_ERROR(checkAvailableBytes(c, BLOCKCHAIN_ID_LEN));
    v->tx.p_export_tx.destination_chain = c->buffer + c->offset;
    if (!MEMCMP(PIC(v->tx.p_export_tx.destination_chain), v->blockchain_id, BLOCKCHAIN_ID_LEN)) {
        return parser_unexpected_chain;
    }
    CHECK_ERROR(verifyBytes(c, BLOCKCHAIN_ID_LEN));

    // Get number of outputs
    CHECK_ERROR(read_u32(c, &v->tx.p_export_tx.secp_outs.n_outs));
    if (v->tx.p_export_tx.secp_outs.n_outs > MAX_OUTPUTS) {
        return parser_unexpected_number_items;
    }

    // Pointer to outputs
    CHECK_ERROR(verifyContext(c));
    v->tx.p_export_tx.secp_outs.outs = c->buffer + c->offset;
    v->tx.p_export_tx.secp_outs.outs_offset = c->offset;
    CHECK_ERROR(parse_transferable_secp_output(c, &v->tx.p_export_tx.secp_outs, true));

    return parser_ok;
}

parser_error_t parser_handle_p_import_tx(parser_context_t *c, parser_tx_t *v) {
    // Parse base tx
    CHECK_ERROR(parser_base_tx(c, &v->tx.p_import_tx.base_secp_ins, &v->tx.p_import_tx.base_secp_outs));

    // Get source chain
    CHECK_ERROR(checkAvailableBytes(c, BLOCKCHAIN_ID_LEN));
    v->tx.p_import_tx.source_chain = c->buffer + c->offset;
    if (!MEMCMP(v->tx.p_import_tx.source_chain, v->blockchain_id, BLOCKCHAIN_ID_LEN)) {
        return parser_unexpected_chain;
    }
    CHECK_ERROR(verifyBytes(c, BLOCKCHAIN_ID_LEN));

    // Get number of inputs
    CHECK_ERROR(read_u32(c, &v->tx.p_import_tx.secp_ins.n_ins));
    if (v->tx.p_import_tx.secp_ins.n_ins > MAX_INPUTS) {
        return parser_unexpected_number_items;
    }

    // Pointer to inputs
    CHECK_ERROR(verifyContext(c));
    v->tx.p_import_tx.secp_ins.ins = c->buffer + c->offset;
    v->tx.p_import_tx.secp_ins.ins_offset = c->offset;
    CHECK_ERROR(parse_transferable_secp_input(c, &v->tx.p_import_tx.secp_ins));

    return parser_ok;
}
// https://build.avax.network/docs/api-reference/p-chain/txn-format#unsigned-add-permissionless-validator-tx
// https://build.avax.network/docs/api-reference/p-chain/txn-format#unsigned-add-permissionless-delegator-tx
parser_error_t parser_handle_add_permissionless_delegator_validator(parser_context_t *c, parser_tx_t *v) {
    if (v->tx_type == add_permissionless_validator_tx) {
        CHECK_ERROR(parser_base_tx(c, &v->tx.add_permissionless_validator_tx.base_secp_ins,
                                   &v->tx.add_permissionless_validator_tx.base_secp_outs));
    } else {
        CHECK_ERROR(parser_base_tx(c, &v->tx.add_permissionless_delegator_tx.base_secp_ins,
                                   &v->tx.add_permissionless_delegator_tx.base_secp_outs));
    }

    validator_t *validator = (v->tx_type == add_permissionless_validator_tx)
                                 ? &v->tx.add_permissionless_validator_tx.validator
                                 : &v->tx.add_permissionless_delegator_tx.validator;

    CHECK_ERROR(verifyContext(c));
    validator->node_id = c->buffer + c->offset;
    CHECK_ERROR(verifyBytes(c, NODE_ID_LEN));

    CHECK_ERROR(read_u64(c, &validator->start_time));

    CHECK_ERROR(read_u64(c, &validator->end_time));

    if (validator->end_time <= validator->start_time) {
        return parser_invalid_time_stamp;
    }

    CHECK_ERROR(read_u64(c, &validator->weight));

    CHECK_ERROR(verifyContext(c));
    if (v->tx_type == add_permissionless_validator_tx) {
        v->tx.add_permissionless_validator_tx.subnet_id = c->buffer + c->offset;
    } else {
        v->tx.add_permissionless_delegator_tx.subnet_id = c->buffer + c->offset;
    }
    CHECK_ERROR(verifyBytes(c, BLOCKCHAIN_ID_LEN));

    if (v->tx_type == add_permissionless_validator_tx) {
        CHECK_ERROR(read_u32(c, &v->tx.add_permissionless_validator_tx.signer.signer_type));
        if (v->tx.add_permissionless_validator_tx.signer.signer_type == PROOF_OF_POSSESSION_TYPE_ID) {
            CHECK_ERROR(verifyContext(c));
            v->tx.add_permissionless_validator_tx.signer.proof_of_possession.public_key = c->buffer + c->offset;
            CHECK_ERROR(verifyBytes(c, 48));

            CHECK_ERROR(verifyContext(c));
            v->tx.add_permissionless_validator_tx.signer.proof_of_possession.signature = c->buffer + c->offset;
            CHECK_ERROR(verifyBytes(c, 96));
        } else if (v->tx.add_permissionless_validator_tx.signer.signer_type != EMPTY_SIGNER_TYPE_ID) {
            return parser_unexpected_type;
        }
    }

    transferable_out_secp_t *stake_outs = (v->tx_type == add_permissionless_validator_tx)
                                              ? &v->tx.add_permissionless_validator_tx.stake_outs
                                              : &v->tx.add_permissionless_delegator_tx.stake_outs;

    CHECK_ERROR(read_u32(c, &stake_outs->n_outs));
    if (stake_outs->n_outs > MAX_OUTPUTS) {
        return parser_unexpected_number_items;
    }

    CHECK_ERROR(verifyContext(c));
    stake_outs->outs = c->buffer + c->offset;
    CHECK_ERROR(parse_transferable_secp_output(c, stake_outs, false));

    if (validator->weight != stake_outs->out_sum) {
        return parser_invalid_stake_amount;
    }

    if (v->tx_type == add_permissionless_validator_tx) {
        CHECK_ERROR(verifyContext(c));
        v->tx.add_permissionless_validator_tx.validator_rewards_owner.outs = c->buffer + c->offset;
        v->tx.add_permissionless_validator_tx.validator_rewards_owner.n_outs = 1;
        CHECK_ERROR(parse_secp_owners_output(c, &v->tx.add_permissionless_validator_tx.validator_rewards_owner));
    }

    secp_owners_out_t *delegator_rewards_owner = (v->tx_type == add_permissionless_validator_tx)
                                                     ? &v->tx.add_permissionless_validator_tx.delegator_rewards_owner
                                                     : &v->tx.add_permissionless_delegator_tx.delegator_rewards_owner;

    CHECK_ERROR(verifyContext(c));
    delegator_rewards_owner->outs = c->buffer + c->offset;
    delegator_rewards_owner->n_outs = 1;
    CHECK_ERROR(parse_secp_owners_output(c, delegator_rewards_owner));

    if (v->tx_type == add_permissionless_validator_tx) {
        CHECK_ERROR(read_u32(c, &v->tx.add_permissionless_validator_tx.delegation_shares));
    }

    return parser_ok;
}

parser_error_t parser_pchain(parser_context_t *c, parser_tx_t *v) {
    switch (v->tx_type) {
        case base_tx:
            return parser_handle_base_tx(c, v);
            break;
        case p_export_tx:
            return parser_handle_p_export_tx(c, v);
            break;
        case p_import_tx:
            return parser_handle_p_import_tx(c, v);
            break;
        case add_permissionless_delegator_tx:
        case add_permissionless_validator_tx:
            return parser_handle_add_permissionless_delegator_validator(c, v);
            break;
        default:
            return parser_unexpected_type;
            break;
    }

    return parser_ok;
}

parser_error_t print_p_export_tx(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen,
                                 char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    if (displayIdx == 0) {
        snprintf(outKey, outKeyLen, "Export");

        char chain = 0;
        CHECK_ERROR(parser_get_chain_alias(ctx->tx_obj->tx.p_export_tx.destination_chain, &chain));
        snprintf(outVal, outValLen, "P to %c chain", chain);
        return parser_ok;
    }

    // print ampount and addresses
    if (displayIdx <= ctx->tx_obj->tx.p_export_tx.secp_outs.n_addrs + ctx->tx_obj->tx.p_export_tx.secp_outs.n_outs) {
        // Create new context parser for outputs
        parser_context_t output_ctx = {.buffer = ctx->tx_obj->tx.p_export_tx.secp_outs.outs,
                                       .bufferLen = ctx->bufferLen - ctx->tx_obj->tx.p_export_tx.secp_outs.outs_offset + 1,
                                       .offset = 0,
                                       .tx_obj = NULL};

        uint8_t inner_displayIdx = displayIdx - 1;
        uint8_t element_idx = 0;
        uint64_t amount = 0;
        uint8_t address[ADDRESS_LEN] = {0};

        // check which output cointains the displayIdx we want
        CHECK_ERROR(parser_get_secp_output_for_index(&output_ctx, ctx->tx_obj->tx.p_export_tx.secp_outs, inner_displayIdx,
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

    if (displayIdx == ctx->tx_obj->tx.p_export_tx.secp_outs.n_addrs + ctx->tx_obj->tx.p_export_tx.secp_outs.n_outs + 1) {
        snprintf(outKey, outKeyLen, "Fee");
        if (UINT64_MAX - ctx->tx_obj->tx.p_export_tx.base_secp_outs.out_sum <
            ctx->tx_obj->tx.p_export_tx.secp_outs.out_sum) {
            // Prevent overflow
            return parser_unexpected_value;
        }
        uint64_t outs_total =
            ctx->tx_obj->tx.p_export_tx.base_secp_outs.out_sum + ctx->tx_obj->tx.p_export_tx.secp_outs.out_sum;

        if (outs_total > ctx->tx_obj->tx.p_export_tx.base_secp_ins.in_sum) {
            // Prevent underflow
            return parser_unexpected_value;
        }

        uint64_t fee = ctx->tx_obj->tx.p_export_tx.base_secp_ins.in_sum - outs_total;

        CHECK_ERROR(
            printAmount64(fee, AMOUNT_DECIMAL_PLACES, ctx->tx_obj->network_id, outVal, outValLen, pageIdx, pageCount));
        return parser_ok;
    }

    if (displayIdx == ctx->tx_obj->tx.p_export_tx.secp_outs.n_addrs + ctx->tx_obj->tx.p_export_tx.secp_outs.n_outs + 1 + 1) {
        snprintf(outKey, outKeyLen, "Hash");
        printHash(ctx, outVal, outValLen, pageIdx, pageCount);
        return parser_ok;
    }

    return parser_display_idx_out_of_range;
}

parser_error_t print_p_import_tx(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen,
                                 char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    if (displayIdx == 0) {
        snprintf(outKey, outKeyLen, "Import");

        char chain = 0;
        CHECK_ERROR(parser_get_chain_alias(ctx->tx_obj->tx.p_import_tx.source_chain, &chain));
        snprintf(outVal, outValLen, "P from %c chain", chain);
        return parser_ok;
    }

    // print ampount and addresses
    if (displayIdx <=
        ctx->tx_obj->tx.p_import_tx.base_secp_outs.n_addrs + ctx->tx_obj->tx.p_import_tx.base_secp_outs.n_outs) {
        // Create new context parser for outputs
        parser_context_t output_ctx = {
            .buffer = ctx->tx_obj->tx.p_import_tx.base_secp_outs.outs,
            .bufferLen = ctx->bufferLen - ctx->tx_obj->tx.p_import_tx.base_secp_outs.outs_offset + 1,
            .offset = 0,
            .tx_obj = NULL};

        uint8_t inner_displayIdx = displayIdx - 1;
        uint8_t element_idx = 0;
        uint64_t amount = 0;
        uint8_t address[ADDRESS_LEN] = {0};

        // check which output cointains the displayIdx we want
        CHECK_ERROR(parser_get_secp_output_for_index(&output_ctx, ctx->tx_obj->tx.p_import_tx.base_secp_outs,
                                                     inner_displayIdx, &amount, address, &element_idx));

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

    if (displayIdx ==
        ctx->tx_obj->tx.p_import_tx.base_secp_outs.n_addrs + ctx->tx_obj->tx.p_import_tx.base_secp_outs.n_outs + 1) {
        snprintf(outKey, outKeyLen, "Fee");

        uint64_t base_secp_ins_in_sum = ctx->tx_obj->tx.p_import_tx.base_secp_ins.in_sum;
        uint64_t secp_ins_in_sum = ctx->tx_obj->tx.p_import_tx.secp_ins.in_sum;
        uint64_t base_secp_outs_out_sum = ctx->tx_obj->tx.p_import_tx.base_secp_outs.out_sum;

        // Prevent overflow
        if (base_secp_ins_in_sum > UINT64_MAX - secp_ins_in_sum) {
            return parser_unexpected_value;
        }

        // Prevent underflow
        if (base_secp_ins_in_sum + secp_ins_in_sum < base_secp_outs_out_sum) {
            return parser_unexpected_value;
        }

        uint64_t fee = (base_secp_ins_in_sum + secp_ins_in_sum) - base_secp_outs_out_sum;
        CHECK_ERROR(
            printAmount64(fee, AMOUNT_DECIMAL_PLACES, ctx->tx_obj->network_id, outVal, outValLen, pageIdx, pageCount));
        return parser_ok;
    }

    if (displayIdx ==
        ctx->tx_obj->tx.p_import_tx.base_secp_outs.n_addrs + ctx->tx_obj->tx.p_import_tx.base_secp_outs.n_outs + 1 + 1) {
        snprintf(outKey, outKeyLen, "Hash");
        printHash(ctx, outVal, outValLen, pageIdx, pageCount);
        return parser_ok;
    }

    return parser_display_idx_out_of_range;
}

parser_error_t print_add_permissionless_del_val_tx(const parser_context_t *ctx, uint8_t displayIdx, char *outKey,
                                                   uint16_t outKeyLen, char *outVal, uint16_t outValLen, uint8_t pageIdx,
                                                   uint8_t *pageCount) {
    const validator_t *validator = (ctx->tx_obj->tx_type == add_permissionless_validator_tx)
                                       ? &ctx->tx_obj->tx.add_permissionless_validator_tx.validator
                                       : &ctx->tx_obj->tx.add_permissionless_delegator_tx.validator;

    switch (displayIdx) {
        case 0:
            snprintf(outKey, outKeyLen, "Validator");
            CHECK_ERROR(printNodeId(validator->node_id, outVal, outValLen, pageIdx, pageCount));
            break;
        case 1:
            snprintf(outKey, outKeyLen, "Start time");
            CHECK_ERROR(printTimestamp(validator->start_time, outVal, outValLen, pageIdx, pageCount));
            break;
        case 2:
            snprintf(outKey, outKeyLen, "End time");
            CHECK_ERROR(printTimestamp(validator->end_time, outVal, outValLen, pageIdx, pageCount));
            break;
        case 3:
            snprintf(outKey, outKeyLen, "Total stake");
            CHECK_ERROR(printAmount64(validator->weight, AMOUNT_DECIMAL_PLACES, ctx->tx_obj->network_id, outVal, outValLen,
                                      pageIdx, pageCount));
            break;
        case 4:
            if (ctx->tx_obj->tx_type == add_permissionless_validator_tx) {
                snprintf(outKey, outKeyLen, "Rewards to");
                CHECK_ERROR(printAddress(ctx->tx_obj->tx.add_permissionless_validator_tx.validator_rewards_owner.addr,
                                         ctx->tx_obj->network_id, outVal, outValLen, pageIdx, pageCount));
            } else {
                snprintf(outKey, outKeyLen, "Rewards to");
                CHECK_ERROR(printAddress(ctx->tx_obj->tx.add_permissionless_delegator_tx.delegator_rewards_owner.addr,
                                         ctx->tx_obj->network_id, outVal, outValLen, pageIdx, pageCount));
            }
            break;
        case 5:
            if (ctx->tx_obj->tx_type == add_permissionless_validator_tx) {
                snprintf(outKey, outKeyLen, "Delegate fee");
                uint32_t shares = ctx->tx_obj->tx.add_permissionless_validator_tx.delegation_shares;
                snprintf(outVal, outValLen, "%u %%", shares / 10000);
                break;
            } else {
                snprintf(outKey, outKeyLen, "Fee");
                if (UINT64_MAX - ctx->tx_obj->tx.add_permissionless_delegator_tx.base_secp_outs.out_sum <
                    ctx->tx_obj->tx.add_permissionless_delegator_tx.stake_outs.out_sum) {
                    return parser_unexpected_value;
                }
                uint64_t outs_total = ctx->tx_obj->tx.add_permissionless_delegator_tx.base_secp_outs.out_sum +
                                      ctx->tx_obj->tx.add_permissionless_delegator_tx.stake_outs.out_sum;

                if (outs_total > ctx->tx_obj->tx.add_permissionless_delegator_tx.base_secp_ins.in_sum) {
                    return parser_unexpected_value;
                }
                uint64_t fee = ctx->tx_obj->tx.add_permissionless_delegator_tx.base_secp_ins.in_sum -
                               (ctx->tx_obj->tx.add_permissionless_delegator_tx.base_secp_outs.out_sum +
                                ctx->tx_obj->tx.add_permissionless_delegator_tx.stake_outs.out_sum);
                CHECK_ERROR(printAmount64(fee, AMOUNT_DECIMAL_PLACES, ctx->tx_obj->network_id, outVal, outValLen, pageIdx,
                                          pageCount));
                break;
            }
        case 6:
            if (ctx->tx_obj->tx_type == add_permissionless_validator_tx) {
                snprintf(outKey, outKeyLen, "Fee");
                if (UINT64_MAX - ctx->tx_obj->tx.add_permissionless_validator_tx.base_secp_outs.out_sum <
                    ctx->tx_obj->tx.add_permissionless_validator_tx.stake_outs.out_sum) {
                    // Prevent overflow
                    return parser_unexpected_value;
                }
                uint64_t outs_total = ctx->tx_obj->tx.add_permissionless_validator_tx.base_secp_outs.out_sum +
                                      ctx->tx_obj->tx.add_permissionless_validator_tx.stake_outs.out_sum;

                if (outs_total > ctx->tx_obj->tx.add_permissionless_validator_tx.base_secp_ins.in_sum) {
                    // Prevent underflow
                    return parser_unexpected_value;
                }

                uint64_t fee = ctx->tx_obj->tx.add_permissionless_validator_tx.base_secp_ins.in_sum -
                               (ctx->tx_obj->tx.add_permissionless_validator_tx.base_secp_outs.out_sum +
                                ctx->tx_obj->tx.add_permissionless_validator_tx.stake_outs.out_sum);
                CHECK_ERROR(printAmount64(fee, AMOUNT_DECIMAL_PLACES, ctx->tx_obj->network_id, outVal, outValLen, pageIdx,
                                          pageCount));
            } else {
                snprintf(outKey, outKeyLen, "Hash");
                printHash(ctx, outVal, outValLen, pageIdx, pageCount);
            }
            break;
        case 7:
            if (ctx->tx_obj->tx_type == add_permissionless_validator_tx) {
                snprintf(outKey, outKeyLen, "Hash");
                printHash(ctx, outVal, outValLen, pageIdx, pageCount);
            } else {
                return parser_display_idx_out_of_range;
            }
            break;
        default:
            return parser_display_idx_out_of_range;
    }
    return parser_ok;
}

parser_error_t print_base_tx(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen, char *outVal,
                             uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount) {
    if (displayIdx == 0) {
        snprintf(outKey, outKeyLen, "Send");
        snprintf(outVal, outValLen, "P chain");
        return parser_ok;
    }

    if (displayIdx <= ctx->tx_obj->tx.base_tx.base_secp_outs.n_addrs + ctx->tx_obj->tx.base_tx.base_secp_outs.n_outs) {
        parser_context_t output_ctx = {.buffer = ctx->tx_obj->tx.base_tx.base_secp_outs.outs,
                                       .bufferLen = ctx->bufferLen - ctx->tx_obj->tx.base_tx.base_secp_outs.outs_offset + 1,
                                       .offset = 0,
                                       .tx_obj = NULL};

        uint8_t inner_displayIdx = displayIdx - 1;
        uint8_t element_idx = 0;
        uint64_t amount = 0;
        uint8_t address[ADDRESS_LEN] = {0};

        CHECK_ERROR(parser_get_secp_output_for_index(&output_ctx, ctx->tx_obj->tx.base_tx.base_secp_outs, inner_displayIdx,
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

    if (displayIdx == ctx->tx_obj->tx.base_tx.base_secp_outs.n_addrs + ctx->tx_obj->tx.base_tx.base_secp_outs.n_outs + 1) {
        snprintf(outKey, outKeyLen, "Fee");
        // Check for underflow before subtraction
        if (ctx->tx_obj->tx.base_tx.base_secp_ins.in_sum < ctx->tx_obj->tx.base_tx.base_secp_outs.out_sum) {
            return parser_unexpected_error;  // Invalid transaction: outputs exceed inputs
        }
        uint64_t fee = ctx->tx_obj->tx.base_tx.base_secp_ins.in_sum - ctx->tx_obj->tx.base_tx.base_secp_outs.out_sum;
        CHECK_ERROR(
            printAmount64(fee, AMOUNT_DECIMAL_PLACES, ctx->tx_obj->network_id, outVal, outValLen, pageIdx, pageCount));
        return parser_ok;
    }

    if (displayIdx ==
        ctx->tx_obj->tx.base_tx.base_secp_outs.n_addrs + ctx->tx_obj->tx.base_tx.base_secp_outs.n_outs + 1 + 1) {
        snprintf(outKey, outKeyLen, "Hash");
        printHash(ctx, outVal, outValLen, pageIdx, pageCount);
        return parser_ok;
    }

    return parser_display_idx_out_of_range;
}
