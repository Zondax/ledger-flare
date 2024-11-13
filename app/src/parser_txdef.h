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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define BLOCKCHAIN_ID_LEN 32
#define ASSET_ID_LEN 32
#define NONCE_LEN 8
#define AMOUNT_LEN 8
#define NODE_ID_LEN 20
#define NODE_ID_MAX_SIZE 41
#define CB58_CHECKSUM_LEN 4
#define TX_ID_LEN 32
#define ADDRESS_LEN 20
#define TYPE_ID_LEN 4
#define LOCKTIME_LEN 8
#define THRESHOLD_LEN 4
#define SECP_TYPE_ID 0x7
#define SECP_INPUT_TYPE_ID 0x5
#define SECP_OWNERS_TYPE_ID 0xb
#define EVM_INPUT_LEN 2068
#define UTXOINDEX 4
#define MAX_MEMO_LEN 256
#define SHARES_DIVISON_BASE 10000

#define AMOUNT_OFFSET ASSET_ID_LEN + TYPE_ID_LEN
#define N_ADDRESS_OFFSET LOCKTIME_LEN + THRESHOLD_LEN
#define ADDRESS_OFFSET N_ADDRESS_OFFSET + 4

// Transaction types
#define C_CHAIN_IMPORT_TX 0x00000000
#define P_CHAIN_EXPORT_TX 0x00000012
#define P_CHAIN_IMPORT_TX 0x00000011
#define C_CHAIN_EXPORT_TX 0x00000001
#define ADD_DELEGATOR_TX 0x0000000e
#define ADD_VALIDATOR_TX 0x0000000c

#define MAINNET_ID 14
#define COSTON_ID 7
#define COSTON2_ID 114
#define SONGBIRD_ID 5

#define MAX_OUTPUTS 64
#define MAX_INPUTS 64
#define MAX_MEMO_SIZE 256

#define AMOUNT_DECIMAL_PLACES 9

// Header and tx identification realted structures
typedef enum {
    mainnet = 0,
    songbird,
    coston,
    coston2,
} network_id_e;

typedef enum {
    p_chain = 0,  // platform chain that accommodates staking
    c_chain,      // contract chain that is used for smart contracts
} chain_id_e;
typedef enum {
    p_import_tx = 0,
    p_export_tx,
    c_import_tx,
    c_export_tx,
    add_validator_tx,
    add_delegator_tx,
} tx_type_e;

typedef struct {
    uint8_t blockchain_id[BLOCKCHAIN_ID_LEN];
    chain_id_e chain;
    char name;
} chain_lookup_table_t;

// Transactions body structures
typedef struct {
    uint32_t n_ins;
    const uint8_t *ins;
    uint16_t ins_offset;
    uint64_t in_sum;
} transferable_in_secp_t;

typedef struct {
    uint32_t n_outs;
    const uint8_t *outs;
    uint16_t outs_offset;
    uint64_t out_sum;
    uint32_t n_addrs;
} transferable_out_secp_t;

typedef struct {
    uint32_t n_outs;
    const uint8_t *outs;
    uint32_t n_addr;
    const uint8_t *addr;
} secp_owners_out_t;

typedef struct {
    uint32_t n_ins;
    const uint8_t *ins;
    uint64_t in_sum;

    const uint8_t *addr;
    const uint8_t *amount;
    const uint8_t *asset_id;
    uint64_t nonce;
} evm_inputs_t;

typedef struct {
    uint32_t n_outs;
    const uint8_t *outs;
    uint16_t outs_offset;
    uint64_t out_sum;
} evm_outs_t;

typedef struct {
    transferable_out_secp_t base_secp_outs;
    transferable_in_secp_t base_secp_ins;
    const uint8_t *source_chain;
    transferable_in_secp_t secp_ins;
} p_import_tx_t;

typedef struct {
    transferable_out_secp_t base_secp_outs;
    transferable_in_secp_t base_secp_ins;
    const uint8_t *destination_chain;
    transferable_out_secp_t secp_outs;
} p_export_tx_t;

typedef struct {
    const uint8_t *source_chain;
    transferable_in_secp_t secp_inputs;
    evm_outs_t evm_outs;
} c_import_tx_t;

typedef struct {
    const uint8_t *destination_chain;
    evm_inputs_t evm_inputs;
    transferable_out_secp_t secp_outs;
} c_export_tx_t;

typedef struct {
    transferable_out_secp_t base_secp_outs;
    transferable_in_secp_t base_secp_ins;
    const uint8_t *node_id;
    uint64_t start_time;
    uint64_t end_time;
    uint64_t weigth;
    transferable_out_secp_t staked_outs;
    secp_owners_out_t owners_out;
    uint32_t shares;
} p_add_del_val_tx;

// Transactions union and common parameters
typedef union tx_command {
    p_import_tx_t p_import_tx;
    p_export_tx_t p_export_tx;
    c_import_tx_t c_import_tx;
    c_export_tx_t c_export_tx;
    p_add_del_val_tx add_del_val_tx;
} tx_t;

typedef struct {
    // tx identificaiton
    tx_type_e tx_type;
    network_id_e network_id;
    const uint8_t *blockchain_id;
    chain_id_e chain_id;

    // transactions specific
    tx_t tx;
} parser_tx_t;

#ifdef __cplusplus
}
#endif
