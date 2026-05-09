#pragma once

#include "Stack.h"
#include "Tokenizer.h"

class PostfixConverter {
	Stack<Token>* operatorStack;
	Token* output;
	int outputCount;

	int getPrecedence(const Token& t);
	bool isOperatorToken(const Token& t);
	void appendTokenString(char* buffer, int bufferSize, const char* tokenValue);
	void buildExpressionString(Token* tokens, int count, char* buffer, int bufferSize);

public:
	PostfixConverter();
	~PostfixConverter();
	Token* convert(Token* tokens, int count, int& outCount);
	void printPostfix(Token* postfix, int count);
};
