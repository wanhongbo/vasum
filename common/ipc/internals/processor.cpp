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
 * @brief   Data and event processing thread
 */

#include "config.hpp"

#include "ipc/exception.hpp"
#include "ipc/internals/processor.hpp"
#include "ipc/internals/utils.hpp"

#include <list>
#include <cerrno>
#include <cstring>
#include <stdexcept>

#include <sys/socket.h>
#include <limits>


namespace security_containers {
namespace ipc {

const Processor::MethodID Processor::RETURN_METHOD_ID = std::numeric_limits<MethodID>::max();

Processor::Processor(const PeerCallback& newPeerCallback,
                     const PeerCallback& removedPeerCallback,
                     const unsigned int maxNumberOfPeers)
    : mNewPeerCallback(newPeerCallback),
      mRemovedPeerCallback(removedPeerCallback),
      mMaxNumberOfPeers(maxNumberOfPeers),
      mMessageIDCounter(0),
      mPeerIDCounter(0)
{
    LOGT("Creating Processor");
}

Processor::~Processor()
{
    LOGT("Destroying Processor");
    try {
        stop();
    } catch (IPCException& e) {
        LOGE("Error in Processor's destructor: " << e.what());
    }
    LOGT("Destroyed Processor");
}

void Processor::start()
{
    LOGT("Starting Processor");
    if (!mThread.joinable()) {
        mThread = std::thread(&Processor::run, this);
    }
    LOGT("Started Processor");
}

void Processor::stop()
{
    LOGT("Stopping Processor");
    if (mThread.joinable()) {
        mEventQueue.send(Event::FINISH);
        mThread.join();
    }
    LOGT("Stopped Processor");
}

void Processor::removeMethod(const MethodID methodID)
{
    LOGT("Removing method " << methodID);
    Lock lock(mCallsMutex);
    mMethodsCallbacks.erase(methodID);
}

Processor::PeerID Processor::addPeer(const std::shared_ptr<Socket>& socketPtr)
{
    LOGT("Adding socket");
    PeerID peerID;
    {
        Lock lock(mSocketsMutex);
        peerID = getNextPeerID();
        SocketInfo socketInfo;
        socketInfo.peerID = peerID;
        socketInfo.socketPtr = std::move(socketPtr);
        mNewSockets.push(std::move(socketInfo));
    }
    LOGI("New peer added. Id: " << peerID);
    mEventQueue.send(Event::NEW_PEER);

    return peerID;
}

void Processor::removePeer(PeerID peerID)
{
    LOGW("Removing naughty peer. ID: " << peerID);
    {
        Lock lock(mSocketsMutex);
        mSockets.erase(peerID);
    }
    resetPolling();
}

void Processor::resetPolling()
{
    LOGI("Resetting polling");
    // Setup polling on eventfd and sockets
    Lock lock(mSocketsMutex);
    mFDs.resize(mSockets.size() + 1);

    mFDs[0].fd = mEventQueue.getFD();
    mFDs[0].events = POLLIN;

    auto socketIt = mSockets.begin();
    for (unsigned int i = 1; i < mFDs.size(); ++i) {
        mFDs[i].fd = socketIt->second->getFD();
        mFDs[i].events = POLLIN | POLLHUP; // Listen for input events
        ++socketIt;
        // TODO: It's possible to block on writing to fd. Maybe listen for POLLOUT too?
    }
}

void Processor::run()
{
    resetPolling();

    mIsRunning = true;
    while (mIsRunning) {
        LOGT("Waiting for communication...");
        int ret = poll(mFDs.data(), mFDs.size(), -1 /*blocking call*/);
        LOGT("... incoming communication!");
        if (ret == -1 || ret == 0) {
            if (errno == EINTR) {
                continue;
            }
            LOGE("Error in poll: " << std::string(strerror(errno)));
            throw IPCException("Error in poll: " + std::string(strerror(errno)));
        }

        // Check for lost connections:
        if (handleLostConnections()) {
            // mFDs changed
            continue;
        }

        // Check for incoming data.
        if (handleInputs()) {
            // mFDs changed
            continue;
        }

        // Check for incoming events
        if (handleEvent()) {
            // mFDs changed
            continue;
        }
    }
}

bool Processor::handleLostConnections()
{
    std::list<PeerID> peersToRemove;

    {
        Lock lock(mSocketsMutex);
        auto socketIt = mSockets.begin();
        for (unsigned int i = 1; i < mFDs.size(); ++i, ++socketIt) {
            if (mFDs[i].revents & POLLHUP) {
                LOGI("Lost connection to peer: " << socketIt->first);
                mFDs[i].revents &= ~(POLLHUP);
                peersToRemove.push_back(socketIt->first);
            }
        }

        for (const auto peerID : peersToRemove) {
            LOGT("Removing peer. ID: " << peerID);
            mSockets.erase(peerID);
        }
    }

    if (!peersToRemove.empty()) {
        resetPolling();
    }

    return !peersToRemove.empty();
}

bool Processor::handleInputs()
{
    std::list<std::pair<PeerID, std::shared_ptr<Socket>> > peersWithInput;
    {
        Lock lock(mSocketsMutex);
        auto socketIt = mSockets.begin();
        for (unsigned int i = 1; i < mFDs.size(); ++i, ++socketIt) {
            if (mFDs[i].revents & POLLIN) {
                mFDs[i].revents &= ~(POLLIN);
                peersWithInput.push_back(*socketIt);
            }
        }
    }

    bool pollChanged = false;
    // Handle input outside the critical section
    for (const auto& peer : peersWithInput) {
        pollChanged = pollChanged || handleInput(peer.first, *peer.second);
    }
    return pollChanged;
}

bool Processor::handleInput(const PeerID peerID, const Socket& socket)
{
    LOGT("Handle incoming data");
    MethodID methodID;
    MessageID messageID;
    {
        LOGI("Locking");
        Socket::Guard guard = socket.getGuard();
        socket.read(&methodID, sizeof(methodID));
        socket.read(&messageID, sizeof(messageID));
        LOGI("Locked");

        if (methodID == RETURN_METHOD_ID) {
            LOGI("Return value for messageID: " << messageID);
            ReturnCallbacks returnCallbacks;
            try {
                Lock lock(mReturnCallbacksMutex);
                LOGT("Getting the return callback");
                returnCallbacks = std::move(mReturnCallbacks.at(messageID));
                mReturnCallbacks.erase(messageID);
            } catch (const std::out_of_range&) {
                LOGW("No return callback for messageID: " << messageID);
                return false;
            }

            std::shared_ptr<void> data;
            try {
                LOGT("Parsing incoming return data");
                data = returnCallbacks.parse(socket.getFD());
            } catch (const IPCException&) {
                removePeer(peerID);
                return true;
            }

            guard.unlock();

            LOGT("Process callback for methodID: " << methodID << "; messageID: " << messageID);
            returnCallbacks.process(data);

        } else {
            LOGI("Remote call; methodID: " << methodID << " messageID: " << messageID);
            std::shared_ptr<MethodHandlers> methodCallbacks;
            try {
                Lock lock(mCallsMutex);
                methodCallbacks = mMethodsCallbacks.at(methodID);
            } catch (const std::out_of_range&) {
                LOGW("No method callback for methodID: " << methodID);
                removePeer(peerID);
                return true;
            }

            std::shared_ptr<void> data;
            try {
                LOGT("Parsing incoming data");
                data = methodCallbacks->parse(socket.getFD());
            } catch (const IPCException&) {
                removePeer(peerID);
                return true;
            }

            guard.unlock();

            LOGT("Process callback for methodID: " << methodID << "; messageID: " << messageID);
            std::shared_ptr<void> returnData = methodCallbacks->method(data);

            LOGT("Sending return data; methodID: " << methodID << "; messageID: " << messageID);
            try {
                // Send the call with the socket
                Socket::Guard guard = socket.getGuard();
                socket.write(&RETURN_METHOD_ID, sizeof(RETURN_METHOD_ID));
                socket.write(&messageID, sizeof(messageID));
                methodCallbacks->serialize(socket.getFD(), returnData);
            } catch (const IPCException&) {
                removePeer(peerID);
                return true;
            }
        }
    }

    return false;
}

bool Processor::handleEvent()
{
    if (!(mFDs[0].revents & POLLIN)) {
        // No event to serve
        return false;
    }

    mFDs[0].revents &= ~(POLLIN);

    switch (mEventQueue.receive()) {
    case Event::FINISH: {
        LOGD("Event FINISH");
        mIsRunning = false;
        return false;
    }

    case Event::CALL: {
        LOGD("Event CALL");
        handleCall();
        return false;
    }

    case Event::NEW_PEER: {
        LOGD("Event NEW_PEER");
        SocketInfo socketInfo;
        {
            Lock lock(mSocketsMutex);

            socketInfo = std::move(mNewSockets.front());
            mNewSockets.pop();

            if (mSockets.size() > mMaxNumberOfPeers) {
                LOGE("There are too many peers. I don't accept the connection with " << socketInfo.peerID);

            }
            if (mSockets.count(socketInfo.peerID) != 0) {
                LOGE("There already was a socket for peerID: " << socketInfo.peerID);
            }
            mSockets[socketInfo.peerID] = socketInfo.socketPtr;
        }
        resetPolling();

        if (mNewPeerCallback) {
            // Notify about the new user.
            mNewPeerCallback(socketInfo.peerID);
        }
        return true;
    }
    }

    return false;
}

Processor::MessageID Processor::getNextMessageID()
{
    // TODO: This method of generating UIDs is buggy. To be changed.
    return ++mMessageIDCounter;
}

Processor::PeerID Processor::getNextPeerID()
{
    // TODO: This method of generating UIDs is buggy. To be changed.
    return ++mPeerIDCounter;
}

Processor::Call Processor::getCall()
{
    Lock lock(mCallsMutex);
    if (mCalls.empty()) {
        LOGE("Calls queue empty");
        throw IPCException("Calls queue empty");
    }
    Call call = std::move(mCalls.front());
    mCalls.pop();
    return call;
}

void Processor::handleCall()
{
    LOGT("Handle call from another thread");
    Call call = getCall();

    ReturnCallbacks returnCallbacks;
    returnCallbacks.parse = call.parse;
    returnCallbacks.process = call.process;

    std::shared_ptr<Socket> socketPtr;
    try {
        // Get the addressee's socket
        Lock lock(mSocketsMutex);
        socketPtr = mSockets.at(call.peerID);
    } catch (const std::out_of_range&) {
        LOGE("Peer disconnected. No socket with a peerID: " << call.peerID);
        return;
    }

    MessageID messageID = getNextMessageID();

    {
        // Set what to do with the return message
        Lock lock(mReturnCallbacksMutex);
        if (mReturnCallbacks.count(messageID) != 0) {
            LOGE("There already was a return callback for messageID: " << messageID);
        }
        mReturnCallbacks[messageID] = std::move(returnCallbacks);
    }

    try {
        // Send the call with the socket
        Socket::Guard guard = socketPtr->getGuard();
        socketPtr->write(&call.methodID, sizeof(call.methodID));
        socketPtr->write(&messageID, sizeof(messageID));
        call.serialize(socketPtr->getFD(), call.data);
    } catch (const std::exception& e) {
        LOGE("Error during sending a message: " << e.what());
        {
            Lock lock(mReturnCallbacksMutex);
            mReturnCallbacks.erase(messageID);
        }
        // TODO: User should get the error code.
    }
}

} // namespace ipc
} // namespace security_containers