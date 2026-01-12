// Copyright 2026 bitHeads, Inc. All Rights Reserved.
#include "ServiceName.h"

namespace BrainCloud
{
    const ServiceName ServiceName::None = ServiceName("None");
    const ServiceName ServiceName::Authenticate = ServiceName("authentication");
	const ServiceName ServiceName::RTTRegistration = ServiceName("rttRegistration");
    const ServiceName ServiceName::RTT = ServiceName("rtt");
    const ServiceName ServiceName::Chat = ServiceName("chat");
    const ServiceName ServiceName::Messaging = ServiceName("messaging");
    const ServiceName ServiceName::Lobby = ServiceName("lobby");
    const ServiceName ServiceName::Relay = ServiceName("relay");


    ServiceName::ServiceName(const char * value)
    {
        _value = value;
    }

    bool ServiceName::operator== (const ServiceName& s) const
    {
        return _value == s.getValue();
    }

    bool ServiceName::operator!= (const ServiceName& s) const
    {
        return _value != s.getValue();
    }

    void ServiceName::operator= (const ServiceName& s)
    {
        _value = s.getValue();
    }
}