#include "Row.h"
#include <cstdio>

Row::Row(int maxFieldsValue) {
	maxFields = maxFieldsValue;
	fieldCount = 0;
	fields = new Field*[maxFields];

	for (int i = 0; i < maxFields; ++i) {
		fields[i] = 0;
	}
}

Row::~Row() {
	for (int i = 0; i < fieldCount; ++i) {
		delete fields[i];
		fields[i] = 0;
	}

	delete[] fields;
	fields = 0;
	fieldCount = 0;
	maxFields = 0;
}

void Row::addField(Field* f) {
	if (f == 0) {
		return;
	}

	if (fieldCount >= maxFields) {
		delete f;
		return;
	}

	fields[fieldCount] = f;
	fieldCount += 1;
}

void Row::setField(int index, Field* f) {
	if (f == 0) {
		return;
	}

	if (index < 0 || index >= fieldCount) {
		delete f;
		return;
	}

	delete fields[index];
	fields[index] = f;
}

Field* Row::getField(int index) const {
	if (index < 0 || index >= fieldCount) {
		return 0;
	}
	return fields[index];
}

int Row::getFieldCount() const {
	return fieldCount;
}

void Row::print() const {
	for (int i = 0; i < fieldCount; ++i) {
		if (fields[i] != 0) {
			fields[i]->print();
		}
		if (i < fieldCount - 1) {
			std::printf(" | ");
		}
	}
	std::printf("\n");
}

Row* Row::clone() const {
	Row* copy = new Row(maxFields);
	for (int i = 0; i < fieldCount; ++i) {
		if (fields[i] != 0) {
			copy->addField(fields[i]->clone());
		}
	}
	return copy;
}
