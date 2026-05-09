#include "Table.h"
#include <cstdio>
#include <cstdlib>

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

static void stringCopy(char* dest, int destSize, const char* src) {
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

static bool isNumericString(const char* text) {
	if (text == 0 || text[0] == '\0') {
		return false;
	}

	char* endPtr = 0;
	std::strtod(text, &endPtr);
	return (endPtr != text && endPtr != 0 && *endPtr == '\0');
}

static bool isFloatString(const char* text) {
	if (text == 0) {
		return false;
	}

	for (int i = 0; text[i] != '\0'; ++i) {
		if (text[i] == '.' || text[i] == 'e' || text[i] == 'E') {
			return true;
		}
	}
	return false;
}

Table::Table(const char* name, int maxRowsValue) {
	stringCopy(tableName, static_cast<int>(sizeof(tableName)), name);
	rowCount = 0;
	maxRows = maxRowsValue;
	rows = new Row*[maxRows];

	for (int i = 0; i < maxRows; ++i) {
		rows[i] = 0;
	}

	columnCount = 0;
	columnNames = new char*[MAX_COLUMNS];
	for (int i = 0; i < MAX_COLUMNS; ++i) {
		columnNames[i] = 0;
	}
}

Table::~Table() {
	for (int i = 0; i < maxRows; ++i) {
		delete rows[i];
		rows[i] = 0;
	}
	delete[] rows;
	rows = 0;

	for (int i = 0; i < columnCount; ++i) {
		delete[] columnNames[i];
		columnNames[i] = 0;
	}
	delete[] columnNames;
	columnNames = 0;

	rowCount = 0;
	maxRows = 0;
	columnCount = 0;
}

void Table::addRow(Row* r) {
	if (r == 0) {
		return;
	}

	if (rowCount >= maxRows) {
		delete r;
		return;
	}

	rows[rowCount] = r;
	rowCount += 1;
}

Row* Table::getRow(int index) const {
	if (index < 0 || index >= rowCount) {
		return 0;
	}
	return rows[index];
}

int Table::getRowCount() const {
	return rowCount;
}

void Table::addColumn(const char* name) {
	if (name == 0) {
		return;
	}

	if (columnCount >= MAX_COLUMNS) {
		return;
	}

	int len = stringLength(name);
	char* stored = new char[len + 1];
	stringCopy(stored, len + 1, name);
	columnNames[columnCount] = stored;
	columnCount += 1;
}

int Table::getColumnIndex(const char* name) const {
	if (name == 0) {
		return -1;
	}
	for (int i = 0; i < columnCount; ++i) {
		if (compareStrings(columnNames[i], name) == 0) {
			return i;
		}
	}
	return -1;
}

int Table::getColumnCount() const {
	return columnCount;
}

const char* Table::getColumnName(int index) const {
	if (index < 0 || index >= columnCount) {
		return 0;
	}
	return columnNames[index];
}

void Table::print() const {
	std::printf("Table: %s\n", tableName);

	for (int i = 0; i < columnCount; ++i) {
		std::printf("%s", columnNames[i] != 0 ? columnNames[i] : "");
		if (i < columnCount - 1) {
			std::printf(" | ");
		}
	}
	if (columnCount > 0) {
		std::printf("\n");
	}

	for (int i = 0; i < rowCount; ++i) {
		if (rows[i] != 0) {
			rows[i]->print();
		}
	}
}

void Table::loadFromFile(const char* filepath) {
	if (filepath == 0) {
		return;
	}

	FILE* file = std::fopen(filepath, "r");
	if (file == 0) {
		return;
	}

	const int MAX_LINE = 8192;
	const int MAX_TOKEN = 1024;
	char line[MAX_LINE];

	while (std::fgets(line, MAX_LINE, file) != 0) {
		if (rowCount >= maxRows) {
			break;
		}

		Row* row = new Row(columnCount > 0 ? columnCount : MAX_COLUMNS);
		char token[MAX_TOKEN];
		int tokenLen = 0;

		for (int i = 0; line[i] != '\0'; ++i) {
			char c = line[i];
			if (c == '|') {
				token[tokenLen] = '\0';
				Field* field = 0;
				if (isNumericString(token)) {
					if (isFloatString(token)) {
						field = new FloatField(static_cast<float>(std::strtod(token, 0)));
					} else {
						field = new IntField(std::atoi(token));
					}
				} else {
					field = new StringField(token);
				}
				row->addField(field);
				tokenLen = 0;
			} else if (c == '\n' || c == '\r') {
				if (tokenLen > 0) {
					token[tokenLen] = '\0';
					Field* field = 0;
					if (isNumericString(token)) {
						if (isFloatString(token)) {
							field = new FloatField(static_cast<float>(std::strtod(token, 0)));
						} else {
							field = new IntField(std::atoi(token));
						}
					} else {
						field = new StringField(token);
					}
					row->addField(field);
					tokenLen = 0;
				}
				break;
			} else {
				if (tokenLen < MAX_TOKEN - 1) {
					token[tokenLen] = c;
					tokenLen += 1;
				}
			}
		}

		if (tokenLen > 0) {
			token[tokenLen] = '\0';
			Field* field = 0;
			if (isNumericString(token)) {
				if (isFloatString(token)) {
					field = new FloatField(static_cast<float>(std::strtod(token, 0)));
				} else {
					field = new IntField(std::atoi(token));
				}
			} else {
				field = new StringField(token);
			}
			row->addField(field);
		}

		addRow(row);
	}

	std::fclose(file);
}
