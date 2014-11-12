/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Jan Olszak <j.olszak@samsung.com>
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
 * @author  Jan Olszak (j.olszak@samsung.com)
 * @brief   Tests of the IPC
 */

// TODO: Test connection limit
// TODO: Refactor tests - function for setting up env


#include "config.hpp"
#include "ut.hpp"

#include "ipc/service.hpp"
#include "ipc/client.hpp"
#include "ipc/types.hpp"

#include "config/fields.hpp"
#include "logger/logger.hpp"

#include <random>
#include <string>
#include <thread>
#include <chrono>
#include <boost/filesystem.hpp>

using namespace security_containers;
using namespace security_containers::ipc;
namespace fs = boost::filesystem;

namespace {
struct Fixture {
    std::string socketPath;

    Fixture()
        : socketPath(fs::unique_path("/tmp/ipc-%%%%.socket").string())
    {
    }
    ~Fixture()
    {
        fs::remove(socketPath);
    }
};

struct SendData {
    int intVal;
    SendData(int i = 0): intVal(i) {}

    CONFIG_REGISTER
    (
        intVal
    )
};

struct EmptyData {
    CONFIG_REGISTER_EMPTY
};

std::shared_ptr<EmptyData> returnEmptyCallback(std::shared_ptr<EmptyData>&)
{
    return std::shared_ptr<EmptyData>(new EmptyData());
}

std::shared_ptr<SendData> returnDataCallback(std::shared_ptr<SendData>&)
{
    return std::shared_ptr<SendData>(new SendData(1));
}

std::shared_ptr<SendData> echoCallback(std::shared_ptr<SendData>& data)
{
    return data;
}

void testEcho(Client& c, const Client::MethodID methodID)
{
    std::shared_ptr<SendData> sentData(new SendData(34));
    std::shared_ptr<SendData> recvData = c.callSync<SendData, SendData>(methodID, sentData);
    BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
}

void testEcho(Service& s, const Client::MethodID methodID, const Service::PeerID peerID)
{
    std::shared_ptr<SendData> sentData(new SendData(56));
    std::shared_ptr<SendData> recvData = s.callSync<SendData, SendData>(methodID, peerID, sentData);
    BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
}

} // namespace


BOOST_FIXTURE_TEST_SUITE(IPCSuite, Fixture)

BOOST_AUTO_TEST_CASE(ConstructorDestructorTest)
{
    Service s(socketPath);
    Client c(socketPath);
}

BOOST_AUTO_TEST_CASE(ServiceAddRemoveMethodTest)
{
    Service s(socketPath);

    s.addMethodHandler<EmptyData, EmptyData>(1, returnEmptyCallback);
    s.addMethodHandler<SendData, SendData>(1, returnDataCallback);

    s.start();

    s.addMethodHandler<SendData, SendData>(1, echoCallback);
    s.addMethodHandler<SendData, SendData>(2, returnDataCallback);

    Client c(socketPath);
    c.start();
    testEcho(c, 1);

    s.removeMethod(1);
    s.removeMethod(2);

    BOOST_CHECK_THROW(testEcho(c, 2), IPCException);
}

BOOST_AUTO_TEST_CASE(ClientAddRemoveMethodTest)
{
    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    std::condition_variable cv;
    unsigned int peerID = 0;
    auto newPeerCallback = [&cv, &peerID](unsigned int newPeerID) {
        peerID = newPeerID;
        cv.notify_one();
    };
    Service s(socketPath, newPeerCallback);
    s.start();
    Client c(socketPath);

    c.addMethodHandler<EmptyData, EmptyData>(1, returnEmptyCallback);
    c.addMethodHandler<SendData, SendData>(1, returnDataCallback);

    c.start();

    c.addMethodHandler<SendData, SendData>(1, echoCallback);
    c.addMethodHandler<SendData, SendData>(2, returnDataCallback);

    BOOST_CHECK(cv.wait_for(lck, std::chrono::milliseconds(1000), [&peerID]() {
        return peerID != 0;
    }));

    testEcho(s, 1, peerID);

    c.removeMethod(1);
    c.removeMethod(2);

    BOOST_CHECK_THROW(testEcho(s, 1, peerID), IPCException);
}

BOOST_AUTO_TEST_CASE(ServiceStartStopTest)
{
    Service s(socketPath);

    s.addMethodHandler<SendData, SendData>(1, returnDataCallback);

    s.start();
    s.stop();
    s.start();
    s.stop();

    s.start();
    s.start();
}

BOOST_AUTO_TEST_CASE(ClientStartStopTest)
{
    Service s(socketPath);
    Client c(socketPath);
    c.addMethodHandler<SendData, SendData>(1, returnDataCallback);

    c.start();
    c.stop();
    c.start();
    c.stop();

    c.start();
    c.start();

    c.stop();
    c.stop();
}

BOOST_AUTO_TEST_CASE(SyncClientToServiceEchoTest)
{
    Service s(socketPath);
    s.addMethodHandler<SendData, SendData>(1, echoCallback);
    s.addMethodHandler<SendData, SendData>(2, echoCallback);

    s.start();
    Client c(socketPath);
    c.start();
    testEcho(c, 1);
    testEcho(c, 2);
}

BOOST_AUTO_TEST_CASE(RestartTest)
{
    Service s(socketPath);
    s.addMethodHandler<SendData, SendData>(1, echoCallback);
    s.start();
    s.addMethodHandler<SendData, SendData>(2, echoCallback);

    Client c(socketPath);
    c.start();
    testEcho(c, 1);
    testEcho(c, 2);

    c.stop();
    c.start();

    testEcho(c, 1);
    testEcho(c, 2);

    s.stop();
    s.start();

    testEcho(c, 1);
    testEcho(c, 2);
}

BOOST_AUTO_TEST_CASE(SyncServiceToClientEchoTest)
{
    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    std::condition_variable cv;
    unsigned int peerID = 0;
    auto newPeerCallback = [&cv, &peerID](unsigned int newPeerID) {
        peerID = newPeerID;
        cv.notify_one();
    };
    Service s(socketPath, newPeerCallback);
    s.start();
    Client c(socketPath);
    c.addMethodHandler<SendData, SendData>(1, echoCallback);
    c.start();

    BOOST_CHECK(cv.wait_for(lck, std::chrono::milliseconds(1000), [&peerID]() {
        return peerID != 0;
    }));

    std::shared_ptr<SendData> sentData(new SendData(56));
    std::shared_ptr<SendData> recvData = s.callSync<SendData, SendData>(1, peerID, sentData);
    BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
}

BOOST_AUTO_TEST_CASE(AsyncClientToServiceEchoTest)
{
    // Setup Service and Client
    Service s(socketPath);
    s.addMethodHandler<SendData, SendData>(1, echoCallback);
    s.start();
    Client c(socketPath);
    c.start();

    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    std::condition_variable cv;

    //Async call
    std::shared_ptr<SendData> sentData(new SendData(34));
    std::shared_ptr<SendData> recvData;
    auto dataBack = [&cv, &recvData](std::shared_ptr<SendData>& data) {
        recvData = data;
        cv.notify_one();
    };
    c.callAsync<SendData, SendData>(1, sentData, dataBack);

    // Wait for the response
    BOOST_CHECK(cv.wait_for(lck, std::chrono::milliseconds(100), [&recvData]() {
        return static_cast<bool>(recvData);
    }));

    BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
}

BOOST_AUTO_TEST_CASE(AsyncServiceToClientEchoTest)
{
    std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);
    std::condition_variable cv;

    // Setup Service and Client
    unsigned int peerID = 0;
    auto newPeerCallback = [&cv, &peerID](unsigned int newPeerID) {
        peerID = newPeerID;
        cv.notify_one();
    };
    Service s(socketPath, newPeerCallback);
    s.start();
    Client c(socketPath);
    c.addMethodHandler<SendData, SendData>(1, echoCallback);
    c.start();

    // Wait for the connection
    BOOST_CHECK(cv.wait_for(lck, std::chrono::milliseconds(1000), [&peerID]() {
        return peerID != 0;
    }));

    // Async call
    std::shared_ptr<SendData> sentData(new SendData(56));
    std::shared_ptr<SendData> recvData;

    auto dataBack = [&cv, &recvData](std::shared_ptr<SendData>& data) {
        recvData = data;
        cv.notify_one();
    };

    s.callAsync<SendData, SendData>(1, peerID, sentData, dataBack);

    // Wait for the response
    BOOST_CHECK(cv.wait_for(lck, std::chrono::milliseconds(1000), [&recvData]() {
        return recvData.get() != nullptr;
    }));

    BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
}


BOOST_AUTO_TEST_CASE(SyncTimeoutTest)
{
    Service s(socketPath);
    s.addMethodHandler<SendData, SendData>(1, echoCallback);

    s.start();
    Client c(socketPath);
    c.start();

    std::shared_ptr<SendData> sentData(new SendData(78));

    BOOST_CHECK_THROW((c.callSync<SendData, SendData>(1, sentData, 1)), IPCException);
}


// BOOST_AUTO_TEST_CASE(ConnectionLimitTest)
// {
//     unsigned oldLimit = ipc::getMaxFDNumber();
//     ipc::setMaxFDNumber(50);

//     // Setup Service and many Clients
//     Service s(socketPath);
//     s.addMethodHandler<SendData, SendData>(1, echoCallback);
//     s.start();

//     std::list<Client> clients;
//     for (int i = 0; i < 100; ++i) {
//         try {
//             clients.push_back(Client(socketPath));
//             clients.back().start();
//         } catch (...) {}
//     }

//     unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
//     std::mt19937 generator(seed);
//     for (auto it = clients.begin(); it != clients.end(); ++it) {
//         try {
//             std::shared_ptr<SendData> sentData(new SendData(generator()));
//             std::shared_ptr<SendData> recvData = it->callSync<SendData, SendData>(1, sentData);
//             BOOST_CHECK_EQUAL(recvData->intVal, sentData->intVal);
//         } catch (...) {}
//     }

//     ipc::setMaxFDNumber(oldLimit);
// }



BOOST_AUTO_TEST_SUITE_END()