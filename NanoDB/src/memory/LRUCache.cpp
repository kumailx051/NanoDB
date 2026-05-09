#include "LRUCache.h"
#include "util/Logger.h"
#include <cstdio>

DLLNode::DLLNode(int id, Page* p) {
	pageId = id;
	page = p;
	prev = 0;
	next = 0;
	hashNext = 0;
}

LRUCache::LRUCache(int capacityValue) {
	capacity = capacityValue;
	size = 0;
	head = 0;
	tail = 0;
	evictionCount = 0;
	lastEvictedPageId = -1;

	for (int i = 0; i < HASH_SIZE; ++i) {
		hashTable[i] = 0;
	}
}

LRUCache::~LRUCache() {
	DLLNode* current = head;
	while (current != 0) {
		DLLNode* nextNode = current->next;
		delete current;
		current = nextNode;
	}

	head = 0;
	tail = 0;
	size = 0;

	for (int i = 0; i < HASH_SIZE; ++i) {
		hashTable[i] = 0;
	}
}

int LRUCache::hashIndex(int pageId) const {
	int index = pageId % HASH_SIZE;
	if (index < 0) {
		index += HASH_SIZE;
	}
	return index;
}

DLLNode* LRUCache::findNode(int pageId) {
	int index = hashIndex(pageId);
	DLLNode* current = hashTable[index];
	while (current != 0) {
		if (current->pageId == pageId) {
			return current;
		}
		current = current->hashNext;
	}
	return 0;
}

void LRUCache::addToFront(DLLNode* node) {
	node->prev = 0;
	node->next = head;

	if (head != 0) {
		head->prev = node;
	}

	head = node;

	if (tail == 0) {
		tail = node;
	}
}

void LRUCache::removeNode(DLLNode* node) {
	if (node->prev != 0) {
		node->prev->next = node->next;
	} else {
		head = node->next;
	}

	if (node->next != 0) {
		node->next->prev = node->prev;
	} else {
		tail = node->prev;
	}

	node->prev = 0;
	node->next = 0;
}

void LRUCache::addToHash(DLLNode* node) {
	int index = hashIndex(node->pageId);
	node->hashNext = hashTable[index];
	hashTable[index] = node;
}

void LRUCache::removeFromHash(DLLNode* node) {
	int index = hashIndex(node->pageId);
	DLLNode* current = hashTable[index];
	DLLNode* previous = 0;

	while (current != 0) {
		if (current == node) {
			if (previous == 0) {
				hashTable[index] = current->hashNext;
			} else {
				previous->hashNext = current->hashNext;
			}
			current->hashNext = 0;
			return;
		}
		previous = current;
		current = current->hashNext;
	}
}

Page* LRUCache::get(int pageId) {
	DLLNode* node = findNode(pageId);
	if (node == 0) {
		return 0;
	}

	if (node != head) {
		removeNode(node);
		addToFront(node);
	}

	return node->page;
}

void LRUCache::put(int pageId, Page* p) {
	DLLNode* node = findNode(pageId);
	if (node != 0) {
		node->page = p;
		if (node != head) {
			removeNode(node);
			addToFront(node);
		}
		return;
	}

	DLLNode* newNode = new DLLNode(pageId, p);
	addToFront(newNode);
	addToHash(newNode);
	size += 1;
}

int LRUCache::evict() {
	if (tail == 0) {
		return -1;
	}

	DLLNode* node = tail;
	int evictedId = node->pageId;

	removeNode(node);
	removeFromHash(node);
	delete node;
	size -= 1;

	evictionCount += 1;
	lastEvictedPageId = evictedId;

	return evictedId;
}

bool LRUCache::isFull() {
	return size >= capacity;
}

void LRUCache::logEviction(int pageId) {
	Logger::logf("[LOG] Page %d evicted via LRU, written to disk", pageId);
}

DLLNode* LRUCache::getHead() const {
	return head;
}

int LRUCache::getSize() const {
	return size;
}

int LRUCache::getCapacity() const {
	return capacity;
}

int LRUCache::getEvictionCount() const {
	return evictionCount;
}

int LRUCache::getLastEvictedPageId() const {
	return lastEvictedPageId;
}
