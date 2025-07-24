# *******************************************************************************
# *   (c) 2018 -2023 Zondax AG
# *
# *  Licensed under the Apache License, Version 2.0 (the "License");
# *  you may not use this file except in compliance with the License.
# *  You may obtain a copy of the License at
# *
# *      http://www.apache.org/licenses/LICENSE-2.0
# *
# *  Unless required by applicable law or agreed to in writing, software
# *  distributed under the License is distributed on an "AS IS" BASIS,
# *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# *  See the License for the specific language governing permissions and
# *  limitations under the License.
# ********************************************************************************
cmake_minimum_required(VERSION 3.0)
include("cmake/HunterGate.cmake")
HunterGate(
    URL "https://github.com/cpp-pm/hunter/archive/v0.26.1.tar.gz"
    SHA1 "e41ac7a18c49b35ebac99ff2b5244317b2638a65"
    LOCAL
)

project(ledger-flare VERSION 0.0.0)
enable_testing()

# Include common fuzzing configuration
include(deps/ledger-zxlib/fuzzing/FuzzingCommon.cmake)

cmake_policy(SET CMP0025 NEW)
set(CMAKE_CXX_STANDARD 11)

set(HUNTER_STATUS_DEBUG ON)
set(HUNTER_TLS_VERIFY OFF)

# Base debug flags (fuzzing flags are handled by FuzzingCommon.cmake)
string(APPEND CMAKE_C_FLAGS " -fno-omit-frame-pointer -g")
string(APPEND CMAKE_CXX_FLAGS " -fno-omit-frame-pointer -g")
string(APPEND CMAKE_LINKER_FLAGS " -fno-omit-frame-pointer -g")

add_definitions(-DAPP_STANDARD)
add_definitions(-DSUBSTRATE_PARSER_FULL)

hunter_add_package(fmt)
find_package(fmt CONFIG REQUIRED)

# Use FetchContent to get nlohmann_json 3.12.0 since Hunter doesn't support this version
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

hunter_add_package(GTest)
find_package(GTest CONFIG REQUIRED)

# Fuzzing configuration is now handled by FuzzingCommon.cmake

set(RETRIEVE_MAJOR_CMD
    "cat ${CMAKE_CURRENT_SOURCE_DIR}/app/Makefile.version | grep APPVERSION_M | cut -b 14- | tr -d '\n'"
)
set(RETRIEVE_MINOR_CMD
    "cat ${CMAKE_CURRENT_SOURCE_DIR}/app/Makefile.version | grep APPVERSION_N | cut -b 14- | tr -d '\n'"
)
execute_process(
    COMMAND bash "-c" ${RETRIEVE_MAJOR_CMD}
    RESULT_VARIABLE MAJOR_RESULT
    OUTPUT_VARIABLE MAJOR_VERSION
)
execute_process(
    COMMAND bash "-c" ${RETRIEVE_MINOR_CMD}
    RESULT_VARIABLE MINOR_RESULT
    OUTPUT_VARIABLE MINOR_VERSION
)

message(STATUS "LEDGER_MAJOR_VERSION [${MAJOR_RESULT}]: ${MAJOR_VERSION}")
message(STATUS "LEDGER_MINOR_VERSION [${MINOR_RESULT}]: ${MINOR_VERSION}")

add_definitions(
    -DLEDGER_MAJOR_VERSION=${MAJOR_VERSION}
    -DLEDGER_MINOR_VERSION=${MINOR_VERSION}
)

# #############################################################
# #############################################################
# static libs
file(GLOB_RECURSE LIB_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/src/hexutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/src/app_mode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/src/bignum.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/src/hexutils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/src/zxmacros.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/src/zxformat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/src/bech32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/src/base58.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/src/segwit_addr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/src/timeutils.c

    # ###
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/parser_impl.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/crypto_helper.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ripemd160/ripemd160.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/parser_impl_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/parser_print_common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/tx_cchain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/tx_pchain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/evm/parser_impl_evm_specific.c

    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/evm/rlp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/evm/uint256.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/evm/evm_utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/evm/parser_evm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/evm/evm_erc20.c
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/evm/parser_impl_evm.c
)

add_library(app_lib STATIC ${LIB_SRC})

target_include_directories(app_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/app/common
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/evm
    ${CMAKE_CURRENT_SOURCE_DIR}/app/src/common
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/picohash/
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ripemd160
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/ledger-zxlib/evm
)

if(ENABLE_FUZZING) # Fuzz Targets
    # Setup fuzzing directories
    setup_fuzz_directories()
    
    # Add fuzz targets using the common macro
    add_fuzz_target(parser_parse ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/parser_parse.cpp app_lib)
    
else()
    # #############################################################
    # #############################################################
    # Tests
    file(GLOB_RECURSE TESTS_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)

    add_executable(unittests ${TESTS_SRC})
    target_include_directories(unittests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/app/src
        ${CMAKE_CURRENT_SOURCE_DIR}/app/src/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/ripemd160
    )

    target_link_libraries(unittests PRIVATE
        GTest::gtest_main
        app_lib
        fmt::fmt
        nlohmann_json::nlohmann_json)

    add_compile_definitions(TESTVECTORS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/")
    add_test(NAME unittests COMMAND unittests)
    set_tests_properties(unittests PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()
