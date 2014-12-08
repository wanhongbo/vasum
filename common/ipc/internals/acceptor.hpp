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
 * @brief   Class for accepting new connections
 */

#ifndef COMMON_IPC_INTERNALS_ACCEPTOR_HPP
#define COMMON_IPC_INTERNALS_ACCEPTOR_HPP

#include "config.hpp"

#include "ipc/internals/socket.hpp"
#include "ipc/internals/event-queue.hpp"
#include "ipc/types.hpp"

#include <string>
#include <thread>

namespace vasum {
namespace ipc {

/**
 * Accepts new connections and passes the new socket to a callback.
 */
class Acceptor {
public:

    typedef std::function<void(std::shared_ptr<Socket>& socketPtr)> NewConnectionCallback;

    /**
     * Class for accepting new connections.
     *
     * @param socketPath path to the socket
     * @param newConnectionCallback called on new connections
     */
    Acceptor(const std::string& socketPath,
             const NewConnectionCallback& newConnectionCallback);
    ~Acceptor();

    Acceptor(const Acceptor& acceptor) = delete;
    Acceptor& operator=(const Acceptor&) = delete;

    /**
     * Starts the thread accepting the new connections.
     */
    void start();

    /**
     * Stops the accepting thread.
     */
    void stop();

    /**
     * Handle one incoming connection.
     * Used with external polling
     */
    void handleConnection();

    /**
     * Handle one event from the internal event's queue
     * Used with external polling
     */
    void handleEvent();

    /**
     * @return file descriptor of internal event's queue
     */
    FileDescriptor getEventFD();

    /**
     * @return file descriptor for the connection socket
     */
    FileDescriptor getConnectionFD();

private:
    enum class Event : int {
        FINISH  // Shutdown request
    };

    bool mIsRunning;

    NewConnectionCallback mNewConnectionCallback;
    Socket mSocket;

    EventQueue<Event> mEventQueue;
    std::thread mThread;

    void run();
};

} // namespace ipc
} // namespace vasum

#endif // COMMON_IPC_INTERNALS_ACCEPTOR_HPP
