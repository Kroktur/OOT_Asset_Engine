#ifndef OOT_SERIALIZE_F
#define OOT_SERIALIZE_F

#include <map>
#include <type_traits>
#include <unordered_map>
#include "Archive.h"

namespace OOT
{
	template<typename T>
	struct Serialize;
}

template<typename T> requires(std::is_arithmetic_v<T>)
struct OOT::Serialize<T>
{
	using value_type = T;
	static void Write(Archive& archive, value_type value)
	{
		archive.Write<value_type>(&value);
	}
	static value_type Read(Archive& archive)
	{
		value_type value{};
		archive.Read<value_type>(&value);
		return value;
	}
};


template<>
struct OOT::Serialize<std::string>
{
	using value_type = std::string;
	static void Write(Archive& archive, const value_type& value)
	{
		Serialize<std::size_t>::Write(archive, value.size());
		archive.Write(value.c_str(), value.size());
	}
	static value_type Read(Archive& archive)
	{
		std::size_t size = Serialize<std::size_t>::Read(archive);
		value_type value;
		value.resize(size);
		archive.Read(value.data(), static_cast<std::streamsize>(size));
		return value;
	}
};

template<typename T, typename Alloc>
struct OOT::Serialize<std::vector<T, Alloc>>
{
	using val_type = T;
	using value_type = std::vector<T, Alloc>;
	static void Write(Archive& archive, const value_type& vector)
	{
		OOT::Serialize<std::size_t>::Write(archive, vector.size());
		for (auto& e : vector)
			OOT::Serialize <val_type>::Write(archive, e);
	}
	static value_type Read(Archive& archive)
	{
		value_type result;
		result.resize(OOT::Serialize<std::size_t>::Read(archive));
		for (int i = 0; i < result.size(); ++i)
		{
			result[i] = OOT::Serialize<val_type>::Read(archive);
		}
		return std::move(result);
	}
};
template<typename T, std::size_t N>
struct OOT::Serialize<std::array<T, N>>
{
	using val_type = T;
	using value_type = std::array<T, N>;
	static void Write(Archive& archive, const value_type& array)
	{
		for (auto& e : array)
			OOT::Serialize <val_type>::Write(archive, e);
	}
	static value_type Read(Archive& archive)
	{
		value_type result;
		for (int i = 0; i < result.size(); ++i)
		{
			result[i] = OOT::Serialize<val_type>::Read(archive);
		}
		return std::move(result);
	}
};

template<typename T, typename T2>
struct OOT::Serialize<std::pair<T, T2>>
{
	using val_type = T;
	using val_type_2 = T2;
	using value_type = std::pair<T, T2>;
	static void Write(Archive& archive, const value_type& pair)
	{
		OOT::Serialize <val_type>::Write(archive, pair.first);
		OOT::Serialize <val_type_2>::Write(archive, pair.second);
	}
	static value_type Read(Archive& archive)
	{
		value_type result;
		result.first = OOT::Serialize <val_type>::Read(archive);
		result.second = OOT::Serialize <val_type_2>::Read(archive);
		return std::move(result);
	}
};

template<typename KeyT, typename ValT, typename CompareT, typename Alloc>
struct OOT::Serialize<std::map<KeyT, ValT, CompareT, Alloc>>
{
	using val_type = std::pair<KeyT, ValT>;
	using value_type = std::map<KeyT, ValT, CompareT, Alloc>;
	static void Write(Archive& archive, const value_type& map)
	{
		OOT::Serialize<std::size_t>::Write(archive, map.size());
		for (const auto& [key, value] : map)
			Serialize<val_type>::Write(archive, { key, value });
	}
	static value_type Read(Archive& archive)
	{
		value_type result;
		std::size_t size = OOT::Serialize<std::size_t>::Read(archive);
		for (int i = 0; i < size; ++i)
		{
			val_type pair = OOT::Serialize<val_type>::Read(archive);
			result.emplace(pair.first, pair.second);
		}
		return std::move(result);
	}
};

template<typename KeyT, typename ValT, typename CompareT, typename hash, typename Alloc>
struct OOT::Serialize<std::unordered_map<KeyT, ValT, hash, CompareT, Alloc>>
{
	using val_type = std::pair<KeyT, ValT>;
	using value_type = std::unordered_map < KeyT, ValT, hash, CompareT, Alloc >;
	static void Write(Archive& archive, const value_type& map)
	{
		OOT::Serialize<std::size_t>::Write(archive, map.size());
		for (const auto& [key, value] : map)
			Serialize<val_type>::Write(archive, { key, value });
	}
	static value_type Read(Archive& archive)
	{
		value_type result;
		std::size_t size = OOT::Serialize<std::size_t>::Read(archive);
		for (int i = 0; i < size; ++i)
		{
			val_type pair = OOT::Serialize<val_type>::Read(archive);
			result.emplace(pair.first, pair.second);
		}
		return std::move(result);
	}
};
#endif
