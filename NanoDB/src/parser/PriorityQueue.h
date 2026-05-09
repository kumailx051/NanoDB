#pragma once

struct QueryItem {
	char query[512];
	int priority;
	int timestamp;
};

class PriorityQueue {
	QueryItem* heap;
	int size;
	int capacity;
	int nextTimestamp;

	void heapifyUp(int index);
	void heapifyDown(int index);
	void swap(int i, int j);
	int compareItems(const QueryItem& a, const QueryItem& b) const;
	void copyString(char* dest, int destSize, const char* src);

public:
	PriorityQueue(int cap = 100);
	~PriorityQueue();
	void enqueue(const char* query, int priority);
	QueryItem dequeue();
	bool isEmpty();
	void printQueue();
	int getSize() const;
	const QueryItem* getItems() const;
};
