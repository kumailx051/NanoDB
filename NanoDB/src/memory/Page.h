#pragma once

struct Page {
	static const int PAGE_SIZE = 4096;  // 4KB per page
	int pageId;
	bool isDirty;
	bool isOccupied;
	char data[PAGE_SIZE];

	Page();
	void clear();
	void serialize(const char* filename);
	void deserialize(const char* filename);
};
