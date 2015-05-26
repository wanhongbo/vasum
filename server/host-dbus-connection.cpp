/*
 *  Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *  Contact: Piotr Bartosiewicz <p.bartosiewi@partner.samsung.com>
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
 * @author  Piotr Bartosiewicz (p.bartosiewi@partner.samsung.com)
 * @brief   Implementation of a class for communication with server
 */

#include "config.hpp"

#include "host-dbus-connection.hpp"
#include "host-dbus-definitions.hpp"
#include "exception.hpp"
#include "api/dbus-method-result-builder.hpp"
#include "api/messages.hpp"

#include "logger/logger.hpp"
#include "config/manager.hpp"
#include "zones-manager.hpp"

namespace vasum {

namespace {

// Timeout in ms for waiting for dbus name.
// Can happen if glib loop is busy or not present.
// TODO: this should be in host's configuration file
const unsigned int NAME_ACQUIRED_TIMEOUT = 5 * 1000;
const std::string EMPTY_CALLER = "";

} // namespace


HostDbusConnection::HostDbusConnection(ZonesManager* zonesManagerPtr)
    : mNameAcquired(false)
    , mNameLost(false)
    , mZonesManagerPtr(zonesManagerPtr)
{
    LOGT("Connecting to host system DBUS");
    mDbusConnection = dbus::DbusConnection::createSystem();

    LOGT("Setting DBUS name");
    mDbusConnection->setName(api::dbus::BUS_NAME,
                             std::bind(&HostDbusConnection::onNameAcquired, this),
                             std::bind(&HostDbusConnection::onNameLost, this));

    if (!waitForName(NAME_ACQUIRED_TIMEOUT)) {
        LOGE("Could not acquire dbus name: " << api::dbus::BUS_NAME);
        throw HostConnectionException("Could not acquire dbus name: " + api::dbus::BUS_NAME);
    }

    LOGT("Registering DBUS interface");
    using namespace std::placeholders;
    mDbusConnection->registerObject(api::dbus::OBJECT_PATH,
                                    api::dbus::DEFINITION,
                                    std::bind(&HostDbusConnection::onMessageCall,
                                              this, _1, _2, _3, _4, _5));

    mSubscriptionId = mDbusConnection->signalSubscribe(std::bind(&HostDbusConnection::onSignalCall,
                                                                 this, _1, _2, _3, _4, _5),
                                                       std::string(),
                                                       api::dbus::INTERFACE);
    LOGD("Connected");
}

HostDbusConnection::~HostDbusConnection()
{
    mDbusConnection->signalUnsubscribe(mSubscriptionId);
}

bool HostDbusConnection::waitForName(const unsigned int timeoutMs)
{
    std::unique_lock<std::mutex> lock(mNameMutex);
    mNameCondition.wait_for(lock,
                            std::chrono::milliseconds(timeoutMs),
    [this] {
        return mNameAcquired || mNameLost;
    });

    return mNameAcquired;
}

void HostDbusConnection::onNameAcquired()
{
    std::unique_lock<std::mutex> lock(mNameMutex);
    mNameAcquired = true;
    mNameCondition.notify_one();
}

void HostDbusConnection::onNameLost()
{
    std::unique_lock<std::mutex> lock(mNameMutex);
    mNameLost = true;
    mNameCondition.notify_one();

    if (mNameAcquired) {
        // TODO implement reconnecting
        LOGE("TODO Reconnect !!!");
    }
}

void HostDbusConnection::setProxyCallCallback(const ProxyCallCallback& callback)
{
    mProxyCallCallback = callback;
}

void HostDbusConnection::onMessageCall(const std::string& objectPath,
                                   const std::string& interface,
                                   const std::string& methodName,
                                   GVariant* parameters,
                                   dbus::MethodResultBuilder::Pointer result)
{
    if (objectPath != api::dbus::OBJECT_PATH || interface != api::dbus::INTERFACE) {
        return;
    }

    if (methodName == api::dbus::METHOD_SET_ACTIVE_ZONE) {
        api::ZoneId zoneId;
        config::loadFromGVariant(parameters, zoneId);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleSetActiveZoneCall(zoneId, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_PROXY_CALL) {
        const gchar* target = NULL;
        const gchar* targetBusName = NULL;
        const gchar* targetObjectPath = NULL;
        const gchar* targetInterface = NULL;
        const gchar* targetMethod = NULL;
        GVariant* rawArgs = NULL;
        g_variant_get(parameters,
                      "(&s&s&s&s&sv)",
                      &target,
                      &targetBusName,
                      &targetObjectPath,
                      &targetInterface,
                      &targetMethod,
                      &rawArgs);
        dbus::GVariantPtr args(rawArgs, g_variant_unref);

        if (mProxyCallCallback) {
            mProxyCallCallback(target,
                               targetBusName,
                               targetObjectPath,
                               targetInterface,
                               targetMethod,
                               args.get(),
                               result);
        }
        return;
    }

    if (methodName == api::dbus::METHOD_GET_ZONE_ID_LIST) {
        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::ZoneIds>>(result);
        mZonesManagerPtr->handleGetZoneIdsCall(rb);
        return;
    }

    if (methodName == api::dbus::METHOD_GET_ACTIVE_ZONE_ID) {
        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::ZoneId>>(result);
        mZonesManagerPtr->handleGetActiveZoneIdCall(rb);
        return;
    }

    if (methodName == api::dbus::METHOD_GET_ZONE_INFO) {
        api::ZoneId zoneId;
        config::loadFromGVariant(parameters, zoneId);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::ZoneInfoOut>>(result);
        mZonesManagerPtr->handleGetZoneInfoCall(zoneId, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_SET_NETDEV_ATTRS) {
        api::SetNetDevAttrsIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleSetNetdevAttrsCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_GET_NETDEV_ATTRS) {
        api::GetNetDevAttrsIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::GetNetDevAttrs>>(result);
        mZonesManagerPtr->handleGetNetdevAttrsCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_GET_NETDEV_LIST) {
        api::ZoneId data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::NetDevList>>(result);
        mZonesManagerPtr->handleGetNetdevListCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_CREATE_NETDEV_VETH) {
        api::CreateNetDevVethIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleCreateNetdevVethCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_CREATE_NETDEV_MACVLAN) {
        api::CreateNetDevMacvlanIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleCreateNetdevMacvlanCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_CREATE_NETDEV_PHYS) {
        api::CreateNetDevPhysIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleCreateNetdevPhysCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_DESTROY_NETDEV) {
        api::DestroyNetDevIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleDestroyNetdevCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_DELETE_NETDEV_IP_ADDRESS) {
        api::DeleteNetdevIpAddressIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleDeleteNetdevIpAddressCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_DECLARE_FILE) {
        api::DeclareFileIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Declaration>>(result);
        mZonesManagerPtr->handleDeclareFileCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_DECLARE_MOUNT) {
        api::DeclareMountIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Declaration>>(result);
        mZonesManagerPtr->handleDeclareMountCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_DECLARE_LINK) {
        api::DeclareLinkIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Declaration>>(result);
        mZonesManagerPtr->handleDeclareLinkCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_GET_DECLARATIONS) {
        api::ZoneId data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Declarations>>(result);
        mZonesManagerPtr->handleGetDeclarationsCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_REMOVE_DECLARATION) {
        api::RemoveDeclarationIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleRemoveDeclarationCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_CREATE_ZONE) {
        api::CreateZoneIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleCreateZoneCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_DESTROY_ZONE) {
        api::ZoneId data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleDestroyZoneCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_SHUTDOWN_ZONE) {
        api::ZoneId data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleShutdownZoneCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_START_ZONE) {
        api::ZoneId data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleStartZoneCall(data, rb);
    }

    if (methodName == api::dbus::METHOD_LOCK_ZONE) {
        api::ZoneId data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleLockZoneCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_UNLOCK_ZONE) {
        api::ZoneId data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleUnlockZoneCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_GRANT_DEVICE) {
        api::GrantDeviceIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleGrantDeviceCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_REVOKE_DEVICE) {
        api::RevokeDeviceIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleRevokeDeviceCall(data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_NOTIFY_ACTIVE_ZONE) {
        api::NotifActiveZoneIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::Void>>(result);
        mZonesManagerPtr->handleNotifyActiveZoneCall(EMPTY_CALLER, data, rb);
        return;
    }

    if (methodName == api::dbus::METHOD_FILE_MOVE_REQUEST) {
        api::FileMoveRequestIn data;
        config::loadFromGVariant(parameters, data);

        auto rb = std::make_shared<api::DbusMethodResultBuilder<api::FileMoveRequestStatus>>(result);
        mZonesManagerPtr->handleFileMoveCall(EMPTY_CALLER, data, rb);
        return;
    }
}

void HostDbusConnection::onSignalCall(const std::string& /* senderBusName */,
                                      const std::string& objectPath,
                                      const std::string& interface,
                                      const std::string& signalName,
                                      GVariant* /* parameters */)
{
    if (objectPath != api::dbus::OBJECT_PATH || interface != api::dbus::INTERFACE) {
        return;
    }

    if (signalName == api::dbus::SIGNAL_SWITCH_TO_DEFAULT) {
        mZonesManagerPtr->handleSwitchToDefaultCall(EMPTY_CALLER);
    }
}

void HostDbusConnection::proxyCallAsync(const std::string& busName,
                                    const std::string& objectPath,
                                    const std::string& interface,
                                    const std::string& method,
                                    GVariant* parameters,
                                    const dbus::DbusConnection::AsyncMethodCallCallback& callback)
{
    mDbusConnection->callMethodAsync(busName,
                                     objectPath,
                                     interface,
                                     method,
                                     parameters,
                                     std::string(),
                                     callback);
}

void HostDbusConnection::sendNotification(const api::Notification& notify)
{
    GVariant* parameters = g_variant_new("(sss)",
                                         notify.zone.c_str(),
                                         notify.application.c_str(),
                                         notify.message.c_str());
    mDbusConnection->emitSignal(api::dbus::OBJECT_PATH,
                                api::dbus::INTERFACE,
                                api::dbus::SIGNAL_NOTIFICATION,
                                parameters);
}

} // namespace vasum
