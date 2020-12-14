
#include "logger.h"

#include <cstdio>
#include <cstdarg>

Logger::Logger() :m_level(LogLevel::Info){
}

Logger& Logger::getLogger()
{
	static Logger instance;
	return instance;
}

void Logger::setLevel(LogLevel level)
{
	m_level = level;
}

bool Logger::testLevel(LogLevel level)
{
	return m_level >= level;
}

int Logger::info(const char* fmt, ...)
{
	if (testLevel(LogLevel::Info)) {
		int ret = 0;
		va_list va;
		va_start(va, fmt);
		ret = vprintf(fmt, va);
		va_end(va);
		return ret;
	}
	return 0;
}

int Logger::warn(const char* fmt, ...)
{
	if (testLevel(LogLevel::Warning)) {
		int ret = 0;
		va_list va;
		va_start(va, fmt);
		ret = vprintf(fmt, va);
		va_end(va);
		return ret;
	}
	return 0;
}

int Logger::error(const char* fmt, ...)
{
	if (testLevel(LogLevel::Error)) {
		int ret = 0;
		va_list va;
		va_start(va, fmt);
		ret = vprintf(fmt, va);
		va_end(va);
		return ret;
	}
	return 0;
}

int Logger::debug(const char* fmt, ...)
{
	if (testLevel(LogLevel::Debug)) {
		int ret = 0;
		va_list va;
		va_start(va, fmt);
		ret = vprintf(fmt, va);
		va_end(va);
		return ret;
	}
	return 0;
}
