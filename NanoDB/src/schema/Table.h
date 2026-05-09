#pragma once

#include "Row.h"

class Table {
	static const int MAX_COLUMNS = 64;

	char tableName[64];
	Row** rows;
	int rowCount;
	int maxRows;
	char** columnNames;
	int columnCount;

public:
	Table(const char* name, int maxRows);
	~Table();
	void addRow(Row* r);
	Row* getRow(int index) const;
	int getRowCount() const;
	void addColumn(const char* name);
	int getColumnIndex(const char* name) const;
	int getColumnCount() const;
	const char* getColumnName(int index) const;
	void print() const;
	void loadFromFile(const char* filepath);
};
