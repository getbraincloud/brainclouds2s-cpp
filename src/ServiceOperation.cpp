// Copyright 2026 bitHeads, Inc. All Rights Reserved.
#include "ServiceOperation.h"

namespace BrainCloud
{
	const ServiceOperation ServiceOperation::None = ServiceOperation("NONE");

	const ServiceOperation ServiceOperation::Authenticate = ServiceOperation("AUTHENTICATE");

	//rtt registration
	const ServiceOperation ServiceOperation::RequestSystemConnection = ServiceOperation("REQUEST_SYSTEM_CONNECTION");

	ServiceOperation::ServiceOperation(const char * value)
	{
		_value = value;
	}

	bool ServiceOperation::operator== (const ServiceOperation& s) const
	{
		return _value == s.getValue();
	}

	bool ServiceOperation::operator!= (const ServiceOperation& s) const
	{
		return _value != s.getValue();
	}

	void ServiceOperation::operator= (const ServiceOperation& s)
	{
		_value = s.getValue();
	}

}
