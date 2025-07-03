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
#pragma once
#include "parser_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CHECK_CX_PARSER_OK(CALL)            \
    do {                                    \
        cx_err_t __cx_err = CALL;           \
        if (__cx_err != CX_OK) {            \
            return parser_unexpected_error; \
        }                                   \
    } while (0)

parser_error_t printAmount64(uint64_t amount, uint8_t amountDenom, network_id_e network_id, char *outVal, uint16_t outValLen,
                             uint8_t pageIdx, uint8_t *pageCount);
parser_error_t printAddress(const uint8_t *pubkey, network_id_e network_id, char *outVal, uint16_t outValLen,
                            uint8_t pageIdx, uint8_t *pageCount);

parser_error_t printTimestamp(uint64_t timestamp, char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount);

parser_error_t printNodeId(const uint8_t *nodeId, char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount);

parser_error_t printHash(const parser_context_t *ctx, char *outVal, uint16_t outValLen, uint8_t pageIdx, uint8_t *pageCount);

#ifdef __cplusplus
}

#endif
