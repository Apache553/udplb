
#include "args.h"

#include <cstdlib>
#include <cerrno>
#include <limits>

/*
* Usage: udplb <OPTIONS> <INBOUNDS> <OUTBOUNDS>
* OPTIONS ::= [MODE] [LOGLEVEL]
* INBOUNDS ::= <INBOUND> | <INBOUNDS> <INBOUND>
* INBOUND ::= "listen" <ENDPOINT>
* OUTBOUNDS ::= <OUTBOUND> | <OUTBOUNDS> <OUTBOUND>
* OUTBOUND ::= "target" <ENDPOINT> [MARK]
* MARK ::= "mark" MARK
* MODE ::= "mode" ("server" | "client")
* LOGLEVEL ::= "loglevel" ("info" | "warn" | "error" | "debug")
* ENDPOINT ::= ADDRESS port PORT
*/

static int parse_loglevel(const char** beg, const char** end, args_t& args) {
	if (beg == end)return -EINVAL;
	int consumed_token = 0;
	if (strcmp(*beg, "loglevel") != 0)return -EINVAL;
	consumed_token += 1;
	++beg;
	if (beg == end)return -EINVAL;
	if (strcmp(*beg, "info") == 0) {
		args.log_level=Logger::LogLevel::Info;
	}
	else if (strcmp(*beg, "warn") == 0) {
		args.log_level = Logger::LogLevel::Warning;
	}
	else if (strcmp(*beg, "error") == 0) {
		args.log_level = Logger::LogLevel::Error;
	}
	else if (strcmp(*beg, "debug") == 0) {
		args.log_level = Logger::LogLevel::Debug;
	}
	else {
		return -EINVAL;
	}
	consumed_token += 1;
	return consumed_token;
}

static int parse_mode(const char** beg, const char** end, args_t& args) {
	if (beg == end)return -EINVAL;
	int consumed_token = 0;
	if (strcmp(*beg, "mode") != 0)return -EINVAL;
	consumed_token += 1;
	++beg;
	if (beg == end)return -EINVAL;
	if (strcmp(*beg, "server") == 0) {
		args.is_server = true;
	}
	else if (strcmp(*beg, "client") == 0) {
		args.is_server = false;
	}
	else {
		return -EINVAL;
	}
	consumed_token += 1;
	return consumed_token;
}

static int parse_port(const char** beg, const char** end, lb_endpoint_t& ep) {
	if (beg == end)return -EINVAL;
	char* end_pos = nullptr;
	unsigned long long port = ::strtoull(*beg, &end_pos, 0);
	if (*beg == end_pos)return -EINVAL; // no consersion performed
	if (port == 0 || port > 65535)return -ERANGE;
	if (ep.family == AF_INET) {
		ep.addr.in4.sin_port = htons(port);
	}
	else if (ep.family == AF_INET6) {
		ep.addr.in6.sin6_port = htons(port);
	}
	else {
		return -EINVAL;
	}
	return 1; // consumes 1 token
}

static int parse_address(const char** beg, const char** end, lb_endpoint_t& ep) {
	if (beg == end)return -EINVAL;
	if (::inet_pton(AF_INET, *beg, &ep.addr.in4.sin_addr) > 0) {
		// success
		ep.family = ep.addr.in4.sin_family = AF_INET;
	}
	else if (::inet_pton(AF_INET6, *beg, &ep.addr.in6.sin6_addr) > 0) {
		// success
		ep.family = ep.addr.in6.sin6_family = AF_INET6;
	}
	else {
		return -EINVAL;
	}
	return 1; // consumes 1 token
}

static int parse_endpoint(const char** beg, const char** end, lb_endpoint_t& ep) {
	if (beg == end)return -EINVAL;
	int consumed_token = 0;
	int ret = 0;
	if ((ret = parse_address(beg, end, ep)) < 0)return ret; // error parsing address
	consumed_token += ret;
	++beg;
	if (beg == end)return -EINVAL; // no more tokens available
	if (strcmp(*beg, "port") != 0)return -EINVAL; // unexpected token
	consumed_token += 1;
	++beg;
	if ((ret = parse_port(beg, end, ep)) < 0)return ret; // error parsing port
	consumed_token += ret;
	return consumed_token;
}

static int parse_mark(const char** beg, const char** end, lb_endpoint_t& ep) {
	if (beg == end)return -EINVAL;
	int consumed_token = 0;
	if (strcmp(*beg, "mark") != 0)return -EINVAL;
	consumed_token += 1;
	++beg;
	if (beg == end)return -EINVAL;
	char* end_pos = nullptr;
	unsigned long long mark = ::strtoull(*beg, &end_pos, 0);
	if (*beg == end_pos)return -EINVAL; // no conversion performed
	if (mark > std::numeric_limits<uint32_t>::max())return -ERANGE; // exceeds max value
	ep.mark = mark;
	consumed_token += 1;
	return consumed_token;
}

static int parse_inbound(const char** beg, const char** end, lb_endpoint_t& ep) {
	if (beg == end)return -EINVAL;
	int consumed_token = 0;
	int ret = 0;
	if (strcmp(*beg, "listen") != 0)return -EINVAL;
	consumed_token += 1;
	++beg;
	if (beg == end)return -EINVAL;
	if ((ret = parse_endpoint(beg, end, ep)) < 0)return ret;
	consumed_token += ret;
	ep.is_inbound = true; // tag inbound
	return consumed_token;
}

static int parse_outbound(const char** beg, const char** end, lb_endpoint_t& ep) {
	if (beg == end)return -EINVAL;
	int consumed_token = 0;
	int ret = 0;
	if (strcmp(*beg, "target") != 0)return -EINVAL;
	consumed_token += 1;
	++beg;
	if ((ret = parse_endpoint(beg, end, ep)) < 0)return ret;
	consumed_token += ret;
	beg += ret;
	if ((ret = parse_mark(beg, end, ep)) > 0) {
		// parse mark success
		consumed_token += ret;
	}
	return consumed_token;
}

static int parse_inbounds(const char** beg, const char** end, args_t& args) {
	if (beg == end)return -EINVAL;
	int consumed_token = 0;
	int ret = 0;
	while (ret >= 0) {
		lb_endpoint_t ep;
		ret = parse_inbound(beg, end, ep);
		if (ret > 0) {
			consumed_token += ret;
			beg += ret;
			args.inbounds.push_back(ep);
		}
		else {
			break;
		}
	}
	if (consumed_token <= 0)return -EINVAL;
	else return consumed_token;
}

static int parse_outbounds(const char** beg, const char** end, args_t& args) {
	if (beg == end)return -EINVAL;
	int consumed_token = 0;
	int ret = 0;
	while (ret >= 0) {
		lb_endpoint_t ep;
		ret = parse_outbound(beg, end, ep);
		if (ret > 0) {
			consumed_token += ret;
			beg += ret;
			args.outbounds.push_back(ep);
		}
		else {
			break;
		}
	}
	if (consumed_token <= 0)return -EINVAL;
	else return consumed_token;
}

int parse_args(int argc,const char** argv, args_t& args) {
	const char** beg = argv;
	const char** end = argv + argc;
	beg += 1; // skip executable filename

	int consumed_tokens = 0;
	int ret = 0;
	ret = parse_mode(beg, end, args);
	if (ret > 0) {
		consumed_tokens += ret;
		beg += ret;
	}
	ret = parse_loglevel(beg, end, args);
	if (ret > 0) {
		consumed_tokens += ret;
		beg += ret;
	}
	ret = parse_inbounds(beg, end, args);
	if (ret < 0)return ret;
	consumed_tokens += ret;
	beg += ret;
	ret = parse_outbounds(beg, end, args);
	if (ret < 0)return ret;
	consumed_tokens += ret;
	beg += ret;
	if (beg != end)return -EINVAL; // unexpected tokens remaining
	return consumed_tokens;
}