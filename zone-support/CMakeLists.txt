# Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#
#
# @file   CMakeLists.txt
# @author Piotr Bartosiewicz (p.bartosiewi@partner.samsung.com)
#

MESSAGE(STATUS "")
MESSAGE(STATUS "Generating makefile for the Zone Support...")


## Install #####################################################################

CONFIGURE_FILE(configs/org.tizen.vasum.zone.conf.in
               ${CMAKE_BINARY_DIR}/dbus-1/system.d/org.tizen.vasum.zone.conf)

INSTALL(FILES       ${CMAKE_BINARY_DIR}/dbus-1/system.d/org.tizen.vasum.zone.conf
        DESTINATION /etc/dbus-1/system.d/)
