#include "PriorityQueue.h"
#include <cstdio>

PriorityQueue::PriorityQueue(int cap) {
	capacity = cap > 0 ? cap : 1;
	heap = new QueryItem[capacity];
	size = 0;
	nextTimestamp = 0;
}

PriorityQueue::~PriorityQueue() {
	delete[] heap;
	heap = 0;
	size = 0;
	capacity = 0;
	nextTimestamp = 0;
}

void PriorityQueue::copyString(char* dest, int destSize, const char* src) {
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

int PriorityQueue::compareItems(const QueryItem& a, const QueryItem& b) const {
	if (a.priority < b.priority) {
		return -1;
	}
	if (a.priority > b.priority) {
		return 1;
	}
	if (a.timestamp < b.timestamp) {
		return -1;
	}
	if (a.timestamp > b.timestamp) {
		return 1;
	}
	return 0;
}

void PriorityQueue::swap(int i, int j) {
	QueryItem temp = heap[i];
	heap[i] = heap[j];
	heap[j] = temp;
}

void PriorityQueue::heapifyUp(int index) {
	while (index > 0) {
		int parent = (index - 1) / 2;
		if (compareItems(heap[index], heap[parent]) < 0) {
			swap(index, parent);
			index = parent;
		} else {
			break;
		}
	}
}

void PriorityQueue::heapifyDown(int index) {
	while (true) {
		int left = index * 2 + 1;
		int right = index * 2 + 2;
		int smallest = index;

		if (left < size && compareItems(heap[left], heap[smallest]) < 0) {
			smallest = left;
		}
		if (right < size && compareItems(heap[right], heap[smallest]) < 0) {
			smallest = right;
		}

		if (smallest != index) {
			swap(index, smallest);
			index = smallest;
		} else {
			break;
		}
	}
}

void PriorityQueue::enqueue(const char* query, int priority) {
	if (size >= capacity) {
		return;
	}

	QueryItem item;
	copyString(item.query, static_cast<int>(sizeof(item.query)), query);
	item.priority = priority;
	item.timestamp = nextTimestamp;
	nextTimestamp += 1;

	heap[size] = item;
	heapifyUp(size);
	size += 1;
}

QueryItem PriorityQueue::dequeue() {
	QueryItem emptyItem;
	emptyItem.query[0] = '\0';
	emptyItem.priority = 1;
	emptyItem.timestamp = 0;

	if (size <= 0) {
		return emptyItem;
	}

	QueryItem root = heap[0];
	size -= 1;
	if (size > 0) {
		heap[0] = heap[size];
		heapifyDown(0);
	}

	return root;
}

bool PriorityQueue::isEmpty() {
	return size == 0;
}

void PriorityQueue::printQueue() {
	std::printf("PriorityQueue size=%d\n", size);
	for (int i = 0; i < size; ++i) {
		std::printf("[%d] prio=%d time=%d query=%s\n", i, heap[i].priority, heap[i].timestamp, heap[i].query);
	}
}

int PriorityQueue::getSize() const {
	return size;
}

const QueryItem* PriorityQueue::getItems() const {
	return heap;
}
