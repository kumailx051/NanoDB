#include "Logger.h"
#include <cstdarg>
#include <cstdio>

static std::FILE* gLogFile = 0;

void Logger::setFile(std::FILE* file) {
	gLogFile = file;
}

std::FILE* Logger::getFile() {
	return gLogFile;
}

void Logger::log(const char* message) {
	if (message == 0) {
		return;
	}

	std::printf("%s\n", message);
	if (gLogFile != 0) {
		std::fprintf(gLogFile, "%s\n", message);
		std::fflush(gLogFile);
	}
}

void Logger::logf(const char* format, ...) {
	if (format == 0) {
		return;
	}

	char buffer[1024];
	std::va_list args;
	va_start(args, format);
	std::vsnprintf(buffer, static_cast<size_t>(sizeof(buffer)), format, args);
	va_end(args);

	log(buffer);
}
