#pragma once

#include <fstream>
#include <filesystem>

class File
{
public:
	static std::vector<char> ReadFile(std::string path)
	{
		std::ifstream file = std::ifstream(path, std::ios_base::binary | std::ios_base::ate);
		std::vector<char> file_data(file.tellg(), 0);
		file.seekg(0);
		file.read(file_data.data(), file_data.size());
		return file_data;
	}

	static void AppendToFile(std::string path, std::vector<char>& data)
	{
		std::ofstream output_file(path, std::ios_base::binary | std::ios_base::app);
		output_file.write(data.data(), data.size());
		output_file.close();
	}

	static void WriteToFile(std::string path, std::string& text)
	{
		std::ofstream output_file(path, std::ios_base::binary);
		output_file.write(text.c_str(), text.length());
		output_file.close();
	}

	static void WriteToFile(std::string path, char* text)
	{
		std::ofstream output_file(path, std::ios_base::binary);
		output_file.write(text, std::strlen(text));
		output_file.close();
	}

	static void WriteToFile(std::string path, std::vector<char>& data)
	{
		std::ofstream output_file(path, std::ios_base::binary);
		output_file.write(data.data(), data.size());
		output_file.close();
	}

	static void WriteToFile(std::string path, char* data, size_t data_size)
	{
		std::ofstream output_file(path, std::ios_base::binary);
		output_file.write(data, data_size);
		output_file.close();
	}

	static bool Exists(std::string path)
	{
		return std::filesystem::exists(path);
	}
};