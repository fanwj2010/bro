#ifndef BROKER_ENDPOINTIMPL_HH
#define BROKER_ENDPOINTIMPL_HH

#include "broker/Endpoint.hh"

#include <cppa/cppa.hpp>

#include <unordered_map>
#include <string>

namespace broker {

class Endpoint::Impl {
public:

	std::string name;
	cppa::actor endpoint;
	std::unordered_map<cppa::actor, Peer> peers;
	int last_errno;
	std::string last_error;
};
} // namespace broker

#endif // BROKER_ENDPOINTIMPL_HH