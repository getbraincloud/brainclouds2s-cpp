// Copyright 2026 bitHeads, Inc. All Rights Reserved.
#include "brainclouds2s-rtt.h"
#include "brainclouds2s.h"
#include "ServiceOperation.h"
#include "IServerCallback.h"
#include "RTTComms.h"

#include <iostream>

using namespace BrainCloud;

BrainCloudRTT::BrainCloudRTT(RTTComms *in_comms, S2SContext* c)
        : m_commsLayer(in_comms),
        m_S2SContext(c) {
}

void BrainCloudRTT::requestS2SConnection(IServerCallback* in_callback)
{
    Json::Value nullMsg = Json::nullValue;
    bool processed = false;
    std::string ret;

    // Build the rtt registration json
    Json::Value json(Json::ValueType::objectValue);
    json["service"] = "rttRegistration";
    json["operation"] = "REQUEST_SYSTEM_CONNECTION";

    Json::StyledWriter writer;
    printf("%s",writer.write(json).c_str());

    Json::FastWriter fw;

    m_S2SContext->request(fw.write(json), [this, in_callback](const std::string& result)
    {
        in_callback->serverCallback(ServiceName::RTTRegistration, ServiceOperation::RequestSystemConnection, result);
    });
}

// RTT stuff
void BrainCloudRTT::enableRTT(IRTTConnectCallback* in_callback, bool in_useWebSocket)
{
    m_commsLayer->enableRTT(in_callback, in_useWebSocket);
}

void BrainCloudRTT::disableRTT()
{
    m_commsLayer->disableRTT();
}

void BrainCloudRTT::registerRTTCallback(const ServiceName& serviceName, IRTTCallback* in_callback)
{
    m_commsLayer->registerRTTCallback(serviceName, in_callback);
}

void BrainCloudRTT::deregisterRTTCallback(const ServiceName& serviceName)
{
    m_commsLayer->deregisterRTTCallback(serviceName);
}

void BrainCloudRTT::deregisterAllRTTCallbacks()
{
    m_commsLayer->deregisterAllRTTCallbacks();
}



bool BrainCloudRTT::getRTTEnabled()
{
    return m_commsLayer->isRTTEnabled();
}

BrainCloudRTT::RTTConnectionStatus BrainCloudRTT::getConnectionStatus()
{
    return m_commsLayer->getConnectionStatus();
}

const std::string& BrainCloudRTT::getRTTConnectionId() const
{
    return m_commsLayer->getConnectionId();
}

