#include "Page.h"
#include <cstdio>
#include <cstring>

Page::Page() {
	pageId = -1;
	isDirty = false;
	isOccupied = false;
	std::memset(data, 0, PAGE_SIZE);
}

void Page::clear() {
	pageId = -1;
	isDirty = false;
	isOccupied = false;
	std::memset(data, 0, PAGE_SIZE);
}

void Page::serialize(const char* filename) {
	if (filename == 0) {
		return;
	}

	FILE* file = std::fopen(filename, "wb");
	if (file == 0) {
		return;
	}

	unsigned char dirty = isDirty ? 1u : 0u;
	unsigned char occupied = isOccupied ? 1u : 0u;

	std::fwrite(&pageId, sizeof(pageId), 1, file);
	std::fwrite(&dirty, sizeof(dirty), 1, file);
	std::fwrite(&occupied, sizeof(occupied), 1, file);
	std::fwrite(data, 1, PAGE_SIZE, file);

	std::fclose(file);
}

void Page::deserialize(const char* filename) {
	if (filename == 0) {
		return;
	}

	FILE* file = std::fopen(filename, "rb");
	if (file == 0) {
		clear();
		return;
	}

	int id = -1;
	unsigned char dirty = 0u;
	unsigned char occupied = 0u;

	if (std::fread(&id, sizeof(id), 1, file) != 1) {
		std::fclose(file);
		clear();
		return;
	}
	if (std::fread(&dirty, sizeof(dirty), 1, file) != 1) {
		std::fclose(file);
		clear();
		return;
	}
	if (std::fread(&occupied, sizeof(occupied), 1, file) != 1) {
		std::fclose(file);
		clear();
		return;
	}

	size_t bytesRead = std::fread(data, 1, PAGE_SIZE, file);
	if (bytesRead < static_cast<size_t>(PAGE_SIZE)) {
		std::memset(data + bytesRead, 0, PAGE_SIZE - bytesRead);
	}

	pageId = id;
	isDirty = (dirty != 0u);
	isOccupied = (occupied != 0u);

	std::fclose(file);
}
