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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "parser_common.h"
#include "rlp.h"

#define ETH_ADDRESS_LEN 20
typedef struct {
    uint8_t addr[ETH_ADDRESS_LEN];
} eth_addr_t;

// Type that holds the common fields
// for legacy and eip2930 transactions
typedef struct {
    rlp_t nonce;
    rlp_t gasPrice;
    rlp_t gasLimit;
    rlp_t to;
    rlp_t value;
    rlp_t data;
} eth_base_t;

// EIP 2718 TransactionType
// Valid transaction types should be in [0x00, 0x7f]
typedef enum {
    eip2930 = 0x01,
    eip1559 = 0x02,
    // Legacy tx type is greater than or equal to 0xc0.
    legacy = 0xc0
} eth_tx_type_e;

typedef struct {
    eth_tx_type_e tx_type;
    rlp_t chainId;
    // lets use an anonymous
    // union to hold the 3 possible types of transactions:
    // legacy, eip2930, eip1559
    union {
        eth_base_t legacy;
    };

} eth_tx_t;

extern eth_tx_t eth_tx_obj;

parser_error_t _readEth(parser_context_t *ctx, eth_tx_t *eth_tx_obj);

parser_error_t _getItemEth(const parser_context_t *ctx, uint8_t displayIdx, char *outKey, uint16_t outKeyLen, char *outVal,
                           uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount);

// returns the number of items to display on the screen.
// Note: we might need to add a transaction state object,
// Defined with one parameter for now.
parser_error_t _getNumItemsEth(uint8_t *numItems);

parser_error_t _validateTxEth();

// parser_error_t _computeV(unsigned int info, uint8_t *v);
parser_error_t _computeV(parser_context_t *ctx, eth_tx_t *tx_obj, unsigned int info, uint8_t *v);

#ifdef __cplusplus
}
#endif
