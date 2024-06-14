#pragma once

#include <string>

namespace BrainCloud {

	class ServiceOperation
	{
	public:
		static const ServiceOperation None;

		static const ServiceOperation Authenticate;

		//rtt Registration
		static const ServiceOperation RequestSystemConnection;

		std::string getValue() const { return _value; }

		bool operator== (const ServiceOperation& s) const;
		bool operator!= (const ServiceOperation& s) const;
		void operator= (const ServiceOperation& s);

	private:
		std::string _value;

		ServiceOperation(const char * value);
	};

}