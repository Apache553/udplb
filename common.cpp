
#include "common.h"

#include <cstdio>

bool lb_endpoint_validate(const lb_endpoint_t& ep) {
	if (ep.family == AF_INET) {
		if (ep.addr.in4.sin_family == AF_INET && ep.addr.in4.sin_port != 0)return true;
	}
	else if (ep.family == AF_INET6) {
		if (ep.addr.in6.sin6_family == AF_INET && ep.addr.in6.sin6_port != 0)return true;
	}
	return false;
}

const char* sockaddr_to_str(const sockaddr* addr) {
	static char buffer[INET6_ADDRSTRLEN];
	static char buffer_ret[INET6_ADDRSTRLEN * 2];

	if (addr->sa_family == AF_INET) {
		// ipv4
		const sockaddr_in* addr4 = (const sockaddr_in*)addr;
		if (::inet_ntop(AF_INET, (const void*)&addr4->sin_addr, buffer, INET6_ADDRSTRLEN)) {
			// success
			snprintf(buffer_ret, INET6_ADDRSTRLEN * 2, "%s:%d", buffer, ntohs(addr4->sin_port));
			return buffer_ret;
		}
	}
	else if (addr->sa_family == AF_INET6) {
		const sockaddr_in6* addr6 = (const sockaddr_in6*)addr;
		if (::inet_ntop(AF_INET6, (const void*)&addr6->sin6_addr, buffer, INET6_ADDRSTRLEN)) {
			// success
			snprintf(buffer_ret, INET6_ADDRSTRLEN * 2, "[%s]:%d", buffer, ntohs(addr6->sin6_port));
			return buffer_ret;
		}
	}
	strncpy(buffer_ret, "<INVALID ADDRESS>", INET6_ADDRSTRLEN * 2);
	return buffer_ret;
}


lb_endpoint_t::lb_endpoint_t() :family(-1), fd(-1), mark(0), is_inbound(false), peer(nullptr) {
	memset(&addr, 0, sizeof(addr));
}

size_t lb_endpoint_t::sockaddr_length()const {
	if (family == AF_INET)return sizeof(addr.in4);
	else if (family == AF_INET6)return sizeof(addr.in6);
	else return 0;
}

char* lb_endpoint_t::update_addr_str() {
	strncpy(addr_str, sockaddr_to_str((const sockaddr*)&addr), INET6_ADDRSTRLEN * 2);
	return addr_str;
}

void lb_endpoint_t::set_peer(lb_endpoint_t& peer)
{
	this->peer->family = this->family;
	this->peer->fd = this->fd;
	this->peer->mark = this->mark;
	this->peer->addr = peer.addr;
	strcpy(this->peer->addr_str, peer.addr_str);
}
