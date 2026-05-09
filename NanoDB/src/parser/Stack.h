#pragma once

template<typename T>
class Stack {
	T* data;
	int top;
	int capacity;

	void resize(int newCapacity);

public:
	Stack(int cap = 256);
	~Stack();
	void push(const T& val);
	T pop();
	T peek() const;
	bool isEmpty() const;
	int size() const;
};

template<typename T>
Stack<T>::Stack(int cap) {
	capacity = cap > 0 ? cap : 1;
	data = new T[capacity];
	top = -1;
}

template<typename T>
Stack<T>::~Stack() {
	delete[] data;
	data = 0;
	top = -1;
	capacity = 0;
}

template<typename T>
void Stack<T>::resize(int newCapacity) {
	if (newCapacity <= capacity) {
		return;
	}

	T* newData = new T[newCapacity];
	for (int i = 0; i <= top; ++i) {
		newData[i] = data[i];
	}

	delete[] data;
	data = newData;
	capacity = newCapacity;
}

template<typename T>
void Stack<T>::push(const T& val) {
	if (top + 1 >= capacity) {
		resize(capacity * 2);
	}

	top += 1;
	data[top] = val;
}

template<typename T>
T Stack<T>::pop() {
	if (top < 0) {
		return T();
	}

	T value = data[top];
	top -= 1;
	return value;
}

template<typename T>
T Stack<T>::peek() const {
	if (top < 0) {
		return T();
	}

	return data[top];
}

template<typename T>
bool Stack<T>::isEmpty() const {
	return top < 0;
}

template<typename T>
int Stack<T>::size() const {
	return top + 1;
}
