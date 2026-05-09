#pragma once

struct TableMeta {
	char tableName[64];
	char filePath[256];
	int rowCount;
	int columnCount;
	char columnNames[10][64];
	char columnTypes[10][16];
	TableMeta* next;
};

class SystemCatalog {
	static const int HASH_SIZE = 64;
	TableMeta* buckets[HASH_SIZE];
	int tableCount;

	int hashFunction(const char* key) const;
	void clearBuckets();
	void copyString(char* dest, int destSize, const char* src) const;

public:
	SystemCatalog();
	~SystemCatalog();

	void registerTable(const char* name, const char* filePath,
					   int rowCount, int colCount,
					   const char colNames[][64],
					   const char colTypes[][16]);

	TableMeta* lookup(const char* tableName);
	bool tableExists(const char* name);
	void printAll();

	void saveToDisk(const char* filepath);
	void loadFromDisk(const char* filepath);
};
