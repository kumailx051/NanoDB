#include "PostfixConverter.h"
#include "util/Logger.h"
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

static int stringLength(const char* text) {
	if (text == 0) {
		return 0;
	}

	int len = 0;
	while (text[len] != '\0') {
		len += 1;
	}
	return len;
}

PostfixConverter::PostfixConverter() {
	operatorStack = new Stack<Token>(256);
	output = 0;
	outputCount = 0;
}

PostfixConverter::~PostfixConverter() {
	delete operatorStack;
	operatorStack = 0;

	delete[] output;
	output = 0;
	outputCount = 0;
}

bool PostfixConverter::isOperatorToken(const Token& t) {
	return t.type == TOKEN_OPERATOR || t.type == TOKEN_LOGICAL || t.type == TOKEN_MATH;
}

int PostfixConverter::getPrecedence(const Token& t) {
	if (t.type == TOKEN_MATH) {
		if (compareStrings(t.value, "*") == 0 || compareStrings(t.value, "/") == 0 || compareStrings(t.value, "%") == 0) {
			return 3;
		}
		if (compareStrings(t.value, "+") == 0 || compareStrings(t.value, "-") == 0) {
			return 2;
		}
	}

	if (t.type == TOKEN_OPERATOR) {
		return 1;
	}

	if (t.type == TOKEN_LOGICAL) {
		if (compareStrings(t.value, "AND") == 0) {
			return 0;
		}
		if (compareStrings(t.value, "OR") == 0) {
			return -1;
		}
	}

	return -2;
}

void PostfixConverter::appendTokenString(char* buffer, int bufferSize, const char* tokenValue) {
	if (buffer == 0 || bufferSize <= 0 || tokenValue == 0) {
		return;
	}

	int currentLen = stringLength(buffer);
	if (currentLen >= bufferSize - 1) {
		return;
	}

	if (currentLen > 0 && currentLen < bufferSize - 1) {
		buffer[currentLen] = ' ';
		buffer[currentLen + 1] = '\0';
		currentLen += 1;
	}

	for (int i = 0; tokenValue[i] != '\0' && currentLen < bufferSize - 1; ++i) {
		buffer[currentLen] = tokenValue[i];
		currentLen += 1;
	}
	buffer[currentLen] = '\0';
}

void PostfixConverter::buildExpressionString(Token* tokens, int count, char* buffer, int bufferSize) {
	if (buffer == 0 || bufferSize <= 0) {
		return;
	}

	buffer[0] = '\0';
	for (int i = 0; i < count; ++i) {
		if (tokens[i].type == TOKEN_END) {
			break;
		}
		if (tokens[i].type == TOKEN_KEYWORD) {
			continue;
		}
		appendTokenString(buffer, bufferSize, tokens[i].value);
	}
}

Token* PostfixConverter::convert(Token* tokens, int count, int& outCount) {
	if (output != 0) {
		delete[] output;
	}

	int capacity = count > 0 ? count : 1;
	output = new Token[capacity];
	outputCount = 0;

	for (int i = 0; i < count; ++i) {
		Token current = tokens[i];
		if (current.type == TOKEN_END) {
			break;
		}

		if (current.type == TOKEN_IDENTIFIER || current.type == TOKEN_NUMBER || current.type == TOKEN_STRING) {
			output[outputCount] = current;
			outputCount += 1;
		} else if (current.type == TOKEN_LPAREN) {
			operatorStack->push(current);
		} else if (current.type == TOKEN_RPAREN) {
			while (!operatorStack->isEmpty()) {
				Token top = operatorStack->peek();
				if (top.type == TOKEN_LPAREN) {
					operatorStack->pop();
					break;
				}
				output[outputCount] = operatorStack->pop();
				outputCount += 1;
			}
		} else if (isOperatorToken(current)) {
			while (!operatorStack->isEmpty()) {
				Token top = operatorStack->peek();
				if (!isOperatorToken(top)) {
					break;
				}
				int topPrec = getPrecedence(top);
				int currentPrec = getPrecedence(current);
				if (topPrec >= currentPrec) {
					output[outputCount] = operatorStack->pop();
					outputCount += 1;
				} else {
					break;
				}
			}
			operatorStack->push(current);
		}
	}

	while (!operatorStack->isEmpty()) {
		Token top = operatorStack->pop();
		if (top.type == TOKEN_LPAREN || top.type == TOKEN_RPAREN) {
			continue;
		}
		output[outputCount] = top;
		outputCount += 1;
	}

	outCount = outputCount;

	char infix[1024];
	char postfix[1024];
	buildExpressionString(tokens, count, infix, static_cast<int>(sizeof(infix)));
	buildExpressionString(output, outputCount, postfix, static_cast<int>(sizeof(postfix)));
	if (infix[0] != '\0' && postfix[0] != '\0') {
		Logger::logf("[LOG] Infix \"%s\" converted to Postfix \"%s\"", infix, postfix);
	}

	return output;
}

void PostfixConverter::printPostfix(Token* postfix, int count) {
	for (int i = 0; i < count; ++i) {
		if (postfix[i].type == TOKEN_END) {
			break;
		}
		std::printf("%s", postfix[i].value);
		if (i < count - 1) {
			std::printf(" ");
		}
	}
	std::printf("\n");
}
