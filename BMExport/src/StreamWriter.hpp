#pragma once

#include "File.hpp"
#include <vector>
#include <string>
#include <fstream>

class StreamWriter
{
public:
	StreamWriter() { }

	template <typename T>
	void Write(T* data)
	{
		stream.resize(stream.size() + sizeof(T));
		std::memcpy(reinterpret_cast<T*>(stream.data() + position), data, sizeof(T));
		position += sizeof(T);
	}

	template <typename T>
	void Write(T* data, size_t count)
	{
		if (!count)
			return;
		stream.resize(stream.size() + sizeof(T) * count);
		std::memcpy(reinterpret_cast<T*>(stream.data() + position), data, sizeof(T) * count);
		position += sizeof(T) * count;
	}

	template <typename T>
	void Write(const char* data, size_t length)
	{
		if (!length)
			return;
		stream.resize(stream.size() + length);
		std::memcpy(&stream[position], data, length);
		position += length;
	}

	template <typename T>
	void WriteAt(T* data, size_t offset)
	{
		std::memcpy(reinterpret_cast<T*>(stream.data() + offset), data, sizeof(T));
	}

	template <typename T>
	T ReadAt(size_t offset)
	{
		T data;
		std::memcpy(&data, reinterpret_cast<T*>(stream.data() + offset), sizeof(T));		
		return data;
	}

	size_t Size() { return stream.size(); }

	size_t Position() { return position; }

	std::vector<char> Release() { return std::move(stream); }

	void WriteToFile(std::string path) { File::WriteToFile(path, stream); }

	void AppendToFile(std::string path) { File::AppendToFile(path, stream); }

private:
	std::vector<char> stream;
	size_t position = 0;
};