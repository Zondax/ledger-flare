/*******************************************************************************
 *  (c) 2018 - 2022 Zondax AG
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
#include "parser_impl_common.h"

#include "bech32.h"
#include "crypto.h"
#include "parser_common.h"
#include "zxformat.h"
#include "zxmacros.h"

static const chain_lookup_table_t chain_lookup_table[] = {

    // Flare
    {{0x77, 0xd3, 0x07, 0x4d, 0xc5, 0x10, 0xf4, 0x3b, 0x09, 0xac, 0x5b, 0xe7, 0x7e, 0xde, 0xe2, 0x76,
      0xef, 0x3b, 0x55, 0xf0, 0x09, 0x7d, 0x50, 0x48, 0x46, 0xaa, 0x8e, 0xec, 0x61, 0x3f, 0xc6, 0x25},
     c_chain,
     'C'},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
     p_chain,
     'P'},

    // Coston2
    {{0x78, 0xdb, 0x5c, 0x30, 0xbe, 0xd0, 0x4c, 0x05, 0xce, 0x20, 0x91, 0x79, 0x81, 0x28, 0x50, 0xbb,
      0xb3, 0xfe, 0x6d, 0x46, 0xd7, 0xee, 0xf3, 0x74, 0x4d, 0x81, 0x4c, 0x0d, 0xa5, 0x55, 0x24, 0x79},
     c_chain,
     'C'},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
     p_chain,
     'P'},

    // Songbird
    {{0x55, 0xf0, 0x77, 0xed, 0x33, 0x88, 0x89, 0x8d, 0x7c, 0x52, 0xc1, 0xa1, 0x0c, 0xae, 0x70, 0xe8,
      0x34, 0x50, 0xc3, 0x34, 0x99, 0xf4, 0xeb, 0x1a, 0xe8, 0x18, 0x77, 0xb6, 0xf8, 0xfd, 0xa4, 0x02},
     c_chain,
     'C'},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
     p_chain,
     'P'},

    // Coston
    {{0xff, 0xb1, 0x19, 0xb4, 0x04, 0xc1, 0x35, 0x6b, 0x6b, 0xfd, 0xb8, 0x00, 0x45, 0xe2, 0x7b, 0xa1,
      0x3c, 0x37, 0x89, 0xb5, 0xb3, 0x68, 0x4f, 0x00, 0x1d, 0xa0, 0x71, 0xcd, 0x4e, 0x6d, 0xb0, 0x9c},
     c_chain,
     'C'},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
     p_chain,
     'P'},

};

static const uint32_t chain_lookup_len = sizeof(chain_lookup_table) / sizeof(chain_lookup_table[0]);

// Checks that there are at least SIZE bytes available in the buffer
#define CTX_CHECK_AVAIL(CTX, SIZE)                                      \
    if ((CTX) == NULL || ((CTX)->offset + (SIZE)) > (CTX)->bufferLen) { \
        return parser_unexpected_buffer_end;                            \
    }

#define CTX_CHECK_AND_ADVANCE(CTX, SIZE) \
    CTX_CHECK_AVAIL((CTX), (SIZE))       \
    (CTX)->offset += (SIZE);

#define CTX_CHECK_BUFFER(CTX)                                  \
    if ((CTX) == NULL || ((CTX)->offset > (CTX)->bufferLen)) { \
        return parser_unexpected_buffer_end;                   \
    }

parser_error_t read_u64(parser_context_t *ctx, uint64_t *result) {
    if (result == NULL) {
        return parser_unexpected_error;
    }

    CTX_CHECK_AVAIL(ctx, sizeof(uint64_t));

    *result = 0;  // Initialize result

    for (size_t i = 0; i < sizeof(uint64_t); ++i) {
        *result = (*result << 8) | ctx->buffer[i + ctx->offset];
    }
    ctx->offset += sizeof(uint64_t);

    return parser_ok;
}

parser_error_t read_u32(parser_context_t *ctx, uint32_t *result) {
    if (result == NULL) {
        return parser_unexpected_error;
    }
    CTX_CHECK_AVAIL(ctx, sizeof(uint32_t));

    *result = 0;  // Initialize result

    for (size_t i = 0; i < sizeof(uint32_t); ++i) {
        *result = (*result << 8) | ctx->buffer[i + ctx->offset];
    }
    ctx->offset += sizeof(uint32_t);

    return parser_ok;
}

parser_error_t read_u16(parser_context_t *ctx, uint16_t *result) {
    if (result == NULL) {
        return parser_unexpected_error;
    }
    CTX_CHECK_AVAIL(ctx, sizeof(uint16_t));

    *result = 0;  // Initialize result

    for (size_t i = 0; i < sizeof(uint16_t); ++i) {
        *result = (*result << 8) | ctx->buffer[i + ctx->offset];
    }

    ctx->offset += sizeof(uint16_t);
    return parser_ok;
}

parser_error_t read_u8(parser_context_t *ctx, uint8_t *result) {
    if (result == NULL) {
        return parser_unexpected_error;
    }
    CTX_CHECK_AVAIL(ctx, sizeof(uint8_t));

    *result = ctx->buffer[ctx->offset];
    ctx->offset++;

    return parser_ok;
}

parser_error_t checkAvailableBytes(parser_context_t *ctx, uint16_t buffLen) {
    CTX_CHECK_AVAIL(ctx, buffLen)
    return parser_ok;
}

parser_error_t verifyContext(parser_context_t *ctx) {
    CTX_CHECK_BUFFER(ctx)
    return parser_ok;
}

parser_error_t verifyBytes(parser_context_t *ctx, uint16_t buffLen) {
    CTX_CHECK_AVAIL(ctx, buffLen)
    CTX_CHECK_AND_ADVANCE(ctx, buffLen)
    return parser_ok;
}

parser_error_t readBytes(parser_context_t *ctx, uint8_t *buff, uint16_t buffLen) {
    CTX_CHECK_AVAIL(ctx, buffLen)
    MEMCPY(buff, (ctx->buffer + ctx->offset), buffLen);
    CTX_CHECK_AND_ADVANCE(ctx, buffLen)
    return parser_ok;
}

parser_error_t parser_get_chain_id(parser_context_t *c, parser_tx_t *v) {
    if (v == NULL) {
        return parser_unexpected_error;
    }

    v->blockchain_id = c->buffer + c->offset;
    CHECK_ERROR(verifyBytes(c, BLOCKCHAIN_ID_LEN))

    for (size_t i = 0; i < chain_lookup_len; i++) {
        if (MEMCMP(chain_lookup_table[i].blockchain_id, v->blockchain_id, BLOCKCHAIN_ID_LEN) == 0) {
            v->chain_id = chain_lookup_table[i].chain;
            return parser_ok;
        }
    }
    return parser_unexpected_chain;
}

parser_error_t parser_get_chain_alias(const uint8_t *blockchain_id, char *chain) {
    if (blockchain_id == NULL) {
        return parser_unexpected_error;
    }

    for (size_t i = 0; i < array_length(chain_lookup_table); i++) {
        if (MEMCMP(chain_lookup_table[i].blockchain_id, blockchain_id, BLOCKCHAIN_ID_LEN) == 0) {
            *chain = chain_lookup_table[i].name;
            return parser_ok;
        }
    }
    return parser_unexpected_chain;
}

parser_error_t parse_evm_inputs(parser_context_t *c, evm_inputs_t *evm) {
    if (evm == NULL) {
        return parser_unexpected_error;
    }
    evm->in_sum = 0;

    for (uint32_t i = 0; i < evm->n_ins; i++) {
        // Skip address
        CHECK_ERROR(verifyBytes(c, ADDRESS_LEN));

        // Save amount
        uint64_t amount = 0;
        CHECK_ERROR(read_u64(c, &amount));
        evm->in_sum += amount;

        // Skip assetID
        CHECK_ERROR(verifyBytes(c, ASSET_ID_LEN));

        // Skip nonce
        CHECK_ERROR(verifyBytes(c, NONCE_LEN));
    }

    return parser_ok;
}

parser_error_t parse_transferable_secp_output(parser_context_t *c, transferable_out_secp_t *outputs, bool verify_locktime) {
    if (outputs == NULL) {
        return parser_unexpected_error;
    }
    outputs->out_sum = 0;

    for (uint32_t i = 0; i < outputs->n_outs; i++) {
        // skip assetId
        CHECK_ERROR(verifyBytes(c, ASSET_ID_LEN));

        // Skip typeID
        uint32_t typeID = 0;
        CHECK_ERROR(read_u32(c, &typeID));
        if (typeID != SECP_TYPE_ID) {
            return parser_unexpected_type_id;
        }

        // Save amount to total
        uint64_t amount = 0;
        CHECK_ERROR(read_u64(c, &amount));
        outputs->out_sum += amount;

        // Skip locktime
        uint64_t locktime = 0;
        CHECK_ERROR(read_u64(c, &locktime));
        if (verify_locktime && locktime != 0) {
            return parser_unexpected_output_locked;
        }

        // Get threshold
        uint32_t threshold = 0;
        CHECK_ERROR(read_u32(c, &threshold));

        // Get number of Addresses
        uint32_t tmp_n_adresses = 0;
        CHECK_ERROR(read_u32(c, &tmp_n_adresses));

        if (threshold > tmp_n_adresses || (tmp_n_adresses == 0 && threshold != 0)) {
            return parser_unexpected_threshold;
        }

        for (uint32_t j = 0; j < tmp_n_adresses; j++) {
            verifyBytes(c, ADDRESS_LEN);
            outputs->n_addrs++;
        }
    }

    return parser_ok;
}

parser_error_t parse_evm_output(parser_context_t *c, evm_outs_t *outputs) {
    if (outputs == NULL) {
        return parser_unexpected_error;
    }
    outputs->out_sum = 0;

    for (uint32_t i = 0; i < outputs->n_outs; i++) {
        // Check address is renderable
        verifyBytes(c, ADDRESS_LEN);

        // Save amount to total
        uint64_t amount = 0;
        CHECK_ERROR(read_u64(c, &amount));
        outputs->out_sum += amount;

        // skip assetId
        CHECK_ERROR(verifyBytes(c, ASSET_ID_LEN));
    }

    return parser_ok;
}

parser_error_t parse_transferable_secp_input(parser_context_t *c, transferable_in_secp_t *inputs) {
    if (inputs == NULL) {
        return parser_unexpected_error;
    }
    inputs->in_sum = 0;

    for (uint32_t i = 0; i < inputs->n_ins; i++) {
        // skip TxID
        CHECK_ERROR(verifyBytes(c, TX_ID_LEN));

        // skip UTXOIndex
        CHECK_ERROR(verifyBytes(c, UTXOINDEX));

        // skip ASSET_ID
        CHECK_ERROR(verifyBytes(c, ASSET_ID_LEN));

        // Skip typeID
        uint32_t typeID = 0;
        CHECK_ERROR(read_u32(c, &typeID));
        if (typeID != SECP_INPUT_TYPE_ID) {
            return parser_unexpected_type_id;
        }

        // Save amount
        uint64_t amount = 0;
        CHECK_ERROR(read_u64(c, &amount));
        inputs->in_sum += amount;

        // Get Address indices
        uint32_t n_indices = 0;
        CHECK_ERROR(read_u32(c, &n_indices));

        // skip addresses
        CHECK_ERROR(verifyBytes(c, sizeof(uint32_t) * n_indices));
    }

    return parser_ok;
}

parser_error_t parse_secp_owners_output(parser_context_t *c, secp_owners_out_t *outputs) {
    if (outputs == NULL) {
        return parser_unexpected_error;
    }

    for (uint32_t i = 0; i < outputs->n_outs; i++) {
        // Skip typeID
        uint32_t typeID = 0;
        CHECK_ERROR(read_u32(c, &typeID));
        if (typeID != SECP_OWNERS_TYPE_ID) {
            return parser_unexpected_type_id;
        }

        // Skip locktime
        CHECK_ERROR(verifyBytes(c, LOCKTIME_LEN));

        // Get threshold
        uint32_t threshold = 0;
        CHECK_ERROR(read_u32(c, &threshold));

        // Get number of Addresses
        uint32_t n_addresses = 0;
        CHECK_ERROR(read_u32(c, &n_addresses));
        outputs->n_addr += n_addresses;

        if (threshold > n_addresses || (n_addresses == 0 && threshold != 0)) {
            return parser_unexpected_threshold;
        }

        // skip addresses
        outputs->addr = c->buffer + c->offset;
        CHECK_ERROR(verifyBytes(c, ADDRESS_LEN * n_addresses));
    }

    return parser_ok;
}

parser_error_t parser_go_to_next_transferable_output(parser_context_t *c) {
    if (c == NULL) {
        return parser_init_context_empty;
    }
    c->offset += ADDRESS_OFFSET;
    uint32_t n_addresses = 0;
    CHECK_ERROR(read_u32(c, &n_addresses));
    CHECK_ERROR(verifyBytes(c, ADDRESS_LEN * n_addresses));

    return parser_ok;
}

parser_error_t parser_get_secp_output_for_index(parser_context_t *out_ctx, transferable_out_secp_t secp_outs,
                                                uint8_t inner_displayIdx, uint64_t *amount, uint8_t *address,
                                                uint8_t *element_idx) {
    if (amount == NULL || address == NULL || element_idx == NULL) {
        return parser_unexpected_error;
    }
    uint64_t count = 0;
    uint32_t out_n_addr = 0;
    for (uint32_t i = 0; i < secp_outs.n_outs; i++) {
        // read amount and check if its the index we are looking for return element 0 for amount print
        CHECK_ERROR(verifyBytes(out_ctx, AMOUNT_OFFSET));
        CHECK_ERROR(read_u64(out_ctx, amount));
        if (count == inner_displayIdx) {
            *element_idx = 0;
            return parser_ok;
        }

        CHECK_ERROR(verifyBytes(out_ctx, N_ADDRESS_OFFSET));
        CHECK_ERROR(read_u32(out_ctx, &out_n_addr));
        // Go through output addresses and check if its the index we are looking for return element >0 for address print
        for (uint32_t j = 1; j <= out_n_addr; j++) {
            CHECK_ERROR(readBytes(out_ctx, address, ADDRESS_LEN));
            count++;
            if (count == inner_displayIdx) {
                *element_idx = j;
                return parser_ok;
            }
        }
        // We did not find the index in the address add one for the next amoount we are about to read
        count++;
    }
    return parser_unexpected_number_items;
}

parser_error_t parser_get_evm_output_index(parser_context_t *out_ctx, evm_outs_t evm_outs, uint8_t out_index,
                                           uint64_t *amount, uint8_t *address) {
    if (amount == NULL || address == NULL) {
        return parser_unexpected_error;
    }
    uint64_t count = 0;

    for (uint32_t i = 0; i < evm_outs.n_outs; i++) {
        CHECK_ERROR(readBytes(out_ctx, address, ADDRESS_LEN));
        CHECK_ERROR(read_u64(out_ctx, amount));
        CHECK_ERROR(verifyBytes(out_ctx, ASSET_ID_LEN));

        if (count == out_index) {
            return parser_ok;
        }
        count++;
    }
    return parser_unexpected_number_items;
}

int parser_get_renderable_outputs_number(uint64_t mask) {
    int count = 0;

    while (mask) {
        count += mask & 1;
        mask >>= 1;
    }

    return count;
}
