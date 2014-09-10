/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Mateusz Malicki <m.malicki2@samsung.com>
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
 * @author  Mateusz Malicki (m.malicki2@samsung.com)
 * @brief   Unit tests of the client C API
 */

#include <config.hpp>
#include "ut.hpp"
#include <security-containers-client.h>

#include "utils/latch.hpp"
#include "containers-manager.hpp"
#include "container-dbus-definitions.hpp"

#include <map>
#include <string>
#include <utility>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

using namespace security_containers;
using namespace security_containers::utils;

namespace {

const std::string TEST_DBUS_CONFIG_PATH =
    SC_TEST_CONFIG_INSTALL_DIR "/client/ut-client/test-dbus-daemon.conf";

struct Loop {
    Loop() { sc_start_glib_loop(); }
    ~Loop() { sc_stop_glib_loop(); }
};

struct Fixture {
    Loop loop;
    ContainersManager cm;

    Fixture(): cm(TEST_DBUS_CONFIG_PATH)
    {
        cm.startAll();
    }
};

const int EVENT_TIMEOUT = 5000; ///< ms
const std::map<std::string, std::string> EXPECTED_DBUSES_STARTED = {
    {"ut-containers-manager-console1-dbus",
     "unix:path=/tmp/ut-containers-manager/console1-dbus/dbus/system_bus_socket"},
    {"ut-containers-manager-console2-dbus",
     "unix:path=/tmp/ut-containers-manager/console2-dbus/dbus/system_bus_socket"},
    {"ut-containers-manager-console3-dbus",
     "unix:path=/tmp/ut-containers-manager/console3-dbus/dbus/system_bus_socket"}};

void convertDictToMap(ScArrayString keys,
                      ScArrayString values,
                      std::map<std::string, std::string>& ret)
{
    ScArrayString iKeys;
    ScArrayString iValues;
    for (iKeys = keys, iValues = values; *iKeys && *iValues; iKeys++, iValues++) {
        ret.insert(std::make_pair(*iKeys, *iValues));
    }
}

void convertArrayToSet(ScArrayString values, std::set<std::string>& ret)
{
    for (ScArrayString iValues = values; *iValues; iValues++) {
        ret.insert(*iValues);
    }
}

int getArrayStringLength(ScArrayString astring, int max_len = -1)
{
    int i = 0;
    for (i = 0; astring[i];  i++) {
        if (i == max_len) {
            return max_len;
        }
    }
    return i;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(Client, Fixture)

BOOST_AUTO_TEST_CASE(NotRunningServerTest)
{
    cm.stopAll();

    ScClient client = sc_client_create();
    ScStatus status = sc_connect_custom(client,
                                        EXPECTED_DBUSES_STARTED.begin()->second.c_str());
    BOOST_CHECK_EQUAL(SCCLIENT_IO_ERROR, status);
    sc_client_free(client);
}

BOOST_AUTO_TEST_CASE(GetContainerDbusesTest)
{
    ScClient client = sc_client_create();
    ScStatus status = sc_connect(client);
    BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);
    ScArrayString keys, values;
    status = sc_get_container_dbuses(client, &keys, &values);
    BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);

    BOOST_CHECK_EQUAL(getArrayStringLength(keys, EXPECTED_DBUSES_STARTED.size() + 1),
                      EXPECTED_DBUSES_STARTED.size());
    BOOST_CHECK_EQUAL(getArrayStringLength(values, EXPECTED_DBUSES_STARTED.size() + 1),
                      EXPECTED_DBUSES_STARTED.size());

    std::map<std::string, std::string> containers;
    convertDictToMap(keys, values, containers);
    BOOST_CHECK(containers == EXPECTED_DBUSES_STARTED);
    sc_array_string_free(keys);
    sc_array_string_free(values);
    sc_client_free(client);
}

BOOST_AUTO_TEST_CASE(GetContainerIdsTest)
{
    ScClient client = sc_client_create();
    ScStatus status = sc_connect(client);
    BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);
    ScArrayString values;
    status = sc_get_container_ids(client, &values);
    BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);
    BOOST_CHECK_EQUAL(getArrayStringLength(values, EXPECTED_DBUSES_STARTED.size() + 1),
                      EXPECTED_DBUSES_STARTED.size());

    std::set<std::string> containers;
    convertArrayToSet(values, containers);

    for (const auto& container : containers) {
        BOOST_CHECK(EXPECTED_DBUSES_STARTED.find(container) != EXPECTED_DBUSES_STARTED.cend());
    }
    sc_array_string_free(values);
    sc_client_free(client);
}

BOOST_AUTO_TEST_CASE(GetActiveContainerIdTest)
{
    ScClient client = sc_client_create();
    ScStatus status = sc_connect(client);
    BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);
    ScString container;
    status = sc_get_active_container_id(client, &container);
    BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);

    BOOST_CHECK_EQUAL(container, cm.getRunningForegroundContainerId());

    sc_string_free(container);
    sc_client_free(client);
}

BOOST_AUTO_TEST_CASE(SetActiveContainerTest)
{
    const std::string newActiveContainerId = "ut-containers-manager-console2-dbus";

    BOOST_REQUIRE_NE(newActiveContainerId, cm.getRunningForegroundContainerId());

    ScClient client = sc_client_create();
    ScStatus status = sc_connect(client);
    BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);
    status = sc_set_active_container(client, newActiveContainerId.c_str());
    BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);
    BOOST_CHECK_EQUAL(newActiveContainerId, cm.getRunningForegroundContainerId());
    sc_client_free(client);
}

BOOST_AUTO_TEST_CASE(FileMoveRequestTest)
{
    const std::string path = "/tmp/fake_path";
    const std::string secondContainer = "fake_container";

    ScClient client = sc_client_create();
    ScStatus status = sc_connect_custom(client, EXPECTED_DBUSES_STARTED.begin()->second.c_str());
    BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);
    status = sc_file_move_request(client, secondContainer.c_str(), path.c_str());
    BOOST_REQUIRE_EQUAL(SCCLIENT_CUSTOM_ERROR, status);
    BOOST_REQUIRE_EQUAL(api::container::FILE_MOVE_DESTINATION_NOT_FOUND,
                        sc_get_status_message(client));
    sc_client_free(client);
}

BOOST_AUTO_TEST_CASE(NotificationTest)
{
    const std::string MSG_CONTENT = "msg";
    const std::string MSG_APP = "app";

    struct CallbackData {
        Latch signalReceivedLatch;
        std::vector< std::tuple<std::string, std::string, std::string> > receivedSignalMsg;
    };

    auto callback = [](const char* container,
                       const char* application,
                       const char* message,
                       void* data)
    {
        CallbackData& callbackData = *reinterpret_cast<CallbackData*>(data);
        callbackData.receivedSignalMsg.push_back(std::make_tuple(container, application, message));
        callbackData.signalReceivedLatch.set();
    };

    CallbackData callbackData;
    std::map<std::string, ScClient> clients;
    for (const auto& it : EXPECTED_DBUSES_STARTED) {
        ScClient client = sc_client_create();
        ScStatus status = sc_connect_custom(client, it.second.c_str());
        BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);
        clients[it.first] = client;
    }
    for (auto& client : clients) {
        ScStatus status = sc_notification(client.second, callback, &callbackData);
        BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);
    }
    for (auto& client : clients) {
        ScStatus status = sc_notify_active_container(client.second,
                                                     MSG_APP.c_str(),
                                                     MSG_CONTENT.c_str());
        BOOST_REQUIRE_EQUAL(SCCLIENT_SUCCESS, status);
    }

    BOOST_CHECK(callbackData.signalReceivedLatch.waitForN(clients.size() - 1, EVENT_TIMEOUT));
    BOOST_CHECK(callbackData.signalReceivedLatch.empty());

    for (const auto& msg : callbackData.receivedSignalMsg) {
        BOOST_CHECK(clients.count(std::get<0>(msg)) > 0);
        BOOST_CHECK_EQUAL(std::get<1>(msg), MSG_APP);
        BOOST_CHECK_EQUAL(std::get<2>(msg), MSG_CONTENT);
    }

    for (auto& client : clients) {
        sc_client_free(client.second);
    }
}

BOOST_AUTO_TEST_SUITE_END()