#include "ExpressionEvaluator.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

ExpressionEvaluator::ExpressionEvaluator() {
	numStack = new Stack<double>(256);
	boolStack = new Stack<bool>(256);
	strStack = new Stack<const char*>(256);
	typeStack = new Stack<int>(256);
}

ExpressionEvaluator::~ExpressionEvaluator() {
	delete numStack;
	delete boolStack;
	delete strStack;
	delete typeStack;
	numStack = 0;
	boolStack = 0;
	strStack = 0;
	typeStack = 0;
}

double ExpressionEvaluator::stringToDouble(const char* text) const {
	if (text == 0 || text[0] == '\0') {
		return 0.0;
	}
	char* endPtr = 0;
	double value = std::strtod(text, &endPtr);
	if (endPtr == text) {
		return 0.0;
	}
	return value;
}

int ExpressionEvaluator::compareStrings(const char* left, const char* right) const {
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

bool ExpressionEvaluator::isStringType(const char* typeName) const {
	if (typeName == 0) {
		return false;
	}
	return compareStrings(typeName, "STRING") == 0 || compareStrings(typeName, "VARCHAR") == 0;
}

void ExpressionEvaluator::clearStacks() {
	while (!numStack->isEmpty()) {
		numStack->pop();
	}
	while (!boolStack->isEmpty()) {
		boolStack->pop();
	}
	while (!strStack->isEmpty()) {
		strStack->pop();
	}
	while (!typeStack->isEmpty()) {
		typeStack->pop();
	}
}

bool ExpressionEvaluator::evaluate(Token* postfix, int count, Row* row, Table* table) {
	clearStacks();

	for (int i = 0; i < count; ++i) {
		Token token = postfix[i];
		if (token.type == TOKEN_END) {
			break;
		}

		if (token.type == TOKEN_NUMBER) {
			double value = stringToDouble(token.value);
			numStack->push(value);
			typeStack->push(0);
			continue;
		}

		if (token.type == TOKEN_STRING) {
			strStack->push(token.value);
			typeStack->push(1);
			continue;
		}

		if (token.type == TOKEN_IDENTIFIER) {
			Field* field = 0;
			if (table != 0 && row != 0) {
				int index = table->getColumnIndex(token.value);
				field = row->getField(index);
			}

			if (field != 0) {
				if (isStringType(field->getType())) {
					strStack->push(field->toString());
					typeStack->push(1);
				} else {
					numStack->push(field->toDouble());
					typeStack->push(0);
				}
			} else {
				numStack->push(0.0);
				typeStack->push(0);
			}
			continue;
		}

		if (token.type == TOKEN_MATH) {
			if (typeStack->size() < 2) {
				continue;
			}

			int rightType = typeStack->pop();
			int leftType = typeStack->pop();

			double rightValue = (rightType == 1) ? stringToDouble(strStack->pop()) : numStack->pop();
			double leftValue = (leftType == 1) ? stringToDouble(strStack->pop()) : numStack->pop();

			double result = applyMathOp(leftValue, rightValue, token.value);
			numStack->push(result);
			typeStack->push(0);
			continue;
		}

		if (token.type == TOKEN_OPERATOR) {
			if (typeStack->size() < 2) {
				continue;
			}

			int rightType = typeStack->pop();
			int leftType = typeStack->pop();

			const char* rightStr = 0;
			const char* leftStr = 0;
			double rightNum = 0.0;
			double leftNum = 0.0;

			if (rightType == 1) {
				rightStr = strStack->pop();
			} else {
				rightNum = numStack->pop();
			}

			if (leftType == 1) {
				leftStr = strStack->pop();
			} else {
				leftNum = numStack->pop();
			}

			bool result = false;
			if (leftType == 1 && rightType == 1) {
				int cmp = compareStrings(leftStr, rightStr);
				if (token.value[0] == '>' && token.value[1] == '=') {
					result = (cmp >= 0);
				} else if (token.value[0] == '<' && token.value[1] == '=') {
					result = (cmp <= 0);
				} else if (token.value[0] == '>' && token.value[1] == '\0') {
					result = (cmp > 0);
				} else if (token.value[0] == '<' && token.value[1] == '\0') {
					result = (cmp < 0);
				} else if (token.value[0] == '=' && token.value[1] == '=') {
					result = (cmp == 0);
				} else if (token.value[0] == '!' && token.value[1] == '=') {
					result = (cmp != 0);
				}
			} else {
				if (leftType == 1) {
					leftNum = stringToDouble(leftStr);
				}
				if (rightType == 1) {
					rightNum = stringToDouble(rightStr);
				}
				result = applyCompareOp(leftNum, rightNum, token.value);
			}

			boolStack->push(result);
			continue;
		}

		if (token.type == TOKEN_LOGICAL) {
			if (boolStack->size() < 2) {
				continue;
			}

			bool rightValue = boolStack->pop();
			bool leftValue = boolStack->pop();
			bool result = applyLogicalOp(leftValue, rightValue, token.value);
			boolStack->push(result);
			continue;
		}
	}

	if (!boolStack->isEmpty()) {
		return boolStack->pop();
	}

	if (!typeStack->isEmpty()) {
		int type = typeStack->pop();
		if (type == 1) {
			const char* text = strStack->pop();
			return text != 0 && text[0] != '\0';
		}
		return numStack->pop() != 0.0;
	}

	return false;
}

double ExpressionEvaluator::applyMathOp(double a, double b, const char* op) {
	if (op == 0) {
		return 0.0;
	}

	if (op[0] == '+') {
		return a + b;
	}
	if (op[0] == '-') {
		return a - b;
	}
	if (op[0] == '*') {
		return a * b;
	}
	if (op[0] == '/') {
		return b != 0.0 ? a / b : 0.0;
	}
	if (op[0] == '%') {
		return b != 0.0 ? std::fmod(a, b) : 0.0;
	}

	return 0.0;
}

bool ExpressionEvaluator::applyCompareOp(double a, double b, const char* op) {
	if (op == 0) {
		return false;
	}

	if (op[0] == '>' && op[1] == '=') {
		return a >= b;
	}
	if (op[0] == '<' && op[1] == '=') {
		return a <= b;
	}
	if (op[0] == '>' && op[1] == '\0') {
		return a > b;
	}
	if (op[0] == '<' && op[1] == '\0') {
		return a < b;
	}
	if (op[0] == '=' && op[1] == '=') {
		return a == b;
	}
	if (op[0] == '!' && op[1] == '=') {
		return a != b;
	}

	return false;
}

bool ExpressionEvaluator::applyLogicalOp(bool a, bool b, const char* op) {
	if (op == 0) {
		return false;
	}

	if (op[0] == 'A' || op[0] == 'a') {
		return a && b;
	}
	if (op[0] == 'O' || op[0] == 'o') {
		return a || b;
	}

	return false;
}
