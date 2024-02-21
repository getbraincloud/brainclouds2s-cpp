#include "brainclouds2s-rtt.h"
#include "brainclouds2s.h"
#include "RTTComms.h"
#include "IServerCallback.h"

namespace BrainCloud {
    BrainCloudRTT::BrainCloudRTT(RTTComms *in_comms, S2SContext* c)
            : m_commsLayer(in_comms),
            m_S2SContext(c) {
    }

    void BrainCloudRTT::requestS2SConnection(IServerCallback* in_callback)
    {
        Json::Value nullMsg = Json::nullValue;
        bool processed = false;
        std::string ret;
        //ServerCall* sc = new ServerCall(ServiceName::RTTRegistration, ServiceOperation::RequestClientConnection, nullMsg, in_callback);
        //m_client->getBrainCloudComms()->addToQueue(sc);


        // Build the authentication json
        Json::Value json(Json::ValueType::objectValue);
        //json["packetId"] = 0;
        //Json::Value messages(Json::ValueType::arrayValue);
        //Json::Value message(Json::ValueType::objectValue);
        json["service"] = "rttRegistration";
        json["operation"] = "REQUEST_SYSTEM_CONNECTION";
        //json["data"] = nullMsg;
        //messages.append(message);
        //json["messages"] = messages;

        Json::StyledWriter writer;
        printf("%s",writer.write(json).c_str());

        Json::FastWriter fw;
        m_S2SContext->request(fw.write(json), [&](const std::string& result)
        {
            ret = result;
            processed = true;
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
}