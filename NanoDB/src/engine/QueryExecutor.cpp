#include "QueryExecutor.h"
#include "parser/Tokenizer.h"
#include "parser/PostfixConverter.h"
#include "parser/ExpressionEvaluator.h"
#include "util/Logger.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>

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

static void copyString(char* dest, int destSize, const char* src) {
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

static bool isWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static char toUpperChar(char c) {
	if (c >= 'a' && c <= 'z') {
		return static_cast<char>(c - ('a' - 'A'));
	}
	return c;
}

static bool fileExists(const char* path) {
	if (path == 0 || path[0] == '\0') {
		return false;
	}
	FILE* file = std::fopen(path, "rb");
	if (file == 0) {
		return false;
	}
	std::fclose(file);
	return true;
}

static void joinPath(const char* dir, const char* file, char* outPath, int outSize) {
	if (outPath == 0 || outSize <= 0) {
		return;
	}
	if (dir == 0 || dir[0] == '\0') {
		std::snprintf(outPath, static_cast<size_t>(outSize), "%s", file != 0 ? file : "");
		return;
	}

	int len = stringLength(dir);
	bool needsSep = true;
	if (len > 0) {
		char last = dir[len - 1];
		needsSep = !(last == '/' || last == '\\');
	}

	std::snprintf(outPath, static_cast<size_t>(outSize), "%s%s%s", dir, needsSep ? "/" : "", file != 0 ? file : "");
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

static bool getJoinColumnNames(const char* leftName, const char* rightName, const char*& leftColumn, const char*& rightColumn) {
	leftColumn = 0;
	rightColumn = 0;

	if (leftName == 0 || rightName == 0) {
		return false;
	}

	if (compareStrings(leftName, "customer") == 0 && compareStrings(rightName, "orders") == 0) {
		leftColumn = "c_custkey";
		rightColumn = "o_custkey";
		return true;
	}
	if (compareStrings(leftName, "orders") == 0 && compareStrings(rightName, "customer") == 0) {
		leftColumn = "o_custkey";
		rightColumn = "c_custkey";
		return true;
	}
	if (compareStrings(leftName, "orders") == 0 && compareStrings(rightName, "lineitem") == 0) {
		leftColumn = "o_orderkey";
		rightColumn = "l_orderkey";
		return true;
	}
	if (compareStrings(leftName, "lineitem") == 0 && compareStrings(rightName, "orders") == 0) {
		leftColumn = "l_orderkey";
		rightColumn = "o_orderkey";
		return true;
	}

	return false;
}

static void reorderJoinTables(const char* tables[], Table* loadedTables[], int count,
					  const char* mstOrder[], int mstCount,
					  const char* orderedNames[], Table* orderedTables[], int orderedOriginalIndex[]) {
	for (int i = 0; i < count; ++i) {
		orderedNames[i] = tables[i];
		orderedTables[i] = loadedTables[i];
		orderedOriginalIndex[i] = i;
	}

	if (mstCount != count) {
		return;
	}

	for (int i = 0; i < count; ++i) {
		int sourceIndex = -1;
		for (int j = 0; j < count; ++j) {
			if (mstOrder[i] != 0 && compareStrings(mstOrder[i], tables[j]) == 0) {
				sourceIndex = j;
				break;
			}
		}
		if (sourceIndex < 0) {
			return;
		}
		orderedNames[i] = tables[sourceIndex];
		orderedTables[i] = loadedTables[sourceIndex];
		orderedOriginalIndex[i] = sourceIndex;
	}
}

static bool rowMatchesJoinKey(Row* leftRow, Row* rightRow, int leftKeyIndex, int rightKeyIndex) {
	if (leftRow == 0 || rightRow == 0 || leftKeyIndex < 0 || rightKeyIndex < 0) {
		return false;
	}

	Field* leftField = leftRow->getField(leftKeyIndex);
	Field* rightField = rightRow->getField(rightKeyIndex);
	if (leftField == 0 || rightField == 0) {
		return false;
	}

	return static_cast<int>(leftField->toDouble()) == static_cast<int>(rightField->toDouble());
}

static void placeRow(Row* rows[], int rowCount, int originalIndex, Row* row) {
	if (rows == 0 || originalIndex < 0 || originalIndex >= rowCount) {
		return;
	}
	rows[originalIndex] = row;
}

static void printJoinedRow(Row** rows, int count) {
	if (rows == 0 || count <= 0) {
		return;
	}

	for (int t = 0; t < count; ++t) {
		Row* row = rows[t];
		if (row == 0) {
			continue;
		}
		int fieldCount = row->getFieldCount();
		for (int i = 0; i < fieldCount; ++i) {
			Field* field = row->getField(i);
			if (field != 0) {
				field->print();
			}
			if (i < fieldCount - 1 || t < count - 1) {
				std::printf(" | ");
			}
		}
	}
	std::printf("\n");
}

QueryExecutor::QueryExecutor(BufferPool* bp, SystemCatalog* cat) {
	bufferPool = bp;
	catalog = cat;
	customerIndex = new AVLTree();
	ordersIndex = new AVLTree();
	lineitemIndex = new AVLTree();
	joinGraph = new Graph();
	optimizer = new MSTOptimizer(joinGraph);
	queryQueue = new PriorityQueue(256);
	fileManager = new FileManager();
	logFile = 0;

	customerTable = 0;
	ordersTable = 0;
	lineitemTable = 0;
	customerLoaded = false;
	ordersLoaded = false;
	lineitemLoaded = false;
}

QueryExecutor::~QueryExecutor() {
	delete customerIndex;
	delete ordersIndex;
	delete lineitemIndex;
	delete optimizer;
	delete joinGraph;
	delete queryQueue;
	delete fileManager;

	delete customerTable;
	delete ordersTable;
	delete lineitemTable;

	closeLog();
}

void QueryExecutor::extractFirstWord(const char* query, char* outWord, int outSize) const {
	if (outWord == 0 || outSize <= 0) {
		return;
	}
	outWord[0] = '\0';
	if (query == 0) {
		return;
	}

	int i = 0;
	while (query[i] != '\0' && isWhitespace(query[i])) {
		i += 1;
	}

	int len = 0;
	while (query[i] != '\0' && !isWhitespace(query[i]) && len < outSize - 1) {
		outWord[len] = toUpperChar(query[i]);
		len += 1;
		i += 1;
	}
	outWord[len] = '\0';
}

const char* QueryExecutor::skipFirstWord(const char* text) const {
	if (text == 0) {
		return 0;
	}

	int i = 0;
	while (text[i] != '\0' && isWhitespace(text[i])) {
		i += 1;
	}
	while (text[i] != '\0' && !isWhitespace(text[i])) {
		i += 1;
	}
	while (text[i] != '\0' && isWhitespace(text[i])) {
		i += 1;
	}

	return text + i;
}

void QueryExecutor::trimWhitespace(char* text) const {
	if (text == 0) {
		return;
	}

	int len = stringLength(text);
	int start = 0;
	while (start < len && isWhitespace(text[start])) {
		start += 1;
	}

	int end = len - 1;
	while (end >= start && isWhitespace(text[end])) {
		end -= 1;
	}

	int out = 0;
	for (int i = start; i <= end; ++i) {
		text[out] = text[i];
		out += 1;
	}
	text[out] = '\0';
}

bool QueryExecutor::equalsIgnoreCase(const char* a, const char* b) const {
	if (a == 0 || b == 0) {
		return false;
	}

	int i = 0;
	while (a[i] != '\0' && b[i] != '\0') {
		if (toUpperChar(a[i]) != toUpperChar(b[i])) {
			return false;
		}
		i += 1;
	}

	return a[i] == '\0' && b[i] == '\0';
}

bool QueryExecutor::extractTableFromTokens(Token* tokens, int tokenCount, const char* keyword, char* outName, int outSize) const {
	if (tokens == 0 || keyword == 0 || outName == 0 || outSize <= 0) {
		return false;
	}

	outName[0] = '\0';
	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_END) {
			break;
		}
		if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, keyword)) {
			if (i + 1 < tokenCount && tokens[i + 1].type == TOKEN_IDENTIFIER) {
				copyString(outName, outSize, tokens[i + 1].value);
				return true;
			}
		}
	}

	return false;
}

bool QueryExecutor::extractWhereColumn(Token* tokens, int tokenCount, char* outName, int outSize) const {
	if (tokens == 0 || outName == 0 || outSize <= 0) {
		return false;
	}

	outName[0] = '\0';
	bool inWhere = false;
	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_END) {
			break;
		}
		if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, "WHERE")) {
			inWhere = true;
			continue;
		}
		if (!inWhere) {
			continue;
		}
		if (tokens[i].type == TOKEN_IDENTIFIER) {
			copyString(outName, outSize, tokens[i].value);
			return true;
		}
	}

	return false;
}

bool QueryExecutor::extractFirstNumberAfterWhere(Token* tokens, int tokenCount, int& outValue) const {
	outValue = 0;
	if (tokens == 0) {
		return false;
	}

	bool inWhere = false;
	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_END) {
			break;
		}
		if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, "WHERE")) {
			inWhere = true;
			continue;
		}
		if (!inWhere) {
			continue;
		}
		if (tokens[i].type == TOKEN_NUMBER) {
			outValue = std::atoi(tokens[i].value);
			return true;
		}
	}

	return false;
}

Table* QueryExecutor::getTableByName(const char* name) {
	if (name == 0) {
		return 0;
	}
	if (equalsIgnoreCase(name, "customer")) {
		return customerTable;
	}
	if (equalsIgnoreCase(name, "orders")) {
		return ordersTable;
	}
	if (equalsIgnoreCase(name, "lineitem")) {
		return lineitemTable;
	}
	return 0;
}

bool QueryExecutor::isTableLoaded(const char* name) {
	if (name == 0) {
		return false;
	}
	if (equalsIgnoreCase(name, "customer")) {
		return customerLoaded;
	}
	if (equalsIgnoreCase(name, "orders")) {
		return ordersLoaded;
	}
	if (equalsIgnoreCase(name, "lineitem")) {
		return lineitemLoaded;
	}
	return false;
}

void QueryExecutor::markTableLoaded(const char* name) {
	if (name == 0) {
		return;
	}
	if (equalsIgnoreCase(name, "customer")) {
		customerLoaded = true;
	} else if (equalsIgnoreCase(name, "orders")) {
		ordersLoaded = true;
	} else if (equalsIgnoreCase(name, "lineitem")) {
		lineitemLoaded = true;
	}
}

void QueryExecutor::buildTableFromCatalog(Table* table, const TableMeta* meta) {
	if (table == 0 || meta == 0) {
		return;
	}

	for (int i = 0; i < meta->columnCount && i < 10; ++i) {
		table->addColumn(meta->columnNames[i]);
	}
}

Table* QueryExecutor::ensureTableLoaded(const char* name) {
	if (name == 0 || catalog == 0) {
		return 0;
	}

	TableMeta* meta = catalog->lookup(name);
	if (meta == 0) {
		return 0;
	}

	Table* table = getTableByName(name);
	if (table == 0) {
		int capacity = meta->rowCount > 0 ? meta->rowCount + 100 : 100;
		Table* newTable = new Table(meta->tableName, capacity);
		buildTableFromCatalog(newTable, meta);

		if (equalsIgnoreCase(name, "customer")) {
			customerTable = newTable;
		} else if (equalsIgnoreCase(name, "orders")) {
			ordersTable = newTable;
		} else if (equalsIgnoreCase(name, "lineitem")) {
			lineitemTable = newTable;
		}

		table = newTable;
	}

	if (!isTableLoaded(name)) {
		if (fileManager != 0 && meta->filePath[0] != '\0') {
			fileManager->loadTable(table, meta->filePath);
		}
		if (equalsIgnoreCase(name, "customer") && customerIndex != 0) {
			customerIndex->buildIndex(table, 0);
			Logger::logf("[LOG] Built AVL index for customer");
		} else if (equalsIgnoreCase(name, "orders") && ordersIndex != 0) {
			ordersIndex->buildIndex(table, 0);
			Logger::logf("[LOG] Built AVL index for orders");
		} else if (equalsIgnoreCase(name, "lineitem") && lineitemIndex != 0) {
			lineitemIndex->buildIndex(table, 0);
			Logger::logf("[LOG] Built AVL index for lineitem");
		}
		markTableLoaded(name);
	}

	return table;
}

bool QueryExecutor::extractValueList(const char* query, char* out, int outSize) const {
	if (query == 0 || out == 0 || outSize <= 0) {
		return false;
	}

	const char* start = 0;
	const char* end = 0;
	for (int i = 0; query[i] != '\0'; ++i) {
		if (query[i] == '(') {
			start = query + i + 1;
			break;
		}
	}

	if (start == 0) {
		return false;
	}

	for (int i = 0; start[i] != '\0'; ++i) {
		if (start[i] == ')') {
			end = start + i;
			break;
		}
	}

	if (end == 0) {
		return false;
	}

	int len = static_cast<int>(end - start);
	if (len >= outSize) {
		len = outSize - 1;
	}

	for (int i = 0; i < len; ++i) {
		out[i] = start[i];
	}
	out[len] = '\0';
	return true;
}

Field* QueryExecutor::buildFieldFromToken(const char* token) const {
	if (token == 0) {
		return new StringField("");
	}

	char buffer[256];
	copyString(buffer, static_cast<int>(sizeof(buffer)), token);
	trimWhitespace(buffer);

	int len = stringLength(buffer);
	if (len >= 2 && buffer[0] == '"' && buffer[len - 1] == '"') {
		buffer[len - 1] = '\0';
		return new StringField(buffer + 1);
	}

	bool hasDot = false;
	bool numeric = true;
	int start = 0;
	if (buffer[0] == '-' || buffer[0] == '+') {
		start = 1;
	}

	for (int i = start; buffer[i] != '\0'; ++i) {
		if (buffer[i] == '.') {
			hasDot = true;
		} else if (buffer[i] < '0' || buffer[i] > '9') {
			numeric = false;
			break;
		}
	}

	if (numeric) {
		if (hasDot) {
			return new FloatField(static_cast<float>(std::strtod(buffer, 0)));
		}
		return new IntField(std::atoi(buffer));
	}

	return new StringField(buffer);
}

void QueryExecutor::parseJoinTablesFromTokens(Token* tokens, int tokenCount, char names[][64], int& count) {
	count = 0;
	if (tokens == 0) {
		return;
	}

	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_END) {
			break;
		}

		if (tokens[i].type == TOKEN_KEYWORD && (equalsIgnoreCase(tokens[i].value, "FROM") || equalsIgnoreCase(tokens[i].value, "JOIN"))) {
			if (i + 1 < tokenCount && tokens[i + 1].type == TOKEN_IDENTIFIER) {
				copyString(names[count], 64, tokens[i + 1].value);
				count += 1;
			}
		}
	}
}

void QueryExecutor::submitQuery(const char* query, int priority) {
	if (queryQueue == 0 || query == 0) {
		return;
	}
	char command[32];
	extractFirstWord(query, command, static_cast<int>(sizeof(command)));
	if (equalsIgnoreCase(command, "ADMIN")) {
		const char* remainder = skipFirstWord(query);
		char stripped[512];
		copyString(stripped, static_cast<int>(sizeof(stripped)), remainder != 0 ? remainder : "");
		queryQueue->enqueue(stripped, 0);
		return;
	}

	queryQueue->enqueue(query, priority);
}

void QueryExecutor::processQueue() {
	if (queryQueue == 0) {
		return;
	}

	while (!queryQueue->isEmpty()) {
		QueryItem item = queryQueue->dequeue();
		char command[32];
		extractFirstWord(item.query, command, static_cast<int>(sizeof(command)));

		if (equalsIgnoreCase(command, "ADMIN")) {
			const char* remainder = skipFirstWord(item.query);
			if (remainder != 0 && remainder[0] != '\0') {
				extractFirstWord(remainder, command, static_cast<int>(sizeof(command)));
				if (equalsIgnoreCase(command, "SELECT")) {
					executeSelect(remainder);
				} else if (equalsIgnoreCase(command, "INSERT")) {
					executeInsert(remainder);
				} else if (equalsIgnoreCase(command, "UPDATE")) {
					executeUpdate(remainder);
				}
			}
			continue;
		}

		if (equalsIgnoreCase(command, "SELECT")) {
			executeSelect(item.query);
		} else if (equalsIgnoreCase(command, "SELECT_SEQ")) {
			executeSelectSeq(item.query);
		} else if (equalsIgnoreCase(command, "SELECT_IDX")) {
			executeSelectIdx(item.query);
		} else if (equalsIgnoreCase(command, "STRESS_TEST")) {
			executeStressTest(item.query);
		} else if (equalsIgnoreCase(command, "PERSIST_TEST")) {
			executePersistenceTest();
		} else if (equalsIgnoreCase(command, "INSERT")) {
			executeInsert(item.query);
		} else if (equalsIgnoreCase(command, "UPDATE")) {
			executeUpdate(item.query);
		}
	}
}

void QueryExecutor::runWorkloadFile(const char* filepath) {
	if (filepath == 0) {
		return;
	}

	FILE* file = std::fopen(filepath, "r");
	if (file == 0) {
		return;
	}

	char line[1024];
	while (std::fgets(line, static_cast<int>(sizeof(line)), file) != 0) {
		trimWhitespace(line);
		if (line[0] == '\0') {
			continue;
		}

		char command[32];
		extractFirstWord(line, command, static_cast<int>(sizeof(command)));
		if (equalsIgnoreCase(command, "FLOOD_TEST")) {
			int count = 0;
			if (std::sscanf(line, "FLOOD_TEST %d", &count) == 1 && count > 0) {
				for (int i = 0; i < count; ++i) {
					char query[256];
					int key = (i % 1000) + 1;
					std::snprintf(query, static_cast<size_t>(sizeof(query)),
						"SELECT * FROM customer WHERE c_custkey == %d", key);
					submitQuery(query, 1);
				}
				Logger::logf("[LOG] Flood test enqueued %d SELECT queries", count);
			}
			continue;
		}

		submitQuery(line, 1);
	}

	std::fclose(file);
	processQueue();
}

void QueryExecutor::executeSelect(const char* query) {
	Tokenizer tokenizer(query);
	tokenizer.tokenize();
	Token* tokens = tokenizer.getTokens();
	int tokenCount = tokenizer.getTokenCount();

	char tableNames[5][64];
	int tableCount = 0;
	parseJoinTablesFromTokens(tokens, tokenCount, tableNames, tableCount);

	if (tableCount >= 2) {
		const char* joinList[5];
		for (int i = 0; i < tableCount; ++i) {
			joinList[i] = tableNames[i];
		}
		executeJoin(tokens, tokenCount, joinList, tableCount);
		return;
	}

	if (tableCount <= 0) {
		bool hasWhere = false;
		for (int i = 0; i < tokenCount; ++i) {
			if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, "WHERE")) {
				hasWhere = true;
				break;
			}
		}
		if (hasWhere) {
			copyString(tableNames[0], 64, "customer");
			tableCount = 1;
		} else {
			return;
		}
	}

	Table* table = ensureTableLoaded(tableNames[0]);
	if (table == 0) {
		return;
	}

	int whereIndex = -1;
	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, "WHERE")) {
			whereIndex = i + 1;
			break;
		}
	}

	if (whereIndex < 0) {
		table->print();
		return;
	}

	int exprCount = 0;
	for (int i = whereIndex; i < tokenCount; ++i) {
		exprCount += 1;
		if (tokens[i].type == TOKEN_END) {
			break;
		}
	}

	Token* exprTokens = new Token[exprCount + 1];
	int writeIndex = 0;
	for (int i = whereIndex; i < tokenCount && writeIndex < exprCount; ++i) {
		exprTokens[writeIndex] = tokens[i];
		writeIndex += 1;
		if (tokens[i].type == TOKEN_END) {
			break;
		}
	}
	exprTokens[writeIndex].type = TOKEN_END;
	exprTokens[writeIndex].value[0] = '\0';

	PostfixConverter converter;
	int postfixCount = 0;
	Token* postfix = converter.convert(exprTokens, writeIndex, postfixCount);

	ExpressionEvaluator evaluator;
	int rowCount = table->getRowCount();
	int lastPage = -1;
	for (int i = 0; i < rowCount; ++i) {
		if (bufferPool != 0) {
			int pageId = i / 128;
			if (pageId != lastPage) {
				bufferPool->fetchPage(pageId, tableNames[0]);
				lastPage = pageId;
			}
		}
		Row* row = table->getRow(i);
		if (row == 0) {
			continue;
		}
		if (evaluator.evaluate(postfix, postfixCount, row, table)) {
			row->print();
		}
	}

	delete[] exprTokens;
}

void QueryExecutor::executeSelectSeq(const char* query) {
	if (query == 0) {
		return;
	}

	Tokenizer tokenizer(query);
	tokenizer.tokenize();
	Token* tokens = tokenizer.getTokens();
	int tokenCount = tokenizer.getTokenCount();

	char tableName[64];
	if (!extractTableFromTokens(tokens, tokenCount, "FROM", tableName, static_cast<int>(sizeof(tableName)))) {
		Logger::logf("[ERROR] SELECT_SEQ missing FROM table");
		return;
	}

	char columnName[64];
	if (!extractWhereColumn(tokens, tokenCount, columnName, static_cast<int>(sizeof(columnName)))) {
		Logger::logf("[ERROR] SELECT_SEQ missing WHERE column");
		return;
	}

	int key = 0;
	if (!extractFirstNumberAfterWhere(tokens, tokenCount, key)) {
		Logger::logf("[ERROR] SELECT_SEQ missing WHERE value");
		return;
	}

	Table* table = ensureTableLoaded(tableName);
	if (table == 0) {
		Logger::logf("[ERROR] SELECT_SEQ table not found: %s", tableName);
		return;
	}

	int colIndex = table->getColumnIndex(columnName);
	if (colIndex < 0) {
		Logger::logf("[ERROR] SELECT_SEQ column not found: %s", columnName);
		return;
	}

	using Clock = std::chrono::high_resolution_clock;
	auto start = Clock::now();
	int rowCount = table->getRowCount();
	int comparisons = 0;
	int foundIndex = -1;
	int lastPage = -1;

	for (int i = 0; i < rowCount; ++i) {
		if (bufferPool != 0) {
			int pageId = i / 128;
			if (pageId != lastPage) {
				bufferPool->fetchPage(pageId, tableName);
				lastPage = pageId;
			}
		}

		Row* row = table->getRow(i);
		if (row == 0) {
			continue;
		}
		Field* field = row->getField(colIndex);
		if (field == 0) {
			continue;
		}

		comparisons += 1;
		int value = static_cast<int>(field->toDouble());
		if (value == key) {
			foundIndex = i;
			break;
		}
	}

	auto end = Clock::now();
	double ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
	Logger::logf("[BENCHMARK] SELECT_SEQ %s key=%d time=%.3fms comparisons=%d", tableName, key, ms, comparisons);

	if (foundIndex >= 0) {
		Row* row = table->getRow(foundIndex);
		if (row != 0) {
			row->print();
		}
	} else {
		Logger::logf("[LOG] SELECT_SEQ no match for key %d", key);
	}
}

void QueryExecutor::executeSelectIdx(const char* query) {
	if (query == 0) {
		return;
	}

	Tokenizer tokenizer(query);
	tokenizer.tokenize();
	Token* tokens = tokenizer.getTokens();
	int tokenCount = tokenizer.getTokenCount();

	char tableName[64];
	if (!extractTableFromTokens(tokens, tokenCount, "FROM", tableName, static_cast<int>(sizeof(tableName)))) {
		Logger::logf("[ERROR] SELECT_IDX missing FROM table");
		return;
	}

	int key = 0;
	if (!extractFirstNumberAfterWhere(tokens, tokenCount, key)) {
		Logger::logf("[ERROR] SELECT_IDX missing WHERE value");
		return;
	}

	Table* table = ensureTableLoaded(tableName);
	if (table == 0) {
		Logger::logf("[ERROR] SELECT_IDX table not found: %s", tableName);
		return;
	}

	AVLTree* index = getIndex(tableName);
	if (index == 0) {
		Logger::logf("[ERROR] SELECT_IDX index not available for %s", tableName);
		return;
	}

	using Clock = std::chrono::high_resolution_clock;
	auto start = Clock::now();
	AVLNode* node = index->searchWithLog(key);
	auto end = Clock::now();
	double ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
	Logger::logf("[BENCHMARK] SELECT_IDX %s key=%d time=%.3fms", tableName, key, ms);

	if (node != 0) {
		int rowIndex = node->pageId * 128 + node->rowIndex;
		if (bufferPool != 0) {
			bufferPool->fetchPage(node->pageId, tableName);
		}
		Row* row = table->getRow(rowIndex);
		if (row != 0) {
			row->print();
		}
	} else {
		Logger::logf("[LOG] SELECT_IDX no match for key %d", key);
	}
}

void QueryExecutor::executeStressTest(const char* query) {
	if (query == 0) {
		return;
	}

	char tableName[64];
	int targetRows = 0;
	if (std::sscanf(query, "STRESS_TEST %63s %d", tableName, &targetRows) < 2) {
		Logger::logf("[ERROR] STRESS_TEST syntax: STRESS_TEST <table> <rows>");
		return;
	}

	Table* table = ensureTableLoaded(tableName);
	if (table == 0) {
		Logger::logf("[ERROR] STRESS_TEST table not found: %s", tableName);
		return;
	}

	if (bufferPool == 0) {
		Logger::logf("[ERROR] STRESS_TEST buffer pool not available");
		return;
	}

	int totalRows = table->getRowCount();
	int limit = targetRows < totalRows ? targetRows : totalRows;
	int startFaults = bufferPool->getPageFaultCount();
	int startEvictions = bufferPool->getEvictionCount();
	int lastPage = -1;

	for (int i = 0; i < limit; ++i) {
		int pageId = i / 128;
		if (pageId != lastPage) {
			bufferPool->fetchPage(pageId, tableName);
			lastPage = pageId;
		}
	}

	int faults = bufferPool->getPageFaultCount() - startFaults;
	int evictions = bufferPool->getEvictionCount() - startEvictions;
	Logger::logf("[LOG] Stress test scanned %d rows on %s", limit, tableName);
	Logger::logf("[LOG] Page faults: %d | Evictions: %d", faults, evictions);
}

void QueryExecutor::executePersistenceTest() {
	TableMeta* meta = catalog != 0 ? catalog->lookup("customer") : 0;
	if (meta == 0) {
		Logger::logf("[ERROR] PERSIST_TEST missing customer metadata");
		return;
	}

	int baseKey = meta->rowCount + 100000;
	for (int i = 0; i < 5; ++i) {
		char query[512];
		int key = baseKey + i;
		std::snprintf(query, static_cast<size_t>(sizeof(query)),
			"INSERT INTO customer VALUES (%d, \"Persist User %d\", \"Persist Addr %d\", 1, \"555-900%d\", 1234.00, \"BUILDING\", \"persist\")",
			key, i + 1, i + 1, i + 1);
		executeInsert(query);
	}

	FileManager reloadManager;
	Table* temp = new Table(meta->tableName, meta->rowCount + 10);
	buildTableFromCatalog(temp, meta);
	reloadManager.loadTable(temp, meta->filePath);

	int found = 0;
	int rowCount = temp->getRowCount();
	for (int i = 0; i < rowCount; ++i) {
		Row* row = temp->getRow(i);
		if (row == 0) {
			continue;
		}
		Field* field = row->getField(0);
		if (field == 0) {
			continue;
		}
		int key = static_cast<int>(field->toDouble());
		if (key >= baseKey && key < baseKey + 5) {
			found += 1;
		}
	}

	Logger::logf("[LOG] PERSIST_TEST inserted 5 rows; fresh reload found %d", found);
	delete temp;
}

void QueryExecutor::executeInsert(const char* query) {
	if (query == 0) {
		return;
	}

	Tokenizer tokenizer(query);
	tokenizer.tokenize();
	Token* tokens = tokenizer.getTokens();
	int tokenCount = tokenizer.getTokenCount();

	char tableName[64];
	tableName[0] = '\0';

	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, "INTO")) {
			if (i + 1 < tokenCount && tokens[i + 1].type == TOKEN_IDENTIFIER) {
				copyString(tableName, static_cast<int>(sizeof(tableName)), tokens[i + 1].value);
			}
			break;
		}
	}

	if (tableName[0] == '\0') {
		return;
	}

	Table* table = ensureTableLoaded(tableName);
	if (table == 0) {
		return;
	}

	char values[1024];
	if (!extractValueList(query, values, static_cast<int>(sizeof(values)))) {
		return;
	}

	Row* row = new Row(64);
	char token[256];
	int tokenLen = 0;
	bool inString = false;

	for (int i = 0; values[i] != '\0'; ++i) {
		char c = values[i];
		if (c == '"') {
			inString = !inString;
		}

		if (c == ',' && !inString) {
			token[tokenLen] = '\0';
			Field* field = buildFieldFromToken(token);
			row->addField(field);
			tokenLen = 0;
		} else {
			if (tokenLen < static_cast<int>(sizeof(token)) - 1) {
				token[tokenLen] = c;
				tokenLen += 1;
			}
		}
	}

	if (tokenLen > 0) {
		token[tokenLen] = '\0';
		Field* field = buildFieldFromToken(token);
		row->addField(field);
	}

	table->addRow(row);

	TableMeta* meta = catalog != 0 ? catalog->lookup(tableName) : 0;
	if (meta != 0) {
		meta->rowCount += 1;
		if (fileManager != 0) {
			fileManager->saveTable(table, meta->filePath);
		}
	}

	Field* keyField = row->getField(0);
	if (keyField != 0) {
		int key = static_cast<int>(keyField->toDouble());
		int rowIndex = table->getRowCount() - 1;
		int pageId = rowIndex / 128;
		int rowId = rowIndex % 128;
		if (equalsIgnoreCase(tableName, "customer")) {
			customerIndex->insert(key, pageId, rowId);
		} else if (equalsIgnoreCase(tableName, "orders")) {
			ordersIndex->insert(key, pageId, rowId);
		} else if (equalsIgnoreCase(tableName, "lineitem")) {
			lineitemIndex->insert(key, pageId, rowId);
		}
	}
}

void QueryExecutor::executeUpdate(const char* query) {
	if (query == 0) {
		return;
	}

	Tokenizer tokenizer(query);
	tokenizer.tokenize();
	Token* tokens = tokenizer.getTokens();
	int tokenCount = tokenizer.getTokenCount();

	char tableName[64];
	tableName[0] = '\0';
	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, "UPDATE")) {
			if (i + 1 < tokenCount && tokens[i + 1].type == TOKEN_IDENTIFIER) {
				copyString(tableName, static_cast<int>(sizeof(tableName)), tokens[i + 1].value);
			}
			break;
		}
	}

	if (tableName[0] == '\0') {
		return;
	}

	Table* table = ensureTableLoaded(tableName);
	if (table == 0) {
		return;
	}

	char setColumn[64];
	char setValue[128];
	char whereColumn[64];
	char whereValue[128];
	setColumn[0] = '\0';
	setValue[0] = '\0';
	whereColumn[0] = '\0';
	whereValue[0] = '\0';

	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, "SET")) {
			if (i + 1 < tokenCount && tokens[i + 1].type == TOKEN_IDENTIFIER) {
				copyString(setColumn, static_cast<int>(sizeof(setColumn)), tokens[i + 1].value);
			}
			if (i + 3 < tokenCount) {
				copyString(setValue, static_cast<int>(sizeof(setValue)), tokens[i + 3].value);
			}
		}

		if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, "WHERE")) {
			if (i + 1 < tokenCount && tokens[i + 1].type == TOKEN_IDENTIFIER) {
				copyString(whereColumn, static_cast<int>(sizeof(whereColumn)), tokens[i + 1].value);
			}
			if (i + 3 < tokenCount) {
				copyString(whereValue, static_cast<int>(sizeof(whereValue)), tokens[i + 3].value);
			}
		}
	}

	if (setColumn[0] == '\0' || whereColumn[0] == '\0') {
		return;
	}

	int setIndex = table->getColumnIndex(setColumn);
	int whereIndex = table->getColumnIndex(whereColumn);
	if (setIndex < 0 || whereIndex < 0) {
		return;
	}

	Field* whereField = buildFieldFromToken(whereValue);
	int rowCount = table->getRowCount();
	for (int i = 0; i < rowCount; ++i) {
		Row* row = table->getRow(i);
		if (row == 0) {
			continue;
		}

		Field* rowField = row->getField(whereIndex);
		if (rowField != 0 && (*rowField == *whereField)) {
			Field* updated = buildFieldFromToken(setValue);
			row->setField(setIndex, updated);
		}
	}
	delete whereField;

	TableMeta* meta = catalog != 0 ? catalog->lookup(tableName) : 0;
	if (meta != 0 && fileManager != 0) {
		fileManager->saveTable(table, meta->filePath);
	}
}

void QueryExecutor::executeJoin(Token* tokens, int tokenCount, const char* tables[], int count) {
	if (tables == 0 || count < 2) {
		return;
	}

	delete optimizer;
	delete joinGraph;
	joinGraph = new Graph();
	optimizer = new MSTOptimizer(joinGraph);

	for (int i = 0; i < count; ++i) {
		joinGraph->addNode(tables[i]);
	}

	int customer = joinGraph->getNodeIndex("customer");
	int orders = joinGraph->getNodeIndex("orders");
	int lineitem = joinGraph->getNodeIndex("lineitem");

	if (count == 3 && customer >= 0 && orders >= 0 && lineitem >= 0) {
		joinGraph->addEdge(customer, orders, 20000);
		joinGraph->addEdge(orders, lineitem, 30000);
		joinGraph->addEdge(customer, lineitem, 50000);
	} else {
		for (int i = 0; i < count; ++i) {
			for (int j = i + 1; j < count; ++j) {
				int weight = 1;
				TableMeta* metaLeft = catalog != 0 ? catalog->lookup(tables[i]) : 0;
				TableMeta* metaRight = catalog != 0 ? catalog->lookup(tables[j]) : 0;
				if (metaLeft != 0 && metaRight != 0) {
					weight = metaLeft->rowCount > metaRight->rowCount ? metaLeft->rowCount : metaRight->rowCount;
				}
				int left = joinGraph->getNodeIndex(tables[i]);
				int right = joinGraph->getNodeIndex(tables[j]);
				joinGraph->addEdge(left, right, weight);
			}
		}
	}

	optimizer->executeJoin(0, count, 0, 0);

	const char* mstOrder[10];
	int mstCount = 0;
	optimizer->getJoinOrder(mstOrder, mstCount);

	Table* joinTables[5];
	for (int i = 0; i < count; ++i) {
		joinTables[i] = ensureTableLoaded(tables[i]);
		if (joinTables[i] == 0) {
			Logger::logf("[ERROR] Join table not available: %s", tables[i]);
			return;
		}
	}

	const char* orderedNames[5];
	Table* orderedTables[5];
	int orderedOriginalIndex[5];
	reorderJoinTables(tables, joinTables, count, mstOrder, mstCount, orderedNames, orderedTables, orderedOriginalIndex);

	int whereIndex = -1;
	for (int i = 0; i < tokenCount; ++i) {
		if (tokens[i].type == TOKEN_KEYWORD && equalsIgnoreCase(tokens[i].value, "WHERE")) {
			whereIndex = i + 1;
			break;
		}
	}

	Token* exprTokens = 0;
	int exprCount = 0;
	Token* postfix = 0;
	int postfixCount = 0;
	if (whereIndex >= 0) {
		for (int i = whereIndex; i < tokenCount; ++i) {
			exprCount += 1;
			if (tokens[i].type == TOKEN_END) {
				break;
			}
		}

		exprTokens = new Token[exprCount + 1];
		int writeIndex = 0;
		for (int i = whereIndex; i < tokenCount && writeIndex < exprCount; ++i) {
			exprTokens[writeIndex] = tokens[i];
			writeIndex += 1;
			if (tokens[i].type == TOKEN_END) {
				break;
			}
		}
		exprTokens[writeIndex].type = TOKEN_END;
		exprTokens[writeIndex].value[0] = '\0';

		PostfixConverter converter;
		postfix = converter.convert(exprTokens, writeIndex, postfixCount);
	}

	int filterTable = -1;
	if (whereIndex >= 0 && postfix != 0) {
		int matches[5];
		for (int i = 0; i < count; ++i) {
			matches[i] = 0;
		}
		for (int i = whereIndex; i < tokenCount; ++i) {
			if (tokens[i].type == TOKEN_END) {
				break;
			}
			if (tokens[i].type != TOKEN_IDENTIFIER) {
				continue;
			}
			for (int t = 0; t < count; ++t) {
				if (joinTables[t]->getColumnIndex(tokens[i].value) >= 0) {
					matches[t] += 1;
				}
			}
		}

		int candidate = -1;
		for (int t = 0; t < count; ++t) {
			if (matches[t] > 0) {
				if (candidate == -1) {
					candidate = t;
				} else {
					candidate = -2;
					break;
				}
			}
		}
		if (candidate >= 0) {
			filterTable = candidate;
		}
	}

	ExpressionEvaluator evaluator;
	const int maxOutput = 50;
	int printed = 0;
	int totalMatches = 0;
	bool stop = false;

	if (count == 2) {
		const char* leftName = orderedNames[0];
		const char* rightName = orderedNames[1];
		const char* leftColumn = 0;
		const char* rightColumn = 0;
		if (!getJoinColumnNames(leftName, rightName, leftColumn, rightColumn)) {
			Logger::logf("[ERROR] Unsupported join pair: %s, %s", leftName, rightName);
			delete[] exprTokens;
			return;
		}

		Table* left = orderedTables[0];
		Table* right = orderedTables[1];
		int leftKey = left->getColumnIndex(leftColumn);
		int rightKey = right->getColumnIndex(rightColumn);
		if (leftKey < 0 || rightKey < 0) {
			Logger::logf("[ERROR] Join columns missing for pair: %s, %s", leftName, rightName);
			delete[] exprTokens;
			return;
		}

		int leftRows = left->getRowCount();
		int rightRows = right->getRowCount();
		int lastLeftPage = -1;
		int lastRightPage = -1;

		for (int i = 0; i < leftRows && !stop; ++i) {
			if (bufferPool != 0) {
				int pageId = i / 128;
				if (pageId != lastLeftPage) {
					bufferPool->fetchPage(pageId, leftName);
					lastLeftPage = pageId;
				}
			}

			Row* leftRow = left->getRow(i);
			if (leftRow == 0) {
				continue;
			}
			if (filterTable == orderedOriginalIndex[0] && postfix != 0) {
				if (!evaluator.evaluate(postfix, postfixCount, leftRow, left)) {
					continue;
				}
			}

			Field* leftField = leftRow->getField(leftKey);
			if (leftField == 0) {
				continue;
			}
			int leftVal = static_cast<int>(leftField->toDouble());

			for (int j = 0; j < rightRows; ++j) {
				if (bufferPool != 0) {
					int pageId = j / 128;
					if (pageId != lastRightPage) {
						bufferPool->fetchPage(pageId, rightName);
						lastRightPage = pageId;
					}
				}

				Row* rightRow = right->getRow(j);
				if (rightRow == 0) {
					continue;
				}
				if (filterTable == orderedOriginalIndex[1] && postfix != 0) {
					if (!evaluator.evaluate(postfix, postfixCount, rightRow, right)) {
						continue;
					}
				}

				Field* rightField = rightRow->getField(rightKey);
				if (rightField == 0) {
					continue;
				}
				if (leftVal == static_cast<int>(rightField->toDouble())) {
					Row* rows[5];
					for (int r = 0; r < 5; ++r) {
						rows[r] = 0;
					}
					placeRow(rows, 5, orderedOriginalIndex[0], leftRow);
					placeRow(rows, 5, orderedOriginalIndex[1], rightRow);
					totalMatches += 1;
					if (printed < maxOutput) {
						printJoinedRow(rows, count);
						printed += 1;
					}
					if (printed >= maxOutput) {
						stop = true;
						break;
					}
				}
			}
		}
	} else if (count == 3) {
		const char* firstName = orderedNames[0];
		const char* secondName = orderedNames[1];
		const char* thirdName = orderedNames[2];
		const char* firstLeftColumn = 0;
		const char* firstRightColumn = 0;
		const char* secondLeftColumn = 0;
		const char* secondRightColumn = 0;

		if (!getJoinColumnNames(firstName, secondName, firstLeftColumn, firstRightColumn) ||
			!getJoinColumnNames(secondName, thirdName, secondLeftColumn, secondRightColumn)) {
			Logger::logf("[ERROR] Join order from MST contains unsupported edge sequence: %s -> %s -> %s",
				firstName, secondName, thirdName);
			delete[] exprTokens;
			return;
		}

		Table* firstTable = orderedTables[0];
		Table* secondTable = orderedTables[1];
		Table* thirdTable = orderedTables[2];
		int firstLeftKey = firstTable->getColumnIndex(firstLeftColumn);
		int firstRightKey = secondTable->getColumnIndex(firstRightColumn);
		int secondLeftKey = secondTable->getColumnIndex(secondLeftColumn);
		int secondRightKey = thirdTable->getColumnIndex(secondRightColumn);

		if (firstLeftKey < 0 || firstRightKey < 0 || secondLeftKey < 0 || secondRightKey < 0) {
			Logger::logf("[ERROR] Join columns missing for MST order: %s -> %s -> %s",
				firstName, secondName, thirdName);
			delete[] exprTokens;
			return;
		}

		int firstRows = firstTable->getRowCount();
		int secondRows = secondTable->getRowCount();
		int thirdRows = thirdTable->getRowCount();
		int lastFirstPage = -1;
		int lastSecondPage = -1;
		int lastThirdPage = -1;

		for (int i = 0; i < firstRows && !stop; ++i) {
			if (bufferPool != 0) {
				int pageId = i / 128;
				if (pageId != lastFirstPage) {
					bufferPool->fetchPage(pageId, firstName);
					lastFirstPage = pageId;
				}
			}

			Row* firstRow = firstTable->getRow(i);
			if (firstRow == 0) {
				continue;
			}
			if (filterTable == orderedOriginalIndex[0] && postfix != 0) {
				if (!evaluator.evaluate(postfix, postfixCount, firstRow, firstTable)) {
					continue;
				}
			}

			Field* firstField = firstRow->getField(firstLeftKey);
			if (firstField == 0) {
				continue;
			}
			int firstValue = static_cast<int>(firstField->toDouble());

			for (int j = 0; j < secondRows && !stop; ++j) {
				if (bufferPool != 0) {
					int pageId = j / 128;
					if (pageId != lastSecondPage) {
						bufferPool->fetchPage(pageId, secondName);
						lastSecondPage = pageId;
					}
				}

				Row* secondRow = secondTable->getRow(j);
				if (secondRow == 0) {
					continue;
				}
				if (filterTable == orderedOriginalIndex[1] && postfix != 0) {
					if (!evaluator.evaluate(postfix, postfixCount, secondRow, secondTable)) {
						continue;
					}
				}

				Field* secondLeftField = secondRow->getField(firstRightKey);
				if (secondLeftField == 0 || static_cast<int>(secondLeftField->toDouble()) != firstValue) {
					continue;
				}

				Field* secondRightField = secondRow->getField(secondLeftKey);
				if (secondRightField == 0) {
					continue;
				}
				int secondValue = static_cast<int>(secondRightField->toDouble());

				for (int k = 0; k < thirdRows; ++k) {
					if (bufferPool != 0) {
						int pageId = k / 128;
						if (pageId != lastThirdPage) {
							bufferPool->fetchPage(pageId, thirdName);
							lastThirdPage = pageId;
						}
					}

					Row* thirdRow = thirdTable->getRow(k);
					if (thirdRow == 0) {
						continue;
					}
					if (filterTable == orderedOriginalIndex[2] && postfix != 0) {
						if (!evaluator.evaluate(postfix, postfixCount, thirdRow, thirdTable)) {
							continue;
						}
					}

					Field* thirdField = thirdRow->getField(secondRightKey);
					if (thirdField == 0 || static_cast<int>(thirdField->toDouble()) != secondValue) {
						continue;
					}

					Row* rows[5];
					for (int r = 0; r < 5; ++r) {
						rows[r] = 0;
					}
					placeRow(rows, 5, orderedOriginalIndex[0], firstRow);
					placeRow(rows, 5, orderedOriginalIndex[1], secondRow);
					placeRow(rows, 5, orderedOriginalIndex[2], thirdRow);
					totalMatches += 1;
					if (printed < maxOutput) {
						printJoinedRow(rows, count);
						printed += 1;
					}
					if (printed >= maxOutput) {
						stop = true;
						break;
					}
				}
			}
		}
	} else {
		Logger::logf("[ERROR] Join supports up to 3 tables");
	}

	Logger::logf("[LOG] Join matched %d rows (showing %d)", totalMatches, printed);
	delete[] exprTokens;
}

void QueryExecutor::initLog(const char* logPath) {
	if (logPath == 0) {
		return;
	}
	if (logFile != 0) {
		std::fclose(logFile);
	}
	logFile = std::fopen(logPath, "w");
	Logger::setFile(logFile);
}

void QueryExecutor::writeLog(const char* message) {
	if (message == 0) {
		return;
	}
	Logger::log(message);
}

void QueryExecutor::closeLog() {
	if (logFile != 0) {
		std::fclose(logFile);
		logFile = 0;
	}
	Logger::setFile(0);
}

bool QueryExecutor::prepareDefaultData(const char* dataDir, char* outMessage, int outSize) {
	if (outMessage != 0 && outSize > 0) {
		outMessage[0] = '\0';
	}
	if (dataDir == 0 || dataDir[0] == '\0') {
		copyString(outMessage, outSize, "Set a data folder before building the database.");
		return false;
	}
	if (catalog == 0 || fileManager == 0) {
		copyString(outMessage, outSize, "Catalog or file manager not available.");
		return false;
	}

	const char* tables[] = { "customer", "orders", "lineitem" };
	const int tableCount = 3;
	bool allReady = true;
	bool builtAny = false;
	bool missingTbl = false;

	for (int i = 0; i < tableCount; ++i) {
		const char* tableName = tables[i];
		TableMeta* meta = catalog->lookup(tableName);
		if (meta == 0) {
			allReady = false;
			continue;
		}

		if (fileExists(meta->filePath)) {
			continue;
		}

		char tblFile[96];
		std::snprintf(tblFile, static_cast<size_t>(sizeof(tblFile)), "%s.tbl", tableName);
		char tblPath[256];
		joinPath(dataDir, tblFile, tblPath, static_cast<int>(sizeof(tblPath)));
		if (!fileExists(tblPath)) {
			missingTbl = true;
			allReady = false;
			continue;
		}

		int capacity = meta->rowCount > 0 ? meta->rowCount + 100 : 100;
		Table* tempTable = new Table(meta->tableName, capacity);
		buildTableFromCatalog(tempTable, meta);
		fileManager->loadTBLFile(tblPath, tempTable);
		fileManager->saveTable(tempTable, meta->filePath);
		delete tempTable;
		builtAny = true;
	}

	if (allReady) {
		copyString(outMessage, outSize, builtAny ? "Database built from .tbl files." : "Database already exists.");
		return true;
	}

	if (missingTbl) {
		copyString(outMessage, outSize, "Missing .tbl files in the data folder.");
	} else {
		copyString(outMessage, outSize, "Database not ready. Check catalog entries.");
	}
	return false;
}

Table* QueryExecutor::getOrLoadTable(const char* name) {
	return ensureTableLoaded(name);
}

PriorityQueue* QueryExecutor::getQueue() {
	return queryQueue;
}

AVLTree* QueryExecutor::getIndex(const char* name) {
	if (name == 0) {
		return 0;
	}
	if (equalsIgnoreCase(name, "customer")) {
		return customerIndex;
	}
	if (equalsIgnoreCase(name, "orders")) {
		return ordersIndex;
	}
	if (equalsIgnoreCase(name, "lineitem")) {
		return lineitemIndex;
	}
	return 0;
}

BufferPool* QueryExecutor::getBufferPool() {
	return bufferPool;
}

TableMeta* QueryExecutor::getTableMeta(const char* name) {
	if (catalog == 0) {
		return 0;
	}
	return catalog->lookup(name);
}
