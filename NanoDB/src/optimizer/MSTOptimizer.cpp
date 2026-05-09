#include "MSTOptimizer.h"
#include "util/Logger.h"
#include <cstdio>

MSTOptimizer::MSTOptimizer(Graph* g) {
	graph = g;
	for (int i = 0; i < 10; ++i) {
		parent[i] = i;
		rank_arr[i] = 0;
	}
}

void MSTOptimizer::resetUnionFind(int nodeCount) {
	for (int i = 0; i < nodeCount; ++i) {
		parent[i] = i;
		rank_arr[i] = 0;
	}
}

int MSTOptimizer::find(int x) {
	if (parent[x] != x) {
		parent[x] = find(parent[x]);
	}
	return parent[x];
}

void MSTOptimizer::unite(int x, int y) {
	int rootX = find(x);
	int rootY = find(y);
	if (rootX == rootY) {
		return;
	}

	if (rank_arr[rootX] < rank_arr[rootY]) {
		parent[rootX] = rootY;
	} else if (rank_arr[rootX] > rank_arr[rootY]) {
		parent[rootY] = rootX;
	} else {
		parent[rootY] = rootX;
		rank_arr[rootX] += 1;
	}
}

void MSTOptimizer::sortEdges(MSTEdge* edges, int count) {
	if (edges == 0 || count <= 1) {
		return;
	}

	for (int i = 0; i < count - 1; ++i) {
		for (int j = 0; j < count - i - 1; ++j) {
			if (edges[j].weight > edges[j + 1].weight) {
				MSTEdge temp = edges[j];
				edges[j] = edges[j + 1];
				edges[j + 1] = temp;
			}
		}
	}
}

void MSTOptimizer::traverseMST(int node, int parent, int neighbors[10][10], int neighborCount[10],
							   const char** order, int& count) {
	if (graph == 0 || order == 0) {
		return;
	}

	const char* name = graph->getNodeName(node);
	order[count] = name;
	count += 1;

	for (int i = 0; i < neighborCount[node]; ++i) {
		int nextNode = neighbors[node][i];
		if (nextNode == parent) {
			continue;
		}
		traverseMST(nextNode, node, neighbors, neighborCount, order, count);
	}
}

MSTEdge* MSTOptimizer::findMST(int& edgeCount) {
	edgeCount = 0;
	if (graph == 0) {
		return 0;
	}

	int nodes = graph->getNodeCount();
	if (nodes <= 1) {
		return 0;
	}

	int maxEdges = nodes * (nodes - 1) / 2;
	MSTEdge* edges = new MSTEdge[maxEdges];
	int count = 0;

	for (int i = 0; i < nodes; ++i) {
		for (int j = i + 1; j < nodes; ++j) {
			int weight = graph->getWeight(i, j);
			if (weight > 0) {
				edges[count].from = i;
				edges[count].to = j;
				edges[count].weight = weight;
				count += 1;
			}
		}
	}

	sortEdges(edges, count);
	resetUnionFind(nodes);

	MSTEdge* result = new MSTEdge[nodes - 1];
	int resultCount = 0;

	for (int i = 0; i < count && resultCount < nodes - 1; ++i) {
		int u = edges[i].from;
		int v = edges[i].to;
		if (find(u) != find(v)) {
			result[resultCount] = edges[i];
			resultCount += 1;
			unite(u, v);
		}
	}

	delete[] edges;
	edgeCount = resultCount;
	return result;
}

void MSTOptimizer::buildJoinOrderFromMST(MSTEdge* edges, int edgeCount, const char** order, int& count) {
	count = 0;
	if (edges == 0 || edgeCount <= 0 || order == 0 || graph == 0) {
		return;
	}

	int nodes = graph->getNodeCount();
	if (nodes <= 0) {
		return;
	}

	int neighbors[10][10];
	int neighborCount[10];
	for (int i = 0; i < nodes; ++i) {
		neighborCount[i] = 0;
		for (int j = 0; j < nodes; ++j) {
			neighbors[i][j] = -1;
		}
	}

	for (int i = 0; i < edgeCount; ++i) {
		int u = edges[i].from;
		int v = edges[i].to;
		neighbors[u][neighborCount[u]] = v;
		neighborCount[u] += 1;
		neighbors[v][neighborCount[v]] = u;
		neighborCount[v] += 1;
	}

	int startNode = 0;
	for (int i = 0; i < nodes; ++i) {
		if (neighborCount[i] == 1) {
			startNode = i;
			break;
		}
	}

	traverseMST(startNode, -1, neighbors, neighborCount, order, count);
}

void MSTOptimizer::getJoinOrder(const char** order, int& count) {
	count = 0;
	if (graph == 0 || order == 0) {
		return;
	}

	int edgeCount = 0;
	MSTEdge* mst = findMST(edgeCount);
	if (mst == 0 || edgeCount == 0) {
		delete[] mst;
		return;
	}

	buildJoinOrderFromMST(mst, edgeCount, order, count);
	delete[] mst;
}

void MSTOptimizer::executeJoin(Table** tables, int tableCount,
							   const char** joinColumns, int joinCount) {
	if (graph == 0 || tables == 0 || tableCount <= 0) {
		return;
	}

	int edgeCount = 0;
	MSTEdge* mst = findMST(edgeCount);
	if (mst == 0 || edgeCount == 0) {
		delete[] mst;
		return;
	}

	const char* order[10];
	int orderCount = 0;
	buildJoinOrderFromMST(mst, edgeCount, order, orderCount);

	if (orderCount > 0) {
		char buffer[512];
		int offset = 0;
		offset += std::snprintf(buffer + offset, static_cast<size_t>(sizeof(buffer) - offset),
			"[LOG] Multi-table join routed via MST: ");
		for (int i = 0; i < orderCount && offset < static_cast<int>(sizeof(buffer)); ++i) {
			if (order[i] != 0) {
				offset += std::snprintf(buffer + offset, static_cast<size_t>(sizeof(buffer) - offset),
					"%s", order[i]);
			}
			if (i < orderCount - 1) {
				offset += std::snprintf(buffer + offset, static_cast<size_t>(sizeof(buffer) - offset),
					" -> ");
			}
		}
		Logger::log(buffer);
	}

	delete[] mst;
}
