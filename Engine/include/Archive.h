#ifndef OOT_ARCHIVE_F
#define OOT_ARCHIVE_F
#include <filesystem>
#include <fstream>
#include <json.hpp>

struct ArchiveStrategy
{
public:
	virtual ~ArchiveStrategy() = default;
	virtual void OpenFile(std::fstream& file,const std::filesystem::path& path) = 0;
	virtual void ClearFile(std::fstream& file,const std::filesystem::path& path) = 0;
	virtual void CloseFile(std::fstream& file,const std::filesystem::path& path) = 0;
protected:

};
struct BinaryArchive : public  ArchiveStrategy
{
	void OpenFile(std::fstream& file,const std::filesystem::path& path) override
	{
		assert(path.extension() == ".txt", "extension not supported");
		file.open(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::ate);
		file.seekg(0, std::ios::beg);
		if (!file.is_open())
		{
			file.clear();
			std::ofstream createFile(path, std::ios::binary);
			createFile.close();
			file.open(path, std::ios::in | std::ios::out | std::ios::binary | std::ios::ate);
		}
		assert(file.is_open(), "file douldn't open");
	}
	void ClearFile(std::fstream& file,const std::filesystem::path& path) override
	{
		file.close();
		file.open(path, std::ios::in | std::ios::out |std::ios::binary  | std::ios::trunc);
	}
	void CloseFile(std::fstream& file,const std::filesystem::path& path) override
	{
		file.flush();
		file.close();
	}
	static void Read(std::fstream& file,char* data, std::streamsize size)
	{
		assert(file.is_open(), "file not open");
		file.read(data, size);
	}

	template<typename T> requires(std::is_arithmetic_v<T>)
		static	void Read(std::fstream& file,T* data)
	{
		BinaryArchive::Read(file,reinterpret_cast<char*>(data), sizeof(T));
	}

	static void Write(std::fstream& file,const char* data, std::streamsize size)
	{
		assert(file.is_open(), "file not open");
		file.write(data, size);
	}
	template<typename T> requires(std::is_arithmetic_v<T>)
		static	void Write(std::fstream& file,const T* data)
	{
		BinaryArchive::Write(file,reinterpret_cast<const char*>(data), sizeof(T));
	}
};
struct JsonArchive : ArchiveStrategy
{
public:
	void OpenFile(std::fstream& file,const std::filesystem::path& path) override
	{
		assert(path.extension() == ".json", "extension not supported");
		file.open(path, std::ios::in | std::ios::out | std::ios::ate);
		file.seekg(0, std::ios::beg);
		if (!file.is_open())
		{
			file.clear();
			std::ofstream createFile(path);
			createFile.close();
			file.open(path, std::ios::in | std::ios::out | std::ios::ate);
		}
		assert(file.is_open(), "file douldn't open");
		if (file.peek() == std::fstream::traits_type::eof())
			m_data = nlohmann::json::array();
		else
			file >> m_data;
		m_index = 0;
	}
	void ClearFile(std::fstream& file,const std::filesystem::path& path) override
	{
		file.close();
		file.open(path, std::ios::in | std::ios::out | std::ios::trunc);
		m_data = nlohmann::json::array();
	}
	void CloseFile(std::fstream& file,const std::filesystem::path& path) override
	{
		file.close();
		file.open(path, std::ios::in | std::ios::out | std::ios::trunc);
		file << m_data.dump(4);
		file.flush();
		file.close();
	}
	void Read(char* data, std::streamsize size)
	{
		const std::string value = m_data.at(m_index++).get<std::string>();
		assert(value.size() <= static_cast<std::size_t>(size), "buffer to small");
		std::memcpy(data, value.data(), value.size());
	}

	template<typename T> requires(std::is_arithmetic_v<T>)
		void Read(T* data)
	{
		*data = m_data.at(m_index++).get<T>();
	}

	void Write(const char* data, std::streamsize size)
	{
		m_data.push_back(std::string(data, size));
	}

	template<typename T> requires(std::is_arithmetic_v<T>)
	void Write(const T* data)
	{
		m_data.push_back(*data);
	}
		
private:
	nlohmann::json m_data = nlohmann::json::array();
	std::size_t m_index = 0;
};


enum class FileType
{
	Json,
	Binary
};
struct Archive
{
	void OpenFile(const std::filesystem::path& path)
	{
		assert(!m_file.is_open(), "file already open");

		if (path.extension() == ".txt")
		{
			m_strategy = &m_binary;
			m_type = FileType::Binary;
		}
		else
		{
			m_strategy = &m_json;
			m_type = FileType::Json;

		}

		m_strPath = path;
		m_strategy->OpenFile(m_file, m_strPath);
	}
	void ClearFile()
	{
		assert(m_file.is_open(), "file not open");
		m_strategy->ClearFile(m_file, m_strPath);
	}
	void CloseFile()
	{
		assert(m_file.is_open(), "file not open");
		m_strategy->CloseFile(m_file, m_strPath);
	}
	
	void Read(char* data, std::streamsize size)
	{
		assert(m_file.is_open(), "file not open");
		if (m_type == FileType::Binary)
			BinaryArchive::Read(m_file, data, size);
		else
			static_cast<JsonArchive*>(m_strategy)->Read(data, size);
	}

	template<typename T> requires(std::is_arithmetic_v<T>)
	void Read(T* data)
	{
		assert(m_file.is_open(), "file not open");
		if (m_type == FileType::Binary)
			BinaryArchive::Read<T>(m_file, data);
		else
			static_cast<JsonArchive*>(m_strategy)->Read<T>(data);
	}

	void Write(const char* data, std::streamsize size)
	{
		assert(m_file.is_open(), "file not open");
		if (m_type == FileType::Binary)
			BinaryArchive::Write(m_file, data, size);
		else
			static_cast<JsonArchive*>(m_strategy)->Write(data, size);
	}
	template<typename T> requires(std::is_arithmetic_v<T>)
	void Write(const T* data)
	{
		assert(m_file.is_open(), "file not open");
		if (m_type == FileType::Binary)
			BinaryArchive::Write<T>(m_file, data);
		else
			static_cast<JsonArchive*>(m_strategy)->Write<T>(data);
	}
private:
	ArchiveStrategy* m_strategy = nullptr;
	BinaryArchive m_binary;
	JsonArchive m_json;
	FileType m_type = FileType::Binary;
	std::fstream m_file;
	std::filesystem::path m_strPath;
};
	

#endif
