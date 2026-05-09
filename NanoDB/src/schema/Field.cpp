#include "Field.h"
#include <cstdio>
#include <cstdlib>

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

static void stringCopy(char* dest, const char* src, int maxLen) {
	if (dest == 0 || maxLen <= 0) {
		return;
	}

	if (src == 0) {
		dest[0] = '\0';
		return;
	}

	int i = 0;
	for (; i < maxLen - 1 && src[i] != '\0'; ++i) {
		dest[i] = src[i];
	}
	dest[i] = '\0';
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

static bool isType(const Field& field, const char* typeName) {
	return compareStrings(field.getType(), typeName) == 0;
}

IntField::IntField(int v) {
	value = v;
	buffer[0] = '\0';
}

bool IntField::operator>(const Field& other) const {
	return toDouble() > other.toDouble();
}

bool IntField::operator<(const Field& other) const {
	return toDouble() < other.toDouble();
}

bool IntField::operator==(const Field& other) const {
	return toDouble() == other.toDouble();
}

bool IntField::operator!=(const Field& other) const {
	return !(*this == other);
}

bool IntField::operator>=(const Field& other) const {
	return !(*this < other);
}

bool IntField::operator<=(const Field& other) const {
	return !(*this > other);
}

Field* IntField::clone() const {
	return new IntField(value);
}

void IntField::print() const {
	std::printf("%d", value);
}

const char* IntField::toString() const {
	std::snprintf(buffer, sizeof(buffer), "%d", value);
	return buffer;
}

FloatField::FloatField(float v) {
	value = v;
	buffer[0] = '\0';
}

bool FloatField::operator>(const Field& other) const {
	return toDouble() > other.toDouble();
}

bool FloatField::operator<(const Field& other) const {
	return toDouble() < other.toDouble();
}

bool FloatField::operator==(const Field& other) const {
	return toDouble() == other.toDouble();
}

bool FloatField::operator!=(const Field& other) const {
	return !(*this == other);
}

bool FloatField::operator>=(const Field& other) const {
	return !(*this < other);
}

bool FloatField::operator<=(const Field& other) const {
	return !(*this > other);
}

Field* FloatField::clone() const {
	return new FloatField(value);
}

void FloatField::print() const {
	std::printf("%f", value);
}

const char* FloatField::toString() const {
	std::snprintf(buffer, sizeof(buffer), "%.6f", value);
	return buffer;
}

StringField::StringField(const char* v) {
	if (v == 0) {
		length = 0;
		value = new char[1];
		value[0] = '\0';
		return;
	}

	length = stringLength(v);
	value = new char[length + 1];
	stringCopy(value, v, length + 1);
}

StringField::~StringField() {
	delete[] value;
	value = 0;
	length = 0;
}

bool StringField::operator>(const Field& other) const {
	if (isType(other, "STRING")) {
		return compareStrings(value, other.toString()) > 0;
	}
	return toDouble() > other.toDouble();
}

bool StringField::operator<(const Field& other) const {
	if (isType(other, "STRING")) {
		return compareStrings(value, other.toString()) < 0;
	}
	return toDouble() < other.toDouble();
}

bool StringField::operator==(const Field& other) const {
	if (isType(other, "STRING")) {
		return compareStrings(value, other.toString()) == 0;
	}
	return toDouble() == other.toDouble();
}

bool StringField::operator!=(const Field& other) const {
	return !(*this == other);
}

bool StringField::operator>=(const Field& other) const {
	return !(*this < other);
}

bool StringField::operator<=(const Field& other) const {
	return !(*this > other);
}

Field* StringField::clone() const {
	return new StringField(value);
}

void StringField::print() const {
	std::printf("%s", value);
}

double StringField::toDouble() const {
	if (value == 0 || value[0] == '\0') {
		return 0.0;
	}

	char* endPtr = 0;
	double val = std::strtod(value, &endPtr);
	if (endPtr == value) {
		return 0.0;
	}
	return val;
}

const char* StringField::toString() const {
	return value;
}
