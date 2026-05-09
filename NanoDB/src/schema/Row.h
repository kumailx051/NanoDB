#pragma once

#include "Field.h"

class Row {
	Field** fields;
	int fieldCount;
	int maxFields;

public:
	Row(int maxFields);
	~Row();
	void addField(Field* f);
	void setField(int index, Field* f);
	Field* getField(int index) const;
	int getFieldCount() const;
	void print() const;
	Row* clone() const;
};
