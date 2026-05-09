#include "BufferPool.h"
#include "util/Logger.h"
#include <cstdio>
#include <cstring>

BufferPool::BufferPool(FileManager* fm) {
	fileManager = fm;
	lruCache = new LRUCache(MAX_PAGES);
	pageCount = 0;
	pageFaultCount = 0;
	evictionCount = 0;
	lastEvictedPageId = -1;

	for (int i = 0; i < MAX_PAGES; ++i) {
		pages[i] = new Page();
		pages[i]->clear();
	}
}

BufferPool::~BufferPool() {
	flushAll();

	for (int i = 0; i < MAX_PAGES; ++i) {
		delete pages[i];
		pages[i] = 0;
	}

	delete lruCache;
	lruCache = 0;
}

int BufferPool::findPageIndex(int pageId) {
	for (int i = 0; i < MAX_PAGES; ++i) {
		if (pages[i]->isOccupied && pages[i]->pageId == pageId) {
			return i;
		}
	}
	return -1;
}

int BufferPool::findFreeIndex() {
	for (int i = 0; i < MAX_PAGES; ++i) {
		if (!pages[i]->isOccupied) {
			return i;
		}
	}
	return -1;
}

void BufferPool::buildFilename(const char* tableName, int pageId, char* outPath, int outSize) {
	if (outPath == 0 || outSize <= 0) {
		return;
	}

	if (tableName == 0 || tableName[0] == '\0') {
		std::snprintf(outPath, static_cast<size_t>(outSize), "data/page_%d.bin", pageId);
		return;
	}

	std::snprintf(outPath, static_cast<size_t>(outSize), "data/%s_%d.bin", tableName, pageId);
}

Page* BufferPool::fetchPage(int pageId, const char* tableName) {
	Page* cachedPage = lruCache->get(pageId);
	if (cachedPage != 0) {
		return cachedPage;
	}

	pageFaultCount += 1;

	if (lruCache->isFull()) {
		int evictedId = lruCache->evict();
		if (evictedId >= 0) {
			flushPage(evictedId, tableName);

			int evictIndex = findPageIndex(evictedId);
			if (evictIndex >= 0) {
				pages[evictIndex]->clear();
				pages[evictIndex]->isOccupied = false;
				pageCount -= 1;
			}

			logCacheEvent("evicted via LRU, written to disk", evictedId);
			evictionCount += 1;
			lastEvictedPageId = evictedId;
		}
	}

	int freeIndex = findFreeIndex();
	if (freeIndex < 0) {
		return 0;
	}

	Page* targetPage = pages[freeIndex];
	targetPage->clear();
	targetPage->pageId = pageId;
	targetPage->isOccupied = true;
	targetPage->isDirty = false;

	char filename[256];
	buildFilename(tableName, pageId, filename, 256);
	targetPage->deserialize(filename);

	lruCache->put(pageId, targetPage);
	pageCount += 1;

	return targetPage;
}

void BufferPool::flushPage(int pageId, const char* tableName) {
	int index = findPageIndex(pageId);
	if (index < 0) {
		return;
	}

	Page* page = pages[index];
	if (!page->isOccupied || !page->isDirty) {
		return;
	}

	char filename[256];
	buildFilename(tableName, pageId, filename, 256);
	page->serialize(filename);
	page->isDirty = false;
}

void BufferPool::flushAll() {
	for (int i = 0; i < MAX_PAGES; ++i) {
		if (pages[i]->isOccupied && pages[i]->isDirty) {
			char filename[256];
			buildFilename(0, pages[i]->pageId, filename, 256);
			pages[i]->serialize(filename);
			pages[i]->isDirty = false;
		}
	}
}

void BufferPool::logCacheEvent(const char* event, int pageId) {
	if (event == 0) {
		return;
	}
	Logger::logf("[LOG] Page %d %s", pageId, event);
}

int BufferPool::getMaxPages() const {
	return MAX_PAGES;
}

int BufferPool::getUsedPages() const {
	return pageCount;
}

int BufferPool::getPageFaultCount() const {
	return pageFaultCount;
}

int BufferPool::getEvictionCount() const {
	return evictionCount;
}

int BufferPool::getLastEvictedPageId() const {
	return lastEvictedPageId;
}

LRUCache* BufferPool::getLRUCache() const {
	return lruCache;
}
