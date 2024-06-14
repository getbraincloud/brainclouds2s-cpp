#pragma once

#include <string>

namespace BrainCloud {

    class ServiceName
    {
    public:
        static const ServiceName None;

        static const ServiceName Authenticate;

		static const ServiceName RTTRegistration;
        
        std::string getValue() const { return _value; }

        bool operator== (const ServiceName& s) const;
        bool operator!= (const ServiceName& s) const;
        void operator= (const ServiceName& s);

    private:
        std::string _value;

        ServiceName(const char * value);
    };

}
