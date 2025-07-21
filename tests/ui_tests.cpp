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

#include <hexutils.h>
#include <parser_txdef.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "app_mode.h"
#include "expected_output.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "parser.h"
#include "parser_common.h"
#include "parser_evm.h"
#include "testcases.h"
#include "utils/common.h"

using ::testing::TestWithParam;

class JsonTestsA : public ::testing::TestWithParam<testcase_t> {
   public:
    struct PrintToStringParamName {
        template <class ParamType>
        std::string operator()(const testing::TestParamInfo<ParamType> &info) const {
            auto p = static_cast<testcase_t>(info.param);
            std::stringstream ss;
            ss << p.index << "_" << p.name;
            return ss.str();
        }
    };
};

// Retrieve testcases from json file
std::vector<testcase_t> GetJsonTestCases(std::string jsonFile) {
    auto answer = std::vector<testcase_t>();

    const std::string fullPathJsonFile = std::string(TESTVECTORS_DIR) + jsonFile;

    std::ifstream inFile(fullPathJsonFile);
    if (!inFile.is_open()) {
        return answer;
    }

    // Retrieve all test cases
    nlohmann::json obj;
    inFile >> obj;
    std::cout << "Number of testcases: " << obj.size() << std::endl;

    for (int i = 0; i < obj.size(); i++) {
        auto outputs = std::vector<std::string>();
        for (auto s : obj[i]["output"]) {
            outputs.push_back(s.get<std::string>());
        }

        auto outputs_expert = std::vector<std::string>();
        for (auto s : obj[i]["output_expert"]) {
            outputs_expert.push_back(s.get<std::string>());
        }

        uint64_t index = getUint64Value(obj[i], "index", 0);
        std::string name = getStringValue(obj[i], "name", "");
        std::string blob = getStringValue(obj[i], "blob", "");

        answer.push_back(testcase_t{index, name, blob, outputs, outputs_expert});
    }

    return answer;
}

std::string CleanTestname(std::string s) {
    s.erase(remove_if(s.begin(), s.end(),
                      [](char v) -> bool {
                          return v == ':' || v == ' ' || v == '/' || v == '-' || v == '.' || v == '_' || v == '#';
                      }),
            s.end());
    return s;
}

// Retrieve testcases from json file
template <typename Generator>
std::vector<testcase_t> GetEVMJsonTestCases(const std::string &jsonFile, Generator gen_ui_output) {
    auto answer = std::vector<testcase_t>();

    const std::string fullPathJsonFile = std::string(TESTVECTORS_DIR) + jsonFile;

    std::ifstream inFile(fullPathJsonFile);
    if (!inFile.is_open()) {
        return answer;
    }

    nlohmann::json obj;
    inFile >> obj;
    std::cout << "Number of testcases: " << obj.size() << std::endl;

    for (auto &i : obj) {
        // auto outputs = GenerateExpectedUIOutput(i, false);
        // auto outputs_expert = GenerateExpectedUIOutput(i, true);
        auto outputs = gen_ui_output(i, false);
        auto outputs_expert = gen_ui_output(i, true);

        auto name = CleanTestname(getStringValue(i, "description", ""));
        auto encoded_tx_hex = getStringValue(i, "encoded_tx_hex", "");

        answer.push_back(testcase_t{answer.size() + 1, name, encoded_tx_hex, outputs, outputs_expert});
    }

    return answer;
}

void check_testcase(const testcase_t &tc, bool expert_mode, bool is_eth) {
    app_mode_set_expert(expert_mode);

    parser_error_t err;
    parser_context_t ctx;

    uint8_t buffer[5000];
    const uint16_t bufferLen = parseHexString(buffer, sizeof(buffer), tc.blob.c_str());

    parser_tx_t tx_obj;
    memset(&tx_obj, 0, sizeof(tx_obj));

    if (is_eth) {
        err = parser_parse_eth(&ctx, buffer, bufferLen);
    } else {
        err = parser_parse(&ctx, buffer, bufferLen, &tx_obj);
    }
    ASSERT_EQ(err, parser_ok) << parser_getErrorDescription(err);

    auto output = dumpUI(&ctx, 39, 39, is_eth);

    std::cout << std::endl;
    for (const auto &i : output) {
        std::cout << i << std::endl;
    }
    std::cout << std::endl << std::endl;

    std::vector<std::string> expected = app_mode_expert() ? tc.expected_expert : tc.expected;

    EXPECT_EQ(output.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
        if (i < output.size()) {
            EXPECT_THAT(output[i], testing::Eq(expected[i]));
        }
    }
}

class VerifyEvmTransactions : public JsonTestsA {};

INSTANTIATE_TEST_SUITE_P(JsonTestCasesCurrentTxVer, JsonTestsA,
                         ::testing::ValuesIn(GetJsonTestCases("testvectors/testcases.json")),
                         JsonTestsA::PrintToStringParamName());
INSTANTIATE_TEST_SUITE_P(EVMJsonTestCasesCurrentTxVer, VerifyEvmTransactions,
                         ::testing::ValuesIn(GetEVMJsonTestCases("testvectors/evm.json", EVMGenerateExpectedUIOutput)),
                         JsonTestsA::PrintToStringParamName());

TEST_P(JsonTestsA, JsonTestsA_CheckUIOutput_CurrentTX_Normal) { check_testcase(GetParam(), false, false); }
TEST_P(JsonTestsA, JsonTestsA_CheckUIOutput_CurrentTX_Expert) { check_testcase(GetParam(), true, false); }
TEST_P(VerifyEvmTransactions, JsonTestsEVM_CheckUIOutput_CurrentTX_Normal) { check_testcase(GetParam(), false, true); }
