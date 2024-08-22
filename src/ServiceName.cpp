#include "ServiceName.h"

namespace BrainCloud
{
    const ServiceName ServiceName::None = ServiceName("None");

    const ServiceName ServiceName::Authenticate = ServiceName("authentication");

	const ServiceName ServiceName::RTTRegistration = ServiceName("rttRegistration");

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