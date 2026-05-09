#include "AVLTree.h"
#include "schema/Table.h"
#include "schema/Row.h"
#include "schema/Field.h"
#include "util/Logger.h"
#include <cstdio>
#include <chrono>

AVLNode::AVLNode(int k, int pid, int rid) {
	key = k;
	pageId = pid;
	rowIndex = rid;
	height = 1;
	left = 0;
	right = 0;
}

AVLTree::AVLTree() {
	root = 0;
	lastSearchComparisons = 0;
}

AVLTree::~AVLTree() {
	destroyTree(root);
	root = 0;
}

int AVLTree::height(AVLNode* n) {
	return n == 0 ? 0 : n->height;
}

int AVLTree::balanceFactor(AVLNode* n) {
	if (n == 0) {
		return 0;
	}
	return height(n->left) - height(n->right);
}

void AVLTree::updateHeight(AVLNode* n) {
	if (n == 0) {
		return;
	}
	int leftHeight = height(n->left);
	int rightHeight = height(n->right);
	n->height = (leftHeight > rightHeight ? leftHeight : rightHeight) + 1;
}

AVLNode* AVLTree::rotateRight(AVLNode* y) {
	AVLNode* x = y->left;
	AVLNode* t2 = x->right;

	x->right = y;
	y->left = t2;

	updateHeight(y);
	updateHeight(x);
	return x;
}

AVLNode* AVLTree::rotateLeft(AVLNode* x) {
	AVLNode* y = x->right;
	AVLNode* t2 = y->left;

	y->left = x;
	x->right = t2;

	updateHeight(x);
	updateHeight(y);
	return y;
}

AVLNode* AVLTree::rotateLeftRight(AVLNode* n) {
	n->left = rotateLeft(n->left);
	return rotateRight(n);
}

AVLNode* AVLTree::rotateRightLeft(AVLNode* n) {
	n->right = rotateRight(n->right);
	return rotateLeft(n);
}

AVLNode* AVLTree::balance(AVLNode* n) {
	if (n == 0) {
		return 0;
	}

	updateHeight(n);
	int bf = balanceFactor(n);

	if (bf > 1) {
		if (balanceFactor(n->left) >= 0) {
			return rotateRight(n);
		}
		return rotateLeftRight(n);
	}

	if (bf < -1) {
		if (balanceFactor(n->right) <= 0) {
			return rotateLeft(n);
		}
		return rotateRightLeft(n);
	}

	return n;
}

AVLNode* AVLTree::insert(AVLNode* n, int key, int pageId, int rowId) {
	if (n == 0) {
		return new AVLNode(key, pageId, rowId);
	}

	if (key < n->key) {
		n->left = insert(n->left, key, pageId, rowId);
	} else if (key > n->key) {
		n->right = insert(n->right, key, pageId, rowId);
	} else {
		n->pageId = pageId;
		n->rowIndex = rowId;
		return n;
	}

	return balance(n);
}

AVLNode* AVLTree::minNode(AVLNode* n) {
	AVLNode* current = n;
	while (current != 0 && current->left != 0) {
		current = current->left;
	}
	return current;
}

AVLNode* AVLTree::deleteNode(AVLNode* n, int key) {
	if (n == 0) {
		return 0;
	}

	if (key < n->key) {
		n->left = deleteNode(n->left, key);
	} else if (key > n->key) {
		n->right = deleteNode(n->right, key);
	} else {
		if (n->left == 0 || n->right == 0) {
			AVLNode* child = n->left != 0 ? n->left : n->right;
			delete n;
			return child;
		}

		AVLNode* successor = minNode(n->right);
		n->key = successor->key;
		n->pageId = successor->pageId;
		n->rowIndex = successor->rowIndex;
		n->right = deleteNode(n->right, successor->key);
	}

	return balance(n);
}

AVLNode* AVLTree::search(AVLNode* n, int key) {
	if (n == 0) {
		return 0;
	}

	if (key == n->key) {
		return n;
	}

	if (key < n->key) {
		return search(n->left, key);
	}
	return search(n->right, key);
}

void AVLTree::destroyTree(AVLNode* n) {
	if (n == 0) {
		return;
	}

	destroyTree(n->left);
	destroyTree(n->right);
	delete n;
}

void AVLTree::printInOrder(AVLNode* n) {
	if (n == 0) {
		return;
	}

	printInOrder(n->left);
	std::printf("%d -> page %d row %d\n", n->key, n->pageId, n->rowIndex);
	printInOrder(n->right);
}

int AVLTree::countNodes(AVLNode* n) const {
	if (n == 0) {
		return 0;
	}
	return 1 + countNodes(n->left) + countNodes(n->right);
}

void AVLTree::insert(int key, int pageId, int rowId) {
	root = insert(root, key, pageId, rowId);
}

void AVLTree::remove(int key) {
	root = deleteNode(root, key);
}

AVLNode* AVLTree::search(int key) {
	return search(root, key);
}

void AVLTree::buildIndex(Table* table, int columnIndex) {
	if (table == 0) {
		return;
	}

	const int rowsPerPage = 128;
	int rowCount = table->getRowCount();
	for (int i = 0; i < rowCount; ++i) {
		Row* row = table->getRow(i);
		if (row == 0) {
			continue;
		}

		Field* field = row->getField(columnIndex);
		if (field == 0) {
			continue;
		}

		int key = static_cast<int>(field->toDouble());
		int pageId = i / rowsPerPage;
		int rowId = i % rowsPerPage;
		insert(key, pageId, rowId);
	}
}

void AVLTree::printTree() {
	printInOrder(root);
}

AVLNode* AVLTree::searchWithLog(int key) {
	AVLNode* current = root;
	int comparisons = 0;

	while (current != 0) {
		comparisons += 1;
		if (key == current->key) {
			lastSearchComparisons = comparisons;
			Logger::logf("[LOG] AVL search complete in %d comparisons", comparisons);
			return current;
		}
		if (key < current->key) {
			current = current->left;
		} else {
			current = current->right;
		}
	}

	lastSearchComparisons = comparisons;
	Logger::logf("[LOG] AVL search complete in %d comparisons", comparisons);
	return 0;
}

int AVLTree::getHeight() {
	return height(root);
}

int AVLTree::getNodeCount() {
	return countNodes(root);
}

int AVLTree::getLastSearchComparisons() {
	return lastSearchComparisons;
}

void benchmarkSearch(AVLTree* tree, Table* table, int searchKey) {
	if (tree == 0 || table == 0) {
		return;
	}

	using Clock = std::chrono::high_resolution_clock;

	auto seqStart = Clock::now();
	int rowCount = table->getRowCount();
	for (int i = 0; i < rowCount; ++i) {
		Row* row = table->getRow(i);
		if (row == 0) {
			continue;
		}

		Field* field = row->getField(0);
		if (field == 0) {
			continue;
		}

		int key = static_cast<int>(field->toDouble());
		if (key == searchKey) {
			break;
		}
	}
	auto seqEnd = Clock::now();

	auto avlStart = Clock::now();
	tree->search(searchKey);
	auto avlEnd = Clock::now();

	double seqMs = std::chrono::duration_cast<std::chrono::microseconds>(seqEnd - seqStart).count() / 1000.0;
	double avlMs = std::chrono::duration_cast<std::chrono::microseconds>(avlEnd - avlStart).count() / 1000.0;
	double speedup = avlMs > 0.0 ? (seqMs / avlMs) : 0.0;

	Logger::logf("[BENCHMARK] Sequential scan: %.3fms | AVL Tree: %.3fms | Speedup: %.2fx",
				seqMs, avlMs, speedup);
}
