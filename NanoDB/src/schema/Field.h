#pragma once

class Field {
public:
	virtual ~Field() {}
	virtual bool operator>(const Field& other) const = 0;
	virtual bool operator<(const Field& other) const = 0;
	virtual bool operator==(const Field& other) const = 0;
	virtual bool operator!=(const Field& other) const = 0;
	virtual bool operator>=(const Field& other) const = 0;
	virtual bool operator<=(const Field& other) const = 0;
	virtual Field* clone() const = 0;
	virtual void print() const = 0;
	virtual const char* getType() const = 0;
	virtual double toDouble() const = 0;
	virtual const char* toString() const = 0;
};

class IntField : public Field {
	int value;
	mutable char buffer[32];

public:
	IntField(int v);
	bool operator>(const Field& other) const override;
	bool operator<(const Field& other) const override;
	bool operator==(const Field& other) const override;
	bool operator!=(const Field& other) const override;
	bool operator>=(const Field& other) const override;
	bool operator<=(const Field& other) const override;
	Field* clone() const override;
	void print() const override;
	const char* getType() const override { return "INT"; }
	double toDouble() const override { return static_cast<double>(value); }
	int getValue() const { return value; }
	const char* toString() const override;
};

class FloatField : public Field {
	float value;
	mutable char buffer[64];

public:
	FloatField(float v);
	bool operator>(const Field& other) const override;
	bool operator<(const Field& other) const override;
	bool operator==(const Field& other) const override;
	bool operator!=(const Field& other) const override;
	bool operator>=(const Field& other) const override;
	bool operator<=(const Field& other) const override;
	Field* clone() const override;
	void print() const override;
	const char* getType() const override { return "FLOAT"; }
	double toDouble() const override { return static_cast<double>(value); }
	float getValue() const { return value; }
	const char* toString() const override;
};

class StringField : public Field {
	char* value;
	int length;

public:
	StringField(const char* v);
	~StringField();
	bool operator>(const Field& other) const override;
	bool operator<(const Field& other) const override;
	bool operator==(const Field& other) const override;
	bool operator!=(const Field& other) const override;
	bool operator>=(const Field& other) const override;
	bool operator<=(const Field& other) const override;
	Field* clone() const override;
	void print() const override;
	const char* getType() const override { return "STRING"; }
	double toDouble() const override;
	const char* toString() const override;
};
