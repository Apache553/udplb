
#ifndef _LOGGER_H_
#define _LOGGER_H_

class Logger {
public:
	static Logger& getLogger();
public:
	enum class LogLevel {
		Info = 1,
		Warning = 2,
		Error = 3,
		Debug = 4
	};
	void setLevel(LogLevel level);
	bool testLevel(LogLevel level);

	int info(const char* fmt, ...);
	int warn(const char* fmt, ...);
	int error(const char* fmt, ...);
	int debug(const char* fmt, ...);
private:
	Logger();
	
	LogLevel m_level;
};

#endif