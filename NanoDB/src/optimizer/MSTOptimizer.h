#pragma once

#include "Graph.h"

class Table;

struct MSTEdge {
	int from;
	int to;
	int weight;
};

class MSTOptimizer {
	Graph* graph;

	void sortEdges(MSTEdge* edges, int count);

	int parent[10];
	int rank_arr[10];
	int find(int x);
	void unite(int x, int y);

	void resetUnionFind(int nodeCount);
	void buildJoinOrderFromMST(MSTEdge* edges, int edgeCount, const char** order, int& count);
	void traverseMST(int node, int parent, int neighbors[10][10], int neighborCount[10],
					 const char** order, int& count);

public:
	MSTOptimizer(Graph* g);
	MSTEdge* findMST(int& edgeCount);
	void getJoinOrder(const char** order, int& count);
	void executeJoin(Table** tables, int tableCount,
					 const char** joinColumns, int joinCount);
};
