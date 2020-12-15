


#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/epoll.h>

#include <unistd.h>

#include <cstring>
#include <cstdint>
#include <limits>
#include <cassert>
#include <csignal>
#include <stdexcept>
#include <string>
#include <vector>

#include "common.h"
#include "args.h"
#include "logger.h"

struct forward_status_t {
	size_t inbound_poll_idx = 0;
	size_t outbound_poll_idx = 0;
	args_t* args = nullptr;
};


int lb_endpoint_set_sockopt(lb_endpoint_t& ep) {
	int ret;
	int value = 16777216;
	if (ep.fd == -1)return -EINVAL;
	if ((ret = ::fcntl(ep.fd, F_SETFL, O_NONBLOCK)) < 0) {
		return -errno;
	}
	if ((ret = ::setsockopt(ep.fd, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value))) < 0) {
		close(ep.fd);
		ep.fd = -1;
		return -errno;
	}
	if ((ret = ::setsockopt(ep.fd, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value))) < 0) {
		close(ep.fd);
		ep.fd = -1;
		return -errno;
	}
	return 0;
}

int lb_endpoint_listen(lb_endpoint_t& ep) {
	if (!lb_endpoint_validate(ep))return -EINVAL;
	if (ep.fd != -1)return -EALREADY;
	if ((ep.fd = ::socket(ep.family, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		return -errno;
	}
	if (int ret = lb_endpoint_set_sockopt(ep)) {
		close(ep.fd);
		ep.fd = -1;
		return ret;
	}
	if (::bind(ep.fd, (const sockaddr*)&ep.addr, ep.sockaddr_length()) < 0) {
		close(ep.fd);
		ep.fd = -1;
		return -errno;
	}
	return 0;
}

int lb_endpoint_connect(lb_endpoint_t& ep) {
	if (!lb_endpoint_validate(ep))return -EINVAL;
	if (ep.fd != -1)return -EALREADY;
	if ((ep.fd = ::socket(ep.family, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		return -errno;
	}
	if (int ret = lb_endpoint_set_sockopt(ep)) {
		close(ep.fd);
		ep.fd = -1;
		return ret;
	}
	if (::connect(ep.fd, (const sockaddr*)&ep.addr, ep.sockaddr_length()) < 0) {
		close(ep.fd);
		ep.fd = -1;
		return -errno;
	}
	if (ep.mark != 0) {
		Logger::getLogger().info("set mark for %s: 0x%x\n", ep.addr_str, ep.mark);
		if (::setsockopt(ep.fd, SOL_SOCKET, SO_MARK, &ep.mark, sizeof(ep.mark)) < 0) {
			close(ep.fd);
			ep.fd = -1;
			return -errno;
		}
	}
	return 0;
}

int do_forward(recv_buffer& buffer, forward_status_t& status) {
	lb_endpoint_t* dst = nullptr;
	lb_endpoint_t* send_ep = nullptr;
	if (buffer.ep_arrive->is_inbound) {
		// inbound -> outbound
		// update inbound peer
		buffer.ep_arrive->set_peer(buffer.peer);
		// do forward
		size_t outbound_idx = (status.outbound_poll_idx++) % status.args->outbounds.size();
		send_ep = &status.args->outbounds[outbound_idx];
		dst = send_ep;
	}
	else {
		// outbound -> inbound
		size_t inbound_idx = (status.inbound_poll_idx++) % status.args->inbounds.size();
		send_ep = &status.args->inbounds[inbound_idx];
		dst = send_ep->peer;
	}
	// send
	ssize_t send_len = ::sendto(dst->fd, buffer.buffer, buffer.buffer_size, 0,
		(const sockaddr*)&dst->addr, dst->sockaddr_length());
	if (send_len < 0) {
		// error
		buffer.peer.update_addr_str();
		Logger::getLogger().error("dst addr %s fd %d mark %d sendto error: %s\n", dst->addr_str, dst->fd, dst->mark, strerror(errno));
		return -errno;
	}
	if (Logger::getLogger().testLevel(Logger::LogLevel::Debug)) {
		buffer.peer.update_addr_str();
		dst->update_addr_str();
		Logger::getLogger().debug("%s -> %s -> %s@%d:%d %d/%d bytes\n", buffer.peer.addr_str,
			buffer.ep_arrive->addr_str, dst->addr_str, dst->fd, dst->mark, send_len, buffer.buffer_size);
	}
	return 0;
}

void sigint_handler(int sig) {
	// make program to exit gracefully
	exit(0);
}

int main(int argc, const char** argv) {

	signal(SIGINT, sigint_handler);

	args_t args;
	int ret;
	if ((ret = parse_args(argc, argv, args)) < 0) {
		printf("parse arguments fail: %s\n", strerror(-ret));
		return 1;
	}
	Logger::getLogger().setLevel(args.log_level); // set loglevel
	// validate all endpoints
	if (args.inbounds.size() < 1) {
		printf("at least one inbound endpoint expected\n");
		return 1;
	}
	if (args.outbounds.size() < 1) {
		printf("at least one outbound endpoint expected\n");
		return 1;
	}
	for (auto& ep : args.outbounds) {
		if (!lb_endpoint_validate(ep)) {
			printf("invalid outbound endpoint\n");
			return 1;
		}
		ep.update_addr_str();
	}
	for (auto& ep : args.inbounds) {
		if (!lb_endpoint_validate(ep)) {
			printf("invalid inbound endpoint\n");
			return 1;
		}
		ep.update_addr_str();
	}
	// alloc inbound peers
	std::vector<lb_endpoint_t> inbound_peers(args.inbounds.size());
	for (size_t i = 0; i < args.inbounds.size(); ++i) {
		args.inbounds[i].peer = &inbound_peers[i];
	}
	// listen/connect sockets
	for (auto& ep : args.inbounds) {
		if (int ret = lb_endpoint_listen(ep)) {
			printf("listen fail: %s\n", strerror(-ret));
			return 1;
		}
	}
	for (auto& ep : args.outbounds) {
		if (int ret = lb_endpoint_connect(ep)) {
			printf("connect fail: %s\n", strerror(-ret));
			return 1;
		}
	}
	// forwarding packets
	int epoll_fd = -1;
	
	epoll_fd = epoll_create(args.inbounds.size() + args.outbounds.size());
	if (epoll_fd == -1) {
		printf("epoll_in fd create fail: %s\n", strerror(errno));
		return 1;
	}

	for (auto& ep : args.inbounds) {
		epoll_event event;
		event.events = EPOLLIN | EPOLLET;
		event.data.ptr = (void*)&ep;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ep.fd, &event)) {
			printf("epoll_ctl on epoll_fd fail: %s\n", strerror(errno));
			return 1;
		}
	}

	for (auto& ep : args.outbounds) {
		epoll_event event;
		event.events = EPOLLIN | EPOLLET;
		event.data.ptr = (void*)&ep;
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, ep.fd, &event)) {
			printf("epoll_ctl on epoll_fd fail: %s\n", strerror(errno));
			return 1;
		}
	}

	std::vector<epoll_event> events;
	size_t next_send_endpoint_index = 0;
	events.assign(args.inbounds.size() + args.outbounds.size(), epoll_event());
	
	recv_buffer buffer;
	buffer.buffer_size = 0;

	forward_status_t status;
	status.args = &args;

WAIT_LOOP:
	while ((ret = ::epoll_wait(epoll_fd, events.data(), events.size(), -1)) != -1) {
		for (size_t i = 0; i < ret; ++i) {
			lb_endpoint_t* src_ep = (lb_endpoint_t*)events[i].data.ptr;
			if (events[i].events & EPOLLIN) {
				// data arrives
				buffer.ep_arrive = src_ep;
				socklen_t peer_addr_len = sizeof(buffer.peer.addr);
				ssize_t buffer_len = recvfrom(buffer.ep_arrive->fd, buffer.buffer, BUFFER_SIZE, 0, (sockaddr*)&buffer.peer.addr, &peer_addr_len);
				if (buffer_len < 0) {
					if (errno != EWOULDBLOCK) {
						Logger::getLogger().error("recvfrom error: %s\n", strerror(errno));
					}
					continue;
				}
				buffer.buffer_size = buffer_len;
				// send
				do_forward(buffer, status);
				--i; // continue to recv on same fd until there is no more data available
			}
			if (events[i].events & EPOLLERR) {
				Logger::getLogger().error("epoll EPOLLERR: fd %d\n", src_ep->fd);
			}
		}
	}
	if (ret == -1) {
		if (errno == EINTR)goto WAIT_LOOP; // restart interrupted wait
		printf("epoll_wait on epoll_fd fail: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}
