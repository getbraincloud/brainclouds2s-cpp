// Copyright 2026 bitHeads, Inc. All Rights Reserved.

#pragma once
#include "json/json.h"
#include <string>

#include <fstream>
#include <iostream>
#include <thread>
#include <sstream>
#include "ServiceName.h"

namespace BrainCloud
{
    class RTTComms;
    class IRTTConnectCallback;
    class IRTTCallback;
    class IServerCallback;
    class S2SContext;

    class BrainCloudRTT
    {
    public:
        BrainCloudRTT(RTTComms* in_comms, S2SContext* c);

        enum RTTConnectionStatus
        {
            Connected,
            Disconnected,
            Connecting,
            Disconnecting
        };

        /**
            * Requests the event server address
            *
            * @param callback The callback.
            */
        void requestS2SConnection(IServerCallback* in_callback);

        /**
            * Enables Real Time event for this session.
            * Real Time events are disabled by default. Usually events
            * need to be polled using GET_EVENTS. By enabling this, events will
            * be received instantly when they happen through a TCP connection to an Event Server.
            *
            * This function will first call requestS2SConnection, then connect to the address
            *
            * @param callback The callback.
            * @param useWebSocket Use web sockets instead of TCP for the internal connections. Default is true
            */
        void enableRTT(IRTTConnectCallback* in_callback, bool in_useWebSocket = true);

        /**
            * Disables Real Time event for this session.
            */
        void disableRTT();

        void registerRTTCallback(const ServiceName& serviceName, IRTTCallback* in_callback);
        void deregisterRTTCallback(const ServiceName& serviceName);
        void deregisterAllRTTCallbacks();

        /**
            *returns true if RTT is enabled
            */
        bool getRTTEnabled();

        /**
            *returns rtt connection Status
            */
        BrainCloudRTT::RTTConnectionStatus getConnectionStatus();

        const std::string& getRTTConnectionId() const;

    private:
        RTTComms* m_commsLayer;
        S2SContext* m_S2SContext;
    };
}
