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
 * @brief   File descriptor utility functions
 */

#include "config.hpp"

#include "utils/fd-utils.hpp"
#include "utils/exception.hpp"
#include "logger/logger.hpp"

#include <cerrno>
#include <cstring>
#include <chrono>
#include <unistd.h>
#include <poll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;
namespace chr = std::chrono;

namespace utils {

// TODO: Add here various fixes from config::FDStore

namespace {

void waitForEvent(int fd,
                  short event,
                  const chr::high_resolution_clock::time_point deadline)
{
    // Wait for the rest of the data
    struct pollfd fds[1];
    fds[0].fd = fd;
    fds[0].events = event;

    for (;;) {
        chr::milliseconds timeoutMS = chr::duration_cast<chr::milliseconds>(deadline - chr::high_resolution_clock::now());
        if (timeoutMS.count() < 0) {
            LOGE("Timeout while waiting for event: " << std::hex << event <<
                 " on fd: " << std::dec << fd);
            throw UtilsException("Timeout");
        }

        int ret = ::poll(fds, 1 /*fds size*/, timeoutMS.count());

        if (ret == -1) {
            if (errno == EINTR) {
                continue;
            }
            const std::string msg = getSystemErrorMessage();
            LOGE("Error in poll: " + msg);
            throw UtilsException("Error in poll: " + msg);
        }

        if (ret == 0) {
            LOGE("Timeout in read");
            throw UtilsException("Timeout in read");
        }

        if (fds[0].revents & event) {
            // Here Comes the Sun
            break;
        }

        if (fds[0].revents & POLLHUP) {
            LOGW("Peer disconnected");
            throw UtilsException("Peer disconnected");
        }
    }
}

} // namespace

void close(int fd)
{
    if (fd < 0) {
        return;
    }

    for (;;) {
        if (-1 == ::close(fd)) {
            if (errno == EINTR) {
                LOGT("Close interrupted by a signal, retrying");
                continue;
            }
            LOGE("Error in close: " << getSystemErrorMessage());
        }
        break;
    }
}

void write(int fd, const void* bufferPtr, const size_t size, int timeoutMS)
{
    chr::high_resolution_clock::time_point deadline = chr::high_resolution_clock::now() +
            chr::milliseconds(timeoutMS);

    size_t nTotal = 0;
    for (;;) {
        int n  = ::write(fd,
                         reinterpret_cast<const char*>(bufferPtr) + nTotal,
                         size - nTotal);
        if (n >= 0) {
            nTotal += n;
            if (nTotal == size) {
                // All data is written, break loop
                break;
            }
        } else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            // Neglected errors
            LOGD("Retrying write");
        } else {
            const std::string msg = getSystemErrorMessage();
            LOGE("Error during writing: " + msg);
            throw UtilsException("Error during writing: " + msg);
        }

        waitForEvent(fd, POLLOUT, deadline);
    }
}

void read(int fd, void* bufferPtr, const size_t size, int timeoutMS)
{
    chr::high_resolution_clock::time_point deadline = chr::high_resolution_clock::now() +
            chr::milliseconds(timeoutMS);

    size_t nTotal = 0;
    for (;;) {
        int n  = ::read(fd,
                        reinterpret_cast<char*>(bufferPtr) + nTotal,
                        size - nTotal);
        if (n >= 0) {
            nTotal += n;
            if (nTotal == size) {
                // All data is read, break loop
                break;
            }
            if (n == 0) {
                LOGW("Peer disconnected");
                throw UtilsException("Peer disconnected");
            }
        } else if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            // Neglected errors
            LOGD("Retrying read");
        } else {
            const std::string msg = getSystemErrorMessage();
            LOGE("Error during reading: " + msg);
            throw UtilsException("Error during reading: " + msg);
        }

        waitForEvent(fd, POLLIN, deadline);
    }
}

unsigned int getMaxFDNumber()
{
    struct rlimit rlim;
    if (-1 ==  getrlimit(RLIMIT_NOFILE, &rlim)) {
        const std::string msg = getSystemErrorMessage();
        LOGE("Error during getrlimit: " + msg);
        throw UtilsException("Error during getrlimit: " + msg);
    }
    return rlim.rlim_cur;
}

void setMaxFDNumber(unsigned int limit)
{
    struct rlimit rlim;
    rlim.rlim_cur = limit;
    rlim.rlim_max = limit;
    if (-1 ==  setrlimit(RLIMIT_NOFILE, &rlim)) {
        const std::string msg = getSystemErrorMessage();
        LOGE("Error during setrlimit: " + msg);
        throw UtilsException("Error during setrlimit: " + msg);
    }
}

unsigned int getFDNumber()
{
    const std::string path = "/proc/self/fd/";
    return std::distance(fs::directory_iterator(path),
                         fs::directory_iterator());
}

int fdRecv(int socket, const unsigned int timeoutMS)
{
    std::chrono::high_resolution_clock::time_point deadline =
        std::chrono::high_resolution_clock::now() +
        std::chrono::milliseconds(timeoutMS);

    // Space for the file descriptor
    union {
        struct cmsghdr cmh;
        char   control[CMSG_SPACE(sizeof(int))];
    } controlUnion;

    // Describe the data that we want to recive
    controlUnion.cmh.cmsg_len = CMSG_LEN(sizeof(int));
    controlUnion.cmh.cmsg_level = SOL_SOCKET;
    controlUnion.cmh.cmsg_type = SCM_RIGHTS;

    // Setup the input buffer
    // Ensure at least 1 byte is transmited via the socket
    char buf;
    struct iovec iov;
    iov.iov_base = &buf;
    iov.iov_len = sizeof(char);

    // Set the ancillary data buffer
    // The socket has to be connected, so we don't need to specify the name
    struct msghdr msgh;
    ::memset(&msgh, 0, sizeof(msgh));

    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;

    msgh.msg_control = controlUnion.control;
    msgh.msg_controllen = sizeof(controlUnion.control);

    // Receive
    for(;;) {
        ssize_t ret = ::recvmsg(socket, &msgh, MSG_WAITALL);
        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                // Neglected errors, retry
            } else {
                throw UtilsException("Error during recvmsg: " + getSystemErrorMessage());
            }
        } else if (ret == 0) {
            throw UtilsException("Peer disconnected");
        } else {
            // We receive only 1 byte of data. No need to repeat
            break;
        }

        waitForEvent(socket, POLLIN, deadline);
    }

    struct cmsghdr *cmhp;
    cmhp = CMSG_FIRSTHDR(&msgh);
    if (cmhp == NULL || cmhp->cmsg_len != CMSG_LEN(sizeof(int))) {
        throw UtilsException("Bad cmsg length");
    } else if (cmhp->cmsg_level != SOL_SOCKET) {
        throw UtilsException("cmsg_level != SOL_SOCKET");
    } else if (cmhp->cmsg_type != SCM_RIGHTS) {
        throw UtilsException("cmsg_type != SCM_RIGHTS");
    }

    return *(reinterpret_cast<int*>(CMSG_DATA(cmhp)));
}

bool fdSend(int socket, int fd, const unsigned int timeoutMS)
{
    std::chrono::high_resolution_clock::time_point deadline =
        std::chrono::high_resolution_clock::now() +
        std::chrono::milliseconds(timeoutMS);

    // Space for the file descriptor
    union {
        struct cmsghdr cmh;
        char   control[CMSG_SPACE(sizeof(int))];
    } controlUnion;

    // Ensure at least 1 byte is transmited via the socket
    struct iovec iov;
    char buf = '!';
    iov.iov_base = &buf;
    iov.iov_len = sizeof(char);

    // Fill the message to send:
    // The socket has to be connected, so we don't need to specify the name
    struct msghdr msgh;
    ::memset(&msgh, 0, sizeof(msgh));

    // Only iovec to transmit one element
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;

    // Ancillary data buffer
    msgh.msg_control = controlUnion.control;
    msgh.msg_controllen = sizeof(controlUnion.control);

    // Describe the data that we want to send
    struct cmsghdr *cmhp;
    cmhp = CMSG_FIRSTHDR(&msgh);
    cmhp->cmsg_len = CMSG_LEN(sizeof(int));
    cmhp->cmsg_level = SOL_SOCKET;
    cmhp->cmsg_type = SCM_RIGHTS;
    *(reinterpret_cast<int*>(CMSG_DATA(cmhp))) = fd;

    // Send
    for(;;) {
        ssize_t ret = ::sendmsg(socket, &msgh, MSG_NOSIGNAL);
        if (ret < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                // Neglected errors, retry
            } else {
                throw UtilsException("Error during sendmsg: " + getSystemErrorMessage());
            }
        } else if (ret == 0) {
            // Retry the sending
        } else {
            // We send only 1 byte of data. No need to repeat
            break;
        }

        waitForEvent(socket, POLLOUT, deadline);
    }

    // TODO: It shouldn't return
    return true;
}

} // namespace utils
