#include "Graph.h"
#include <cstdio>

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

Graph::Graph() {
	nodeCount = 0;
	for (int i = 0; i < MAX_NODES; ++i) {
		for (int j = 0; j < MAX_NODES; ++j) {
			adjMatrix[i][j] = 0;
		}
		nodeNames[i][0] = '\0';
	}
}

void Graph::copyString(char* dest, int destSize, const char* src) {
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

void Graph::addNode(const char* tableName) {
	if (tableName == 0 || nodeCount >= MAX_NODES) {
		return;
	}

	if (getNodeIndex(tableName) != -1) {
		return;
	}

	copyString(nodeNames[nodeCount], static_cast<int>(sizeof(nodeNames[nodeCount])), tableName);
	nodeCount += 1;
}

void Graph::addEdge(int from, int to, int weight) {
	if (from < 0 || to < 0 || from >= nodeCount || to >= nodeCount) {
		return;
	}
	adjMatrix[from][to] = weight;
	adjMatrix[to][from] = weight;
}

int Graph::getNodeIndex(const char* name) {
	if (name == 0) {
		return -1;
	}
	for (int i = 0; i < nodeCount; ++i) {
		if (compareStrings(nodeNames[i], name) == 0) {
			return i;
		}
	}
	return -1;
}

int Graph::getWeight(int from, int to) {
	if (from < 0 || to < 0 || from >= nodeCount || to >= nodeCount) {
		return 0;
	}
	return adjMatrix[from][to];
}

const char* Graph::getNodeName(int index) {
	if (index < 0 || index >= nodeCount) {
		return 0;
	}
	return nodeNames[index];
}

void Graph::printGraph() {
	std::printf("Graph nodes: %d\n", nodeCount);
	for (int i = 0; i < nodeCount; ++i) {
		std::printf("%s: ", nodeNames[i]);
		for (int j = 0; j < nodeCount; ++j) {
			std::printf("%d", adjMatrix[i][j]);
			if (j < nodeCount - 1) {
				std::printf(" ");
			}
		}
		std::printf("\n");
	}
}

int Graph::getNodeCount() {
	return nodeCount;
}

int* Graph::getAdjRow(int node) {
	if (node < 0 || node >= nodeCount) {
		return 0;
	}
	return adjMatrix[node];
}
