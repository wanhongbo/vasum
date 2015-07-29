/*
 *  Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Lukasz Pawelczyk (l.pawelczyk@samsung.com)
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
 *  limitations under the License
 */

/**
 * @file
 * @author  Lukasz Pawelczyk (l.pawelczyk@samsung.com)
 * @brief   Persistent file backend for logger
 */

#ifndef COMMON_LOGGER_BACKEND_PERSISTENT_FILE_HPP
#define COMMON_LOGGER_BACKEND_PERSISTENT_FILE_HPP

#include "logger/backend.hpp"

#include <fstream>

namespace logger {

class PersistentFileBackend : public LogBackend {
public:
    PersistentFileBackend(const std::string& filePath) :
        mfilePath(filePath),
        mOut(mfilePath, std::ios::app) {}

    void log(LogLevel logLevel,
             const std::string& file,
             const unsigned int& line,
             const std::string& func,
             const std::string& message) override;
private:
    std::string mfilePath;
    std::ofstream mOut;
};

} // namespace logger

#endif // COMMON_LOGGER_BACKEND_PERSISTENT_FILE_HPP
