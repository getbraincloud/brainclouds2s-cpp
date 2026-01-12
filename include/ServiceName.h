// Copyright 2026 bitHeads, Inc. All Rights Reserved.
#pragma once

#include <string>

namespace BrainCloud {

    class ServiceName
    {
    public:
        static const ServiceName None;

        static const ServiceName Authenticate;

		static const ServiceName RTTRegistration;
        static const ServiceName RTT;
        static const ServiceName Chat;
        static const ServiceName Messaging;
        static const ServiceName Lobby;

        // RS
        static const ServiceName Relay;
        
        std::string getValue() const { return _value; }

        bool operator== (const ServiceName& s) const;
        bool operator!= (const ServiceName& s) const;
        void operator= (const ServiceName& s);

    private:
        std::string _value;

        ServiceName(const char * value);
    };

}
