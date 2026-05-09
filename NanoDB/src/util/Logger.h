#pragma once

#include <cstdio>

namespace Logger {
	void setFile(std::FILE* file);
	std::FILE* getFile();
	void log(const char* message);
	void logf(const char* format, ...);
}
