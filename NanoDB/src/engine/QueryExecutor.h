#pragma once

#include "memory/BufferPool.h"
#include "catalog/SystemCatalog.h"
#include "index/AVLTree.h"
#include "optimizer/MSTOptimizer.h"
#include "parser/PriorityQueue.h"
#include "parser/Tokenizer.h"
#include "engine/FileManager.h"

#include <cstdio>
class Table;
class Field;

class QueryExecutor {
	BufferPool* bufferPool;
	SystemCatalog* catalog;
	AVLTree* customerIndex;
	AVLTree* ordersIndex;
	AVLTree* lineitemIndex;
	MSTOptimizer* optimizer;
	PriorityQueue* queryQueue;
	FileManager* fileManager;
	Graph* joinGraph;
	std::FILE* logFile;

	Table* customerTable;
	Table* ordersTable;
	Table* lineitemTable;
	bool customerLoaded;
	bool ordersLoaded;
	bool lineitemLoaded;
    int maxResultPrintRows;

	void executeSelect(const char* query);
	void executeSelectSeq(const char* query);
	void executeSelectIdx(const char* query);
	void executeStressTest(const char* query);
	void executePersistenceTest();
	void executeInsert(const char* query);
	void executeUpdate(const char* query);
	void executeJoin(Token* tokens, int tokenCount, const char* tables[], int count);

	Table* getTableByName(const char* name);
	bool isTableLoaded(const char* name);
	void markTableLoaded(const char* name);
	Table* ensureTableLoaded(const char* name);
	void buildTableFromCatalog(Table* table, const TableMeta* meta);

	void extractFirstWord(const char* query, char* outWord, int outSize) const;
	const char* skipFirstWord(const char* text) const;
	void trimWhitespace(char* text) const;
	bool equalsIgnoreCase(const char* a, const char* b) const;
	bool extractValueList(const char* query, char* out, int outSize) const;
	Field* buildFieldFromToken(const char* token) const;
	void parseJoinTablesFromTokens(Token* tokens, int tokenCount, char names[][64], int& count);
	bool extractTableFromTokens(Token* tokens, int tokenCount, const char* keyword, char* outName, int outSize) const;
	bool extractWhereColumn(Token* tokens, int tokenCount, char* outName, int outSize) const;
	bool extractFirstNumberAfterWhere(Token* tokens, int tokenCount, int& outValue) const;

public:
	QueryExecutor(BufferPool* bp, SystemCatalog* cat);
	~QueryExecutor();

	// Limit for how many result rows to print for a single query (0 = no limit)
	void setMaxResultPrintRows(int maxRows);

	void submitQuery(const char* query, int priority);
	void processQueue();

	void runWorkloadFile(const char* filepath);

	void initLog(const char* logPath);
	void writeLog(const char* message);
	void closeLog();
	bool prepareDefaultData(const char* dataDir, char* outMessage, int outSize);

	Table* getOrLoadTable(const char* name);
	PriorityQueue* getQueue();
	AVLTree* getIndex(const char* name);
	BufferPool* getBufferPool();
	TableMeta* getTableMeta(const char* name);
};
