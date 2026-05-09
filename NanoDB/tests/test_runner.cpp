#include "engine/FileManager.h"
#include "memory/BufferPool.h"
#include "catalog/SystemCatalog.h"
#include "engine/QueryExecutor.h"
#include "schema/Table.h"
#include <cstdio>

static bool fileExists(const char* path) {
	if (path == 0) {
		return false;
	}
	FILE* file = std::fopen(path, "rb");
	if (file == 0) {
		return false;
	}
	std::fclose(file);
	return true;
}

static void buildTableFromMeta(Table* table, const TableMeta* meta) {
	if (table == 0 || meta == 0) {
		return;
	}

	for (int i = 0; i < meta->columnCount && i < 10; ++i) {
		table->addColumn(meta->columnNames[i]);
	}
}

static void loadTableData(FileManager* fm, SystemCatalog* cat, const char* tableName, const char* tblPath) {
	if (fm == 0 || cat == 0 || tableName == 0 || tblPath == 0) {
		return;
	}

	TableMeta* meta = cat->lookup(tableName);
	if (meta == 0) {
		return;
	}

	if (fileExists(meta->filePath)) {
		return;
	}

	int capacity = meta->rowCount > 0 ? meta->rowCount + 100 : 100;
	Table* table = new Table(meta->tableName, capacity);
	buildTableFromMeta(table, meta);
	fm->loadTBLFile(tblPath, table);
	fm->saveTable(table, meta->filePath);
	delete table;
}

int main() {
	FileManager* fm = new FileManager();
	BufferPool* bp = new BufferPool(fm);
	SystemCatalog* cat = new SystemCatalog();
	QueryExecutor* executor = new QueryExecutor(bp, cat);

	loadTableData(fm, cat, "customer", "data/customer.tbl");
	loadTableData(fm, cat, "orders", "data/orders.tbl");
	loadTableData(fm, cat, "lineitem", "data/lineitem.tbl");

	executor->initLog("logs/nanodb_execution.log");
	executor->runWorkloadFile("tests/queries.txt");

	std::printf("=== NanoDB Test Runner Complete ===\n");
	std::printf("Queries executed: 50\n");
	std::printf("Check logs/nanodb_execution.log for details\n");

	delete executor;
	delete cat;
	delete bp;
	delete fm;
	return 0;
}
