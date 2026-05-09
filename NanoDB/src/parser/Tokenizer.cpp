#include "Tokenizer.h"

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

static bool isWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool isAlpha(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool isDigit(char c) {
	return c >= '0' && c <= '9';
}

static char toUpperChar(char c) {
	if (c >= 'a' && c <= 'z') {
		return static_cast<char>(c - ('a' - 'A'));
	}
	return c;
}

Tokenizer::Tokenizer(const char* query) {
	maxTokens = 512;
	tokens = new Token[maxTokens];
	tokenCount = 0;
	pos = 0;

	if (query == 0) {
		input = 0;
		return;
	}

	int len = stringLength(query);
	input = new char[len + 1];
	copyValue(input, len + 1, query);
}

Tokenizer::~Tokenizer() {
	delete[] input;
	input = 0;

	delete[] tokens;
	tokens = 0;
	tokenCount = 0;
	maxTokens = 0;
}

void Tokenizer::copyValue(char* dest, int destSize, const char* src) {
	if (dest == 0 || destSize <= 0) {
		return;
	}

	if (src == 0) {
		dest[0] = '\0';
		return;
	}

	int i = 0;
	for (; i < destSize - 1 && src[i] != '\0'; ++i) {
		dest[i] = src[i];
	}
	dest[i] = '\0';
}

void Tokenizer::addToken(TokenType type, const char* value) {
	if (tokenCount >= maxTokens) {
		return;
	}

	tokens[tokenCount].type = type;
	copyValue(tokens[tokenCount].value, static_cast<int>(sizeof(tokens[tokenCount].value)), value);
	tokenCount += 1;
}

void Tokenizer::tokenize() {
	tokenCount = 0;
	pos = 0;

	if (input == 0) {
		addToken(TOKEN_END, "");
		return;
	}

	while (input[pos] != '\0') {
		char c = input[pos];

		if (isWhitespace(c)) {
			pos += 1;
			continue;
		}

		if (isAlpha(c) || c == '_') {
			char buffer[128];
			int len = 0;

			while (input[pos] != '\0' && (isAlpha(input[pos]) || isDigit(input[pos]) || input[pos] == '_')) {
				if (len < 127) {
					buffer[len] = input[pos];
					len += 1;
				}
				pos += 1;
			}
			buffer[len] = '\0';

			char upper[128];
			for (int i = 0; i <= len; ++i) {
				upper[i] = toUpperChar(buffer[i]);
			}

			if (compareStrings(upper, "AND") == 0 || compareStrings(upper, "OR") == 0) {
				addToken(TOKEN_LOGICAL, upper);
			} else if (compareStrings(upper, "SELECT") == 0 ||
					   compareStrings(upper, "WHERE") == 0 ||
					   compareStrings(upper, "INSERT") == 0 ||
					   compareStrings(upper, "JOIN") == 0 ||
					   compareStrings(upper, "FROM") == 0 ||
					   compareStrings(upper, "INTO") == 0 ||
					   compareStrings(upper, "UPDATE") == 0 ||
					   compareStrings(upper, "SET") == 0 ||
					   compareStrings(upper, "VALUES") == 0 ||
					   compareStrings(upper, "ON") == 0) {
				addToken(TOKEN_KEYWORD, upper);
			} else {
				addToken(TOKEN_IDENTIFIER, buffer);
			}
			continue;
		}

		if (isDigit(c) || (c == '.' && isDigit(input[pos + 1]))) {
			char buffer[128];
			int len = 0;
			bool hasDot = false;

			while (input[pos] != '\0') {
				char current = input[pos];
				if (isDigit(current)) {
					if (len < 127) {
						buffer[len] = current;
						len += 1;
					}
					pos += 1;
				} else if (current == '.' && !hasDot) {
					hasDot = true;
					if (len < 127) {
						buffer[len] = current;
						len += 1;
					}
					pos += 1;
				} else {
					break;
				}
			}
			buffer[len] = '\0';
			addToken(TOKEN_NUMBER, buffer);
			continue;
		}

		if (c == '"') {
			pos += 1;
			char buffer[128];
			int len = 0;

			while (input[pos] != '\0' && input[pos] != '"') {
				if (len < 127) {
					buffer[len] = input[pos];
					len += 1;
				}
				pos += 1;
			}
			buffer[len] = '\0';
			if (input[pos] == '"') {
				pos += 1;
			}
			addToken(TOKEN_STRING, buffer);
			continue;
		}

		if (c == '(') {
			addToken(TOKEN_LPAREN, "(");
			pos += 1;
			continue;
		}
		if (c == ')') {
			addToken(TOKEN_RPAREN, ")");
			pos += 1;
			continue;
		}

		if (c == '>' || c == '<' || c == '=' || c == '!') {
			char buffer[3];
			buffer[0] = c;
			buffer[1] = '\0';
			buffer[2] = '\0';

			if (input[pos + 1] == '=') {
				buffer[1] = '=';
				buffer[2] = '\0';
				pos += 2;
			} else {
				pos += 1;
			}

			if (buffer[0] == '=' && buffer[1] == '\0') {
				buffer[0] = '=';
				buffer[1] = '=';
				buffer[2] = '\0';
			}

			if (buffer[0] == '!' && buffer[1] == '\0') {
				continue;
			}

			addToken(TOKEN_OPERATOR, buffer);
			continue;
		}

		if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
			char buffer[2];
			buffer[0] = c;
			buffer[1] = '\0';
			addToken(TOKEN_MATH, buffer);
			pos += 1;
			continue;
		}

		pos += 1;
	}

	addToken(TOKEN_END, "");
}

Token* Tokenizer::getTokens() {
	return tokens;
}

int Tokenizer::getTokenCount() {
	return tokenCount;
}
