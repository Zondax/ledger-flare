/*******************************************************************************
 *   (c) 2019 Zondax GmbH
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

#include <coin.h>
#include <fmt/core.h>

#include <cstdint>
#include <string>
#include <vector>

#include "json/value.h"
#include "zxformat.h"
#include "zxmacros_x64.h"
const uint32_t fieldSize = 39;

template <typename S, typename... Args>
void addTo(std::vector<std::string> &answer, const S &format_str, Args &&...args) {
    answer.push_back(fmt::format(format_str, args...));
}

std::vector<std::string> FormatEthAddress(const std::string &name, const uint32_t idx, const std::string &address) {
    auto answer = std::vector<std::string>();
    uint8_t numPages = 0;
    char outBuffer[100];

    pageString(outBuffer, fieldSize, address.c_str(), 0, &numPages);

    for (auto i = 0; i < numPages; i++) {
        MEMZERO(outBuffer, sizeof(outBuffer));
        pageString(outBuffer, fieldSize, address.c_str(), i, &numPages);

        auto pages = std::string("");

        if (numPages > 1) {
            pages = fmt::format("[{}/{}]", i + 1, numPages);
        }

        addTo(answer, "{} | {} {} : {}", idx, name, pages, outBuffer);
    }

    return answer;
}

std::string FormatAmount(const std::string &amount) {
    char buffer[500];
    MEMZERO(buffer, sizeof(buffer));
    fpstr_to_str(buffer, sizeof(buffer), amount.c_str(), COIN_AMOUNT_DECIMAL_PLACES);
    return std::string(buffer);
}

std::vector<std::string> EVMGenerateExpectedUIOutput(const Json::Value &json, bool) {
    auto answer = std::vector<std::string>();

    ///
    auto description = json["description"].asString();
    auto message = json["message"];
    auto receiver = message["Receiver"].asString();
    auto contract = message["Contract"].asString();
    auto amount = message["Amount"].asString();
    auto nonce = message["Nonce"].asString();
    auto maxFee = std::string();
    auto maxPriorityFee = std::string();
    auto gasPrice = std::string();
    if (description.find("eip1559") != std::string::npos) {
        maxFee = message["MaxFeePerGas"].asString();
        maxPriorityFee = message["MaxPriorityFeePerGas"].asString();
    } else {
        gasPrice = message["GasPrice"].asString();
    }
    auto gasLimit = message["GasLimit"].asString();
    auto value = message["Value"].asString();
    auto txhash = message["Eth-Hash"].asString();
    auto data = message["Data"].asString();
    ///

    uint8_t idx = 0;
    auto destAddress = FormatEthAddress("Receiver", idx, receiver);
    answer.insert(answer.end(), destAddress.begin(), destAddress.end());

    idx++;
    auto contractAddress = FormatEthAddress("Contract", idx, contract);
    answer.insert(answer.end(), contractAddress.begin(), contractAddress.end());

    idx++;
    addTo(answer, "{} | Coin asset : {}", idx, "Flare");

    idx++;
    addTo(answer, "{} | Amount : {}", idx, amount);

    idx++;
    addTo(answer, "{} | Nonce : {}", idx, nonce);

    if (description.find("eip1559") != std::string::npos) {
        idx++;
        addTo(answer, "{} | Max Priority Fee : {}", idx, maxPriorityFee);
        idx++;
        addTo(answer, "{} | Max Fee : {}", idx, maxFee);
    }
    if (description.find("eip1559") == std::string::npos) {
        idx++;
        addTo(answer, "{} | Gas price : {}", idx, gasPrice);
    }

    idx++;
    addTo(answer, "{} | Gas limit : {}", idx, gasLimit);

    idx++;
    addTo(answer, "{} | Value : {}", idx, value);

    idx++;
    addTo(answer, "{} | Data : {}", idx, data);

    idx++;
    auto hash = FormatEthAddress("Eth-Hash", idx, txhash);
    answer.insert(answer.end(), hash.begin(), hash.end());

    return answer;
}
