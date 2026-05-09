#pragma once

class Table;

struct AVLNode {
	int key;
	int pageId;
	int rowIndex;
	int height;
	AVLNode* left;
	AVLNode* right;

	AVLNode(int k, int pid, int rid);
};

class AVLTree {
	AVLNode* root;
	int lastSearchComparisons;

	int height(AVLNode* n);
	int balanceFactor(AVLNode* n);
	void updateHeight(AVLNode* n);

	AVLNode* rotateRight(AVLNode* y);
	AVLNode* rotateLeft(AVLNode* x);
	AVLNode* rotateLeftRight(AVLNode* n);
	AVLNode* rotateRightLeft(AVLNode* n);
	AVLNode* balance(AVLNode* n);

	AVLNode* insert(AVLNode* n, int key, int pageId, int rowId);
	AVLNode* deleteNode(AVLNode* n, int key);
	AVLNode* minNode(AVLNode* n);
	AVLNode* search(AVLNode* n, int key);
	void destroyTree(AVLNode* n);
	void printInOrder(AVLNode* n);
	int countNodes(AVLNode* n) const;

public:
	AVLTree();
	~AVLTree();
	void insert(int key, int pageId, int rowId);
	void remove(int key);
	AVLNode* search(int key);
	void buildIndex(Table* table, int columnIndex);
	void printTree();

	AVLNode* searchWithLog(int key);
	int getHeight();
	int getNodeCount();
	int getLastSearchComparisons();
};

void benchmarkSearch(AVLTree* tree, Table* table, int searchKey);
