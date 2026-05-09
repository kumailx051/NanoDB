#pragma once

enum TokenType {
	TOKEN_IDENTIFIER,
	TOKEN_NUMBER,
	TOKEN_STRING,
	TOKEN_OPERATOR,
	TOKEN_LOGICAL,
	TOKEN_MATH,
	TOKEN_LPAREN,
	TOKEN_RPAREN,
	TOKEN_KEYWORD,
	TOKEN_END
};

struct Token {
	TokenType type;
	char value[128];
};

class Tokenizer {
	char* input;
	int pos;
	Token* tokens;
	int tokenCount;
	int maxTokens;

	void addToken(TokenType type, const char* value);
	void copyValue(char* dest, int destSize, const char* src);

public:
	Tokenizer(const char* query);
	~Tokenizer();
	void tokenize();
	Token* getTokens();
	int getTokenCount();
};
