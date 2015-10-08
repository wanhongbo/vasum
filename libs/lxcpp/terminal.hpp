/*
 *  Copyright (C) 2015 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License version 2.1 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file
 * @author  Lukasz Pawelczyk (l.pawelczyk@samsung.com)
 * @brief   Terminal helpers headers
 */

#ifndef LXCPP_TERMINAL_HPP
#define LXCPP_TERMINAL_HPP

#include <termios.h>


namespace lxcpp {


/**
 * Nullifies all standard file descriptors (stdin, stdout, stderr)
 * replacing them with file descriptor to /dev/null. Used to
 * as a part of a process to detach a process from a control terminal.
 *
 * This function has to be safe in regard to signal(7)
 *
 * @returns an error code in case of failure.
 */
int nullStdFDs();

/**
 * Checks if a file descriptor is a terminal.
 */
bool isatty(int fd);

/**
 * Get terminal attributes.
 */
void tcgetattr(const int fd, struct termios *termios_p);

/**
 * Set terminal attributes.
 */
void tcsetattr(const int fd, const int optional_actions, const struct termios *termios_p);

/**
 * Setups the passed fd as a new control and IO (in, out, err) terminal
 */
void setupIOControlTTY(const int ttyFD);

/**
 * This function creates a new pair of virtual character devices
 * using a pseudtoreminal interface. It also configures as much as it
 * can so the devices are immediately usable.
 *
 * @param rawMode  Whether to set the terminal in the raw mode (termios(2))
 *
 * @returns file descriptor to the master device and the path/name of
 *          the pts slace device.
 */
std::pair<int, std::string> openPty(bool rawMode);


} // namespace lxcpp


#endif // LXCPP_TERMINAL_HPP
