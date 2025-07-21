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
#pragma once
#include <cstdint>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// Helper function to safely get string from JSON value (handles both string and number types)
std::string getStringValue(const nlohmann::json &obj, const std::string &key, const std::string &defaultValue = "");

// Helper function to safely get uint64_t from JSON value
uint64_t getUint64Value(const nlohmann::json &obj, const std::string &key, uint64_t defaultValue = 0);

std::vector<std::string> EVMGenerateExpectedUIOutput(const nlohmann::json &json, bool expertMode);
