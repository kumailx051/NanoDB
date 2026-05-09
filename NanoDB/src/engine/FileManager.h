#pragma once

#include "memory/Page.h"
#include "schema/Table.h"

class FileManager {
	void buildPagePath(const char* tableName, int pageId, char* outPath, int outSize);
	bool fileExists(const char* filepath);

public:
	void writePage(const char* tableName, int pageId, Page* page);
	void readPage(const char* tableName, int pageId, Page* page);

	void loadTBLFile(const char* filepath, Table* table);
	bool pageExists(const char* tableName, int pageId);

	void saveTable(Table* table, const char* filepath);
	void loadTable(Table* table, const char* filepath);
};
