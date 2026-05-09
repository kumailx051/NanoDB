#pragma once

#include "Stack.h"
#include "Tokenizer.h"
#include "schema/Row.h"
#include "schema/Table.h"

class ExpressionEvaluator {
	Stack<double>* numStack;
	Stack<bool>* boolStack;
	Stack<const char*>* strStack;
	Stack<int>* typeStack;

	double stringToDouble(const char* text) const;
	int compareStrings(const char* left, const char* right) const;
	bool isStringType(const char* typeName) const;
	void clearStacks();

public:
	ExpressionEvaluator();
	~ExpressionEvaluator();
	bool evaluate(Token* postfix, int count, Row* row, Table* table);
	double applyMathOp(double a, double b, const char* op);
	bool applyCompareOp(double a, double b, const char* op);
	bool applyLogicalOp(bool a, bool b, const char* op);
};
