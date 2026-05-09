#pragma once

#include "Page.h"

struct DLLNode {
	int pageId;
	Page* page;
	DLLNode* prev;
	DLLNode* next;
	DLLNode* hashNext;

	DLLNode(int id, Page* p);
};

class LRUCache {
	int capacity;
	int size;
	DLLNode* head;
	DLLNode* tail;
	int evictionCount;
	int lastEvictedPageId;

	static const int HASH_SIZE = 256;
	DLLNode* hashTable[HASH_SIZE];

	int hashIndex(int pageId) const;
	DLLNode* findNode(int pageId);
	void addToFront(DLLNode* node);
	void removeNode(DLLNode* node);
	void addToHash(DLLNode* node);
	void removeFromHash(DLLNode* node);

public:
	LRUCache(int capacity);
	~LRUCache();
	Page* get(int pageId);
	void put(int pageId, Page* p);
	int evict();
	bool isFull();
	void logEviction(int pageId);
	DLLNode* getHead() const;
	int getSize() const;
	int getCapacity() const;
	int getEvictionCount() const;
	int getLastEvictedPageId() const;
};
