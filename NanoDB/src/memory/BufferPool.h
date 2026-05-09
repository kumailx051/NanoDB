#pragma once

#include "Page.h"
#include "LRUCache.h"
#include "engine/FileManager.h"

class BufferPool {
	static const int MAX_PAGES = 50;
	Page* pages[MAX_PAGES];
	LRUCache* lruCache;
	FileManager* fileManager;
	int pageCount;
	int pageFaultCount;
	int evictionCount;
	int lastEvictedPageId;

	int findPageIndex(int pageId);
	int findFreeIndex();
	void buildFilename(const char* tableName, int pageId, char* outPath, int outSize);

public:
	BufferPool(FileManager* fm);
	~BufferPool();
	Page* fetchPage(int pageId, const char* tableName);
	void flushPage(int pageId, const char* tableName);
	void flushAll();
	void logCacheEvent(const char* event, int pageId);
	int getMaxPages() const;
	int getUsedPages() const;
	int getPageFaultCount() const;
	int getEvictionCount() const;
	int getLastEvictedPageId() const;
	LRUCache* getLRUCache() const;
};
