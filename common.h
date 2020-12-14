
#ifndef _COMMON_H_
#define _COMMON_H_

#include <netinet/in.h>
#include <arpa/inet.h>

#include <cstring>

const size_t BUFFER_SIZE = 65535;

struct lb_endpoint_t {
	int family;
	union {
		sockaddr_in6 in6;
		sockaddr_in in4;
	} addr;
	char addr_str[INET6_ADDRSTRLEN * 2];

	int fd;
	uint32_t mark;

	bool is_inbound;

	lb_endpoint_t* peer;

	lb_endpoint_t();
	size_t sockaddr_length()const;
	char* update_addr_str();
	void set_peer(lb_endpoint_t& peer);
};

bool lb_endpoint_validate(const lb_endpoint_t& ep);

struct recv_buffer {
	char buffer[BUFFER_SIZE];
	size_t buffer_size;
	lb_endpoint_t* ep_arrive;
	lb_endpoint_t peer;
};

// convert sockaddr to string
// Multi-thread: race
const char* sockaddr_to_str(const sockaddr* addr);

#endif 