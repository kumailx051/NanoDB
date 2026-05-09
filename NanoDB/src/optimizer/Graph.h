#pragma once

class Graph {
	static const int MAX_NODES = 10;
	int adjMatrix[MAX_NODES][MAX_NODES];
	char nodeNames[MAX_NODES][64];
	int nodeCount;

	void copyString(char* dest, int destSize, const char* src);

public:
	Graph();
	void addNode(const char* tableName);
	void addEdge(int from, int to, int weight);
	int getNodeIndex(const char* name);
	int getWeight(int from, int to);
	const char* getNodeName(int index);
	void printGraph();
	int getNodeCount();
	int* getAdjRow(int node);
};
