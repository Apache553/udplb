
#ifndef _ARGS_H_
#define _ARGS_H_

#include <vector>

#include "common.h"
#include "logger.h"

struct args_t {
	std::vector<lb_endpoint_t> outbounds;
	std::vector<lb_endpoint_t> inbounds;
	bool is_server = false;
	Logger::LogLevel log_level=Logger::LogLevel::Info;
};

int parse_args(int argc, const char** argv, args_t& args);

#endif