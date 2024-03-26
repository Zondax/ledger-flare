/*******************************************************************************
 *   (c) 2018 - 2023 Zondax AG
 *   (c) 2016 Ledger
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

#include <os.h>
#include <os_io_seproxyhal.h>
#include <string.h>
#include <ux.h>

#include "actions.h"
#include "addr.h"
#include "app_main.h"
#include "app_mode.h"
#include "coin.h"
#include "crypto.h"
#include "crypto_helper.h"
#include "eth_addr.h"
#include "eth_utils.h"
#include "hash.h"
#include "tx.h"
#include "view.h"
#include "view_internal.h"
#include "zxmacros.h"

static const char *msg_error1 = "Expert Mode";
static const char *msg_error2 = "Required";

static bool tx_initialized = false;

void extractHDPath(uint32_t rx, uint32_t offset) {
    tx_initialized = false;

    if ((rx - offset) < sizeof(uint32_t) * HDPATH_LEN_DEFAULT) {
        THROW(APDU_CODE_WRONG_LENGTH);
    }

    memcpy(hdPath, G_io_apdu_buffer + offset, sizeof(uint32_t) * HDPATH_LEN_DEFAULT);

    if (hdPath[0] != HDPATH_ETH_0_DEFAULT || (hdPath[1] != HDPATH_ETH_1_DEFAULT)) {
        THROW(APDU_CODE_DATA_INVALID);
    }
    hdPath_len = HDPATH_LEN_DEFAULT;
}

uint8_t extractHRP(uint32_t rx, uint32_t offset) {
    if (rx < offset + 1) {
        THROW(APDU_CODE_DATA_INVALID);
    }
    MEMZERO(bech32_hrp, MAX_BECH32_HRP_LEN);

    bech32_hrp_len = G_io_apdu_buffer[offset];

    if (bech32_hrp_len == 0 || bech32_hrp_len > MAX_BECH32_HRP_LEN) {
        THROW(APDU_CODE_DATA_INVALID);
    }

    memcpy(bech32_hrp, G_io_apdu_buffer + offset + 1, bech32_hrp_len);
    bech32_hrp[bech32_hrp_len] = 0;  // zero terminate

    return bech32_hrp_len;
}

void extract_eth_path(uint32_t rx, uint32_t offset) {
    tx_initialized = false;

    uint32_t path_len = *(G_io_apdu_buffer + offset);

    if (path_len > MAX_BIP32_PATH || path_len < 1) THROW(APDU_CODE_WRONG_LENGTH);

    if ((rx - offset - 1) < sizeof(uint32_t) * path_len) {
        THROW(APDU_CODE_WRONG_LENGTH);
    }

    // first byte at OFFSET_DATA is the path len, so we skip this
    uint8_t *path_data = G_io_apdu_buffer + offset + 1;

    // hw-app-eth serializes path as BE numbers
    for (uint8_t i = 0; i < path_len; i++) {
        hdPath[i] = U4BE(path_data, 0);
        path_data += sizeof(uint32_t);
    }

    const bool mainnet = hdPath[0] == HDPATH_ETH_0_DEFAULT && hdPath[1] == HDPATH_ETH_1_DEFAULT;

    if (!mainnet) {
        THROW(APDU_CODE_DATA_INVALID);
    }

    // set the hdPath len
    hdPath_len = path_len;
}

__Z_INLINE bool process_chunk(__Z_UNUSED volatile uint32_t *tx, uint32_t rx) {
    const uint8_t payloadType = G_io_apdu_buffer[OFFSET_PAYLOAD_TYPE];
    if (rx < OFFSET_DATA) {
        THROW(APDU_CODE_WRONG_LENGTH);
    }

    uint32_t added;
    switch (payloadType) {
        case P1_INIT:
            tx_initialize_flr();
            tx_reset();
            extractHDPath(rx, OFFSET_DATA);
            tx_initialized = true;
            return false;
        case P1_ADD:
            if (!tx_initialized) {
                THROW(APDU_CODE_TX_NOT_INITIALIZED);
            }
            added = tx_append(&(G_io_apdu_buffer[OFFSET_DATA]), rx - OFFSET_DATA);
            if (added != rx - OFFSET_DATA) {
                tx_initialized = false;
                THROW(APDU_CODE_OUTPUT_BUFFER_TOO_SMALL);
            }
            return false;
        case P1_LAST:
            if (!tx_initialized) {
                THROW(APDU_CODE_TX_NOT_INITIALIZED);
            }
            added = tx_append(&(G_io_apdu_buffer[OFFSET_DATA]), rx - OFFSET_DATA);
            tx_initialized = false;
            if (added != rx - OFFSET_DATA) {
                tx_initialized = false;
                THROW(APDU_CODE_OUTPUT_BUFFER_TOO_SMALL);
            }
            tx_initialized = false;
            return true;
    }

    THROW(APDU_CODE_INVALIDP1P2);
}

__Z_INLINE bool process_chunk_eth(__Z_UNUSED volatile uint32_t *tx, uint32_t rx) {
    const uint8_t payloadType = G_io_apdu_buffer[OFFSET_PAYLOAD_TYPE];

    if (G_io_apdu_buffer[OFFSET_P2] != 0) {
        THROW(APDU_CODE_INVALIDP1P2);
    }

    if (rx < OFFSET_DATA) {
        THROW(APDU_CODE_WRONG_LENGTH);
    }

    uint64_t read = 0;
    uint64_t to_read = 0;
    uint64_t max_len = 0;

    uint8_t *data = &(G_io_apdu_buffer[OFFSET_DATA]);
    uint32_t len = rx - OFFSET_DATA;

    uint64_t added;
    switch (payloadType) {
        case P1_ETH_FIRST:
            tx_initialize_eth();
            tx_reset();
            extract_eth_path(rx, OFFSET_DATA);
            // there is not warranties that the first chunk
            // contains the serialized path only;
            // so we need to offset the data to point to the first transaction
            // byte
            uint32_t path_len = sizeof(uint32_t) * hdPath_len;

            // plus the first offset data containing the path len
            data += path_len + 1;
            if (len < path_len) {
                THROW(APDU_CODE_WRONG_LENGTH);
            }

            // now process the chunk
            len -= path_len + 1;
            if (get_tx_rlp_len(data, len, &read, &to_read) != rlp_ok) {
                THROW(APDU_CODE_DATA_INVALID);
            }

            // get remaining data len
            max_len = saturating_add(read, to_read);
            max_len = MIN(max_len, len);

            added = tx_append(data, max_len);
            if (added != max_len) {
                THROW(APDU_CODE_OUTPUT_BUFFER_TOO_SMALL);
            }

            tx_initialized = true;

            // if the number of bytes read and the number of bytes to read
            //  is the same as what we read...
            if ((saturating_add(read, to_read) - len) == 0) {
                return true;
            }
            return false;
        case P1_ETH_MORE:
            if (!tx_initialized) {
                THROW(APDU_CODE_TX_NOT_INITIALIZED);
            }

            uint64_t buff_len = tx_get_buffer_length();
            uint8_t *buff_data = tx_get_buffer();

            if (get_tx_rlp_len(buff_data, buff_len, &read, &to_read) != rlp_ok) {
                THROW(APDU_CODE_DATA_INVALID);
            }

            uint64_t rlp_read = buff_len - read;

            // either the entire buffer of the remaining bytes we expect
            uint64_t missing = to_read - rlp_read;
            max_len = len;

            if (missing < len) max_len = missing;

            added = tx_append(data, max_len);

            if (added != max_len) {
                tx_initialized = false;
                THROW(APDU_CODE_OUTPUT_BUFFER_TOO_SMALL);
            }

            // check if this chunk was the last one
            if (missing - len == 0) {
                tx_initialized = false;
                return true;
            }

            return false;
    }
    THROW(APDU_CODE_INVALIDP1P2);
}

__Z_INLINE void handleGetAddr(volatile uint32_t *flags, volatile uint32_t *tx, uint32_t rx) {
    zemu_log("handleGetAddr\n");
    uint8_t len = extractHRP(rx, OFFSET_DATA);
    extractHDPath(rx, OFFSET_DATA + 1 + len);

    const uint8_t requireConfirmation = G_io_apdu_buffer[OFFSET_P1];

    const zxerr_t zxerr = app_fill_address();
    if (zxerr != zxerr_ok) {
        *tx = 0;
        THROW(APDU_CODE_DATA_INVALID);
    }

    if (requireConfirmation) {
        view_review_init(addr_getItem, addr_getNumItems, app_reply_address);
        view_review_show(REVIEW_ADDRESS);
        *flags |= IO_ASYNCH_REPLY;
        return;
    }

    *tx = action_addrResponseLen;
    THROW(APDU_CODE_OK);
}

__Z_INLINE void handleGetAddrEth(volatile uint32_t *flags, volatile uint32_t *tx, uint32_t rx) {
    extract_eth_path(rx, OFFSET_DATA);

    uint8_t requireConfirmation = G_io_apdu_buffer[OFFSET_P1];
    uint8_t with_code = G_io_apdu_buffer[OFFSET_P2];

    if (with_code != P2_CHAINCODE && with_code != P2_NO_CHAINCODE) THROW(APDU_CODE_INVALIDP1P2);

    flr_chain_code = with_code;

    zxerr_t zxerr = app_fill_eth_address();
    if (zxerr != zxerr_ok) {
        *tx = 0;
        THROW(APDU_CODE_DATA_INVALID);
    }
    if (requireConfirmation) {
        view_review_init(eth_addr_getItem, eth_addr_getNumItems, app_reply_address);
        view_review_show(REVIEW_ADDRESS);
        *flags |= IO_ASYNCH_REPLY;
        return;
    }
    *tx = action_addrResponseLen;
    THROW(APDU_CODE_OK);
}

__Z_INLINE void handleSign(volatile uint32_t *flags, volatile uint32_t *tx, uint32_t rx) {
    zemu_log("handleSign\n");
    if (!process_chunk(tx, rx)) {
        THROW(APDU_CODE_OK);
    }

    // GET address to test later which outputs the app should show
    zxerr_t zxerr = app_get_address();
    if (zxerr != zxerr_ok) {
        *tx = 0;
        THROW(APDU_CODE_DATA_INVALID);
    }

    const char *error_msg = tx_parse();
    CHECK_APP_CANARY()
    if (error_msg != NULL) {
        const int error_msg_length = strnlen(error_msg, sizeof(G_io_apdu_buffer));
        memcpy(G_io_apdu_buffer, error_msg, error_msg_length);
        *tx += (error_msg_length);
        THROW(APDU_CODE_DATA_INVALID);
    }

    view_review_init(tx_getItem, tx_getNumItems, app_sign);
    view_review_show(REVIEW_TXN);
    *flags |= IO_ASYNCH_REPLY;
}

__Z_INLINE void handleSignHash(volatile uint32_t *flags, volatile uint32_t *tx, uint32_t rx) {
    zemu_log("handleSignHash\n");
    if (!process_chunk(tx, rx)) {
        THROW(APDU_CODE_OK);
    }

    if (!app_mode_expert()) {
        *flags |= IO_ASYNCH_REPLY;
        view_custom_error_show(PIC(msg_error1), PIC(msg_error2));
        THROW(APDU_CODE_DATA_INVALID);
    }

    const char *error_msg = hash_parse();
    CHECK_APP_CANARY()
    if (error_msg != NULL) {
        const int error_msg_length = strnlen(error_msg, sizeof(G_io_apdu_buffer));
        memcpy(G_io_apdu_buffer, error_msg, error_msg_length);
        *tx += (error_msg_length);
        THROW(APDU_CODE_DATA_INVALID);
    }

    view_review_init(hash_getItem, hash_getNumItems, app_sign_hash);
    view_review_show(REVIEW_TXN);
    *flags |= IO_ASYNCH_REPLY;
}

__Z_INLINE void handleSignEth(volatile uint32_t *flags, volatile uint32_t *tx, uint32_t rx) {
    zemu_log_stack("handleSignEth");
    if (!process_chunk_eth(tx, rx)) {
        THROW(APDU_CODE_OK);
    }

    CHECK_APP_CANARY()

    const char *error_msg = tx_parse();
    CHECK_APP_CANARY()

    if (error_msg != NULL) {
        const int error_msg_length = strnlen(error_msg, sizeof(G_io_apdu_buffer));
        MEMCPY(G_io_apdu_buffer, error_msg, error_msg_length);
        *tx += (error_msg_length);
        THROW(APDU_CODE_DATA_INVALID);
    }

    CHECK_APP_CANARY()
    view_review_init(tx_getItem, tx_getNumItems, app_sign_eth);
    view_review_show(REVIEW_TXN);
    *flags |= IO_ASYNCH_REPLY;
}

__Z_INLINE void handle_getversion(__Z_UNUSED volatile uint32_t *flags, volatile uint32_t *tx) {
    G_io_apdu_buffer[0] = 0;

#if defined(APP_TESTING)
    G_io_apdu_buffer[0] = 0x01;
#endif

    G_io_apdu_buffer[1] = (LEDGER_MAJOR_VERSION >> 8) & 0xFF;
    G_io_apdu_buffer[2] = (LEDGER_MAJOR_VERSION >> 0) & 0xFF;

    G_io_apdu_buffer[3] = (LEDGER_MINOR_VERSION >> 8) & 0xFF;
    G_io_apdu_buffer[4] = (LEDGER_MINOR_VERSION >> 0) & 0xFF;

    G_io_apdu_buffer[5] = (LEDGER_PATCH_VERSION >> 8) & 0xFF;
    G_io_apdu_buffer[6] = (LEDGER_PATCH_VERSION >> 0) & 0xFF;

    G_io_apdu_buffer[7] = !IS_UX_ALLOWED;

    G_io_apdu_buffer[8] = (TARGET_ID >> 24) & 0xFF;
    G_io_apdu_buffer[9] = (TARGET_ID >> 16) & 0xFF;
    G_io_apdu_buffer[10] = (TARGET_ID >> 8) & 0xFF;
    G_io_apdu_buffer[11] = (TARGET_ID >> 0) & 0xFF;

    *tx += 12;
    THROW(APDU_CODE_OK);
}

void handleApdu(volatile uint32_t *flags, volatile uint32_t *tx, uint32_t rx) {
    volatile uint16_t sw = 0;

    BEGIN_TRY {
        TRY {
            const uint8_t cla = G_io_apdu_buffer[OFFSET_CLA];

            if ((cla != CLA) && (cla != CLA_ETH)) {
                THROW(APDU_CODE_CLA_NOT_SUPPORTED);
            }

            if (rx < APDU_MIN_LENGTH) {
                THROW(APDU_CODE_WRONG_LENGTH);
            }

            const uint8_t instruction = G_io_apdu_buffer[OFFSET_INS];

            // Handle this case as ins number is the same as normal flr sign
            // instruction
            if (instruction == INS_GET_ADDR_ETH && cla == CLA_ETH) {
                handleGetAddrEth(flags, tx, rx);
            } else {
                switch (instruction) {
                    case INS_GET_VERSION: {
                        handle_getversion(flags, tx);
                        break;
                    }

                    case INS_GET_ADDR: {
                        CHECK_PIN_VALIDATED()
                        handleGetAddr(flags, tx, rx);
                        break;
                    }

                    case INS_SIGN: {
                        CHECK_PIN_VALIDATED()
                        handleSign(flags, tx, rx);
                        break;
                    }

                    case INS_SIGN_HASH: {
                        CHECK_PIN_VALIDATED()
                        handleSignHash(flags, tx, rx);
                        break;
                    }
                    case INS_SIGN_ETH: {
                        CHECK_PIN_VALIDATED()
                        if (cla != CLA_ETH) {
                            THROW(APDU_CODE_COMMAND_NOT_ALLOWED);
                        }
                        handleSignEth(flags, tx, rx);
                        break;
                    }
                    default:
                        THROW(APDU_CODE_INS_NOT_SUPPORTED);
                }
            }
        }
        CATCH(EXCEPTION_IO_RESET) { THROW(EXCEPTION_IO_RESET); }
        CATCH_OTHER(e) {
            switch (e & 0xF000) {
                case 0x6000:
                case APDU_CODE_OK:
                    sw = e;
                    break;
                default:
                    sw = 0x6800 | (e & 0x7FF);
                    break;
            }
            G_io_apdu_buffer[*tx] = sw >> 8;
            G_io_apdu_buffer[*tx + 1] = sw & 0xFF;
            *tx += 2;
        }
        FINALLY {}
    }
    END_TRY;
}
