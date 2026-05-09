#include "FileManager.h"
#include "schema/Row.h"
#include "schema/Field.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

void FileManager::buildPagePath(const char* tableName, int pageId, char* outPath, int outSize) {
	if (outPath == 0 || outSize <= 0) {
		return;
	}

	if (tableName == 0 || tableName[0] == '\0') {
		std::snprintf(outPath, static_cast<size_t>(outSize), "data/page_%d.bin", pageId);
		return;
	}

	std::snprintf(outPath, static_cast<size_t>(outSize), "data/%s_%d.bin", tableName, pageId);
}

bool FileManager::fileExists(const char* filepath) {
	if (filepath == 0) {
		return false;
	}

	FILE* file = std::fopen(filepath, "rb");
	if (file == 0) {
		return false;
	}

	std::fclose(file);
	return true;
}

void FileManager::writePage(const char* tableName, int pageId, Page* page) {
	if (page == 0) {
		return;
	}

	char path[256];
	buildPagePath(tableName, pageId, path, 256);
	page->serialize(path);
}

void FileManager::readPage(const char* tableName, int pageId, Page* page) {
	if (page == 0) {
		return;
	}

	char path[256];
	buildPagePath(tableName, pageId, path, 256);
	page->deserialize(path);
}

void FileManager::loadTBLFile(const char* filepath, Table* table) {
	if (filepath == 0 || table == 0) {
		return;
	}

	FILE* file = std::fopen(filepath, "r");
	if (file == 0) {
		return;
	}

	const int MAX_LINE = 2048;
	char line[MAX_LINE];

	while (std::fgets(line, MAX_LINE, file) != 0) {
		int columnCount = table->getColumnCount();
		int maxFields = columnCount > 0 ? columnCount : 20;
		Row* row = new Row(maxFields);

		char* token = line;
		char* next = 0;
		while ((next = std::strchr(token, '|')) != 0) {
			*next = '\0';

			if (token[0] != '\0') {
				bool numeric = true;
				bool hasDot = false;
				int start = 0;
				if (token[0] == '-' || token[0] == '+') {
					start = 1;
				}

				for (int i = start; token[i] != '\0'; ++i) {
					if (token[i] == '.') {
						hasDot = true;
					} else if (token[i] < '0' || token[i] > '9') {
						numeric = false;
						break;
					}
				}

				if (numeric) {
					if (hasDot) {
						row->addField(new FloatField(static_cast<float>(std::strtod(token, 0))));
					} else {
						row->addField(new IntField(std::atoi(token)));
					}
				} else {
					row->addField(new StringField(token));
				}
			}

			token = next + 1;
		}

		table->addRow(row);
	}

	std::fclose(file);
}

bool FileManager::pageExists(const char* tableName, int pageId) {
	char path[256];
	buildPagePath(tableName, pageId, path, 256);
	return fileExists(path);
}

void FileManager::saveTable(Table* table, const char* filepath) {
	if (table == 0 || filepath == 0) {
		return;
	}

	FILE* file = std::fopen(filepath, "wb");
	if (file == 0) {
		return;
	}

	int rowCount = table->getRowCount();
	int columnCount = 0;
	if (rowCount > 0) {
		Row* row = table->getRow(0);
		if (row != 0) {
			columnCount = row->getFieldCount();
		}
	}

	std::fwrite(&rowCount, sizeof(rowCount), 1, file);
	std::fwrite(&columnCount, sizeof(columnCount), 1, file);

	for (int i = 0; i < rowCount; ++i) {
		Row* row = table->getRow(i);
		if (row == 0) {
			continue;
		}

		for (int j = 0; j < columnCount; ++j) {
			Field* field = row->getField(j);
			unsigned char typeCode = 2u;
			if (field != 0 && compareStrings(field->getType(), "INT") == 0) {
				typeCode = 0u;
			} else if (field != 0 && compareStrings(field->getType(), "FLOAT") == 0) {
				typeCode = 1u;
			}

			std::fwrite(&typeCode, sizeof(typeCode), 1, file);
			if (typeCode == 0u) {
				int value = field != 0 ? static_cast<int>(field->toDouble()) : 0;
				std::fwrite(&value, sizeof(value), 1, file);
			} else if (typeCode == 1u) {
				float value = field != 0 ? static_cast<float>(field->toDouble()) : 0.0f;
				std::fwrite(&value, sizeof(value), 1, file);
			} else {
				const char* text = field != 0 ? field->toString() : "";
				int length = 0;
				while (text[length] != '\0') {
					length += 1;
				}
				std::fwrite(&length, sizeof(length), 1, file);
				if (length > 0) {
					std::fwrite(text, 1, static_cast<size_t>(length), file);
				}
			}
		}
	}

	std::fclose(file);
}

void FileManager::loadTable(Table* table, const char* filepath) {
	if (table == 0 || filepath == 0) {
		return;
	}

	FILE* file = std::fopen(filepath, "rb");
	if (file == 0) {
		return;
	}

	int rowCount = 0;
	int columnCount = 0;
	if (std::fread(&rowCount, sizeof(rowCount), 1, file) != 1) {
		std::fclose(file);
		return;
	}
	if (std::fread(&columnCount, sizeof(columnCount), 1, file) != 1) {
		std::fclose(file);
		return;
	}

	for (int i = 0; i < rowCount; ++i) {
		Row* row = new Row(columnCount);
		for (int j = 0; j < columnCount; ++j) {
			unsigned char typeCode = 2u;
			if (std::fread(&typeCode, sizeof(typeCode), 1, file) != 1) {
				delete row;
				std::fclose(file);
				return;
			}

			if (typeCode == 0u) {
				int value = 0;
				std::fread(&value, sizeof(value), 1, file);
				row->addField(new IntField(value));
			} else if (typeCode == 1u) {
				float value = 0.0f;
				std::fread(&value, sizeof(value), 1, file);
				row->addField(new FloatField(value));
			} else {
				int length = 0;
				std::fread(&length, sizeof(length), 1, file);
				if (length < 0) {
					length = 0;
				}
				char* buffer = new char[length + 1];
				if (length > 0) {
					std::fread(buffer, 1, static_cast<size_t>(length), file);
				}
				buffer[length] = '\0';
				row->addField(new StringField(buffer));
				delete[] buffer;
			}
		}

		table->addRow(row);
	}

	std::fclose(file);
}
