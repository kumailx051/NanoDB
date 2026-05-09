#include "SystemCatalog.h"
#include <cstdio>

static int stringLength(const char* text) {
	if (text == 0) {
		return 0;
	}

	int len = 0;
	while (text[len] != '\0') {
		len += 1;
	}
	return len;
}

static int compareStrings(const char* left, const char* right) {
	if (left == 0 && right == 0) {
		return 0;
	}
	if (left == 0) {
		return -1;
	}
	if (right == 0) {
		return 1;
	}

	int i = 0;
	while (left[i] != '\0' && right[i] != '\0') {
		if (left[i] != right[i]) {
			return (left[i] < right[i]) ? -1 : 1;
		}
		i += 1;
	}

	if (left[i] == right[i]) {
		return 0;
	}
	return (left[i] == '\0') ? -1 : 1;
}

SystemCatalog::SystemCatalog() {
	tableCount = 0;
	for (int i = 0; i < HASH_SIZE; ++i) {
		buckets[i] = 0;
	}

	const char customerCols[8][64] = {
		"c_custkey", "c_name", "c_address", "c_nationkey",
		"c_phone", "c_acctbal", "c_mktsegment", "c_comment"
	};
	const char customerTypes[8][16] = {
		"INT", "VARCHAR", "VARCHAR", "INT",
		"VARCHAR", "FLOAT", "VARCHAR", "VARCHAR"
	};
	registerTable("customer", "data/customer.bin", 20000, 8, customerCols, customerTypes);

	const char ordersCols[9][64] = {
		"o_orderkey", "o_custkey", "o_orderstatus", "o_totalprice",
		"o_orderdate", "o_orderpriority", "o_clerk", "o_shippriority",
		"o_comment"
	};
	const char ordersTypes[9][16] = {
		"INT", "INT", "VARCHAR", "FLOAT",
		"VARCHAR", "VARCHAR", "VARCHAR", "INT",
		"VARCHAR"
	};
	registerTable("orders", "data/orders.bin", 30000, 9, ordersCols, ordersTypes);

	const char lineitemCols[16][64] = {
		"l_orderkey", "l_partkey", "l_suppkey", "l_linenumber",
		"l_quantity", "l_extendedprice", "l_discount", "l_tax",
		"l_returnflag", "l_linestatus", "l_shipdate", "l_commitdate",
		"l_receiptdate", "l_shipinstruct", "l_shipmode", "l_comment"
	};
	const char lineitemTypes[16][16] = {
		"INT", "INT", "INT", "INT",
		"FLOAT", "FLOAT", "FLOAT", "FLOAT",
		"VARCHAR", "VARCHAR", "VARCHAR", "VARCHAR",
		"VARCHAR", "VARCHAR", "VARCHAR", "VARCHAR"
	};
	registerTable("lineitem", "data/lineitem.bin", 50000, 16, lineitemCols, lineitemTypes);
}

SystemCatalog::~SystemCatalog() {
	clearBuckets();
}

void SystemCatalog::clearBuckets() {
	for (int i = 0; i < HASH_SIZE; ++i) {
		TableMeta* current = buckets[i];
		while (current != 0) {
			TableMeta* nextNode = current->next;
			delete current;
			current = nextNode;
		}
		buckets[i] = 0;
	}
	tableCount = 0;
}

void SystemCatalog::copyString(char* dest, int destSize, const char* src) const {
	if (dest == 0 || destSize <= 0) {
		return;
	}

	if (src == 0) {
		dest[0] = '\0';
		return;
	}

	int i = 0;
	for (; i < destSize - 1 && src[i] != '\0'; ++i) {
		dest[i] = src[i];
	}
	dest[i] = '\0';
}

int SystemCatalog::hashFunction(const char* key) const {
	if (key == 0) {
		return 0;
	}

	unsigned long hash = 5381;
	for (int i = 0; key[i] != '\0'; ++i) {
		unsigned char c = static_cast<unsigned char>(key[i]);
		hash = (hash * 33u) + static_cast<unsigned long>(c);
	}

	return static_cast<int>(hash % HASH_SIZE);
}

void SystemCatalog::registerTable(const char* name, const char* filePath,
								  int rowCountValue, int colCount,
								  const char colNames[][64],
								  const char colTypes[][16]) {
	if (name == 0 || filePath == 0 || colNames == 0 || colTypes == 0) {
		return;
	}

	int index = hashFunction(name);
	TableMeta* existing = buckets[index];
	while (existing != 0) {
		if (compareStrings(existing->tableName, name) == 0) {
			return;
		}
		existing = existing->next;
	}

	TableMeta* meta = new TableMeta();
	copyString(meta->tableName, static_cast<int>(sizeof(meta->tableName)), name);
	copyString(meta->filePath, static_cast<int>(sizeof(meta->filePath)), filePath);
	meta->rowCount = rowCountValue;
	meta->columnCount = colCount;
	meta->next = buckets[index];

	for (int i = 0; i < 10; ++i) {
		meta->columnNames[i][0] = '\0';
		meta->columnTypes[i][0] = '\0';
	}

	int limit = colCount;
	if (limit > 10) {
		limit = 10;
	}

	for (int i = 0; i < limit; ++i) {
		copyString(meta->columnNames[i], static_cast<int>(sizeof(meta->columnNames[i])), colNames[i]);
		copyString(meta->columnTypes[i], static_cast<int>(sizeof(meta->columnTypes[i])), colTypes[i]);
	}

	buckets[index] = meta;
	tableCount += 1;
}

TableMeta* SystemCatalog::lookup(const char* tableName) {
	if (tableName == 0) {
		return 0;
	}

	int index = hashFunction(tableName);
	TableMeta* current = buckets[index];
	while (current != 0) {
		if (compareStrings(current->tableName, tableName) == 0) {
			return current;
		}
		current = current->next;
	}
	return 0;
}

bool SystemCatalog::tableExists(const char* name) {
	return lookup(name) != 0;
}

void SystemCatalog::printAll() {
	std::printf("System Catalog: %d tables\n", tableCount);
	for (int i = 0; i < HASH_SIZE; ++i) {
		TableMeta* current = buckets[i];
		while (current != 0) {
			std::printf("- %s (%s) rows=%d cols=%d\n",
						current->tableName,
						current->filePath,
						current->rowCount,
						current->columnCount);
			for (int c = 0; c < current->columnCount && c < 10; ++c) {
				std::printf("  %s : %s\n", current->columnNames[c], current->columnTypes[c]);
			}
			current = current->next;
		}
	}
}

void SystemCatalog::saveToDisk(const char* filepath) {
	if (filepath == 0) {
		return;
	}

	FILE* file = std::fopen(filepath, "wb");
	if (file == 0) {
		return;
	}

	std::fwrite(&tableCount, sizeof(tableCount), 1, file);

	for (int i = 0; i < HASH_SIZE; ++i) {
		TableMeta* current = buckets[i];
		while (current != 0) {
			std::fwrite(current, sizeof(TableMeta), 1, file);
			current = current->next;
		}
	}

	std::fclose(file);
}

void SystemCatalog::loadFromDisk(const char* filepath) {
	if (filepath == 0) {
		return;
	}

	FILE* file = std::fopen(filepath, "rb");
	if (file == 0) {
		return;
	}

	clearBuckets();

	int storedCount = 0;
	if (std::fread(&storedCount, sizeof(storedCount), 1, file) != 1) {
		std::fclose(file);
		return;
	}

	for (int i = 0; i < storedCount; ++i) {
		TableMeta temp;
		if (std::fread(&temp, sizeof(TableMeta), 1, file) != 1) {
			break;
		}

		temp.next = 0;
		registerTable(temp.tableName, temp.filePath,
					  temp.rowCount, temp.columnCount,
					  temp.columnNames, temp.columnTypes);
	}

	std::fclose(file);
}
