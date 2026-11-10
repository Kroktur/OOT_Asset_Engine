#ifndef KTR_FLATHASHMAP_F
#define KTR_FLATHASHMAP_F

//TODO implement in a header
#include <bit>
#include <vector>
#include <stdexcept>
#include <assert.h>

#include "KTR/Allocation/KTR_Allocator.h"
#include "KTR/Hash/KTR_Hasher.h"
namespace KTR::container
{
	namespace flat_map
	{
		template<typename KeyT, typename ValT>
		struct Parameters
		{
			using key_type = KeyT;
			using value_type = ValT;

			key_type key = key_type{};
			value_type value = value_type{};
		};


		template<typename KeyT, typename ValT, bool IsConst>
		class Iterator
		{
		public:
			using key_type = KeyT;
			using val_type = ValT;
			using value_type = flat_map::Parameters<key_type, val_type>;

			using meta_type = std::uint8_t;
			static constexpr meta_type EMPTY = 0x80; //     1000 0000
			static constexpr meta_type DELETED = 0xFE; //   1111 1110

			using iterator_category = std::random_access_iterator_tag;

			using difference_type = std::ptrdiff_t;
			using meta_pointer_type = const meta_type*;
			using reference = std::conditional_t<IsConst, const value_type&, value_type&>;
			using pointer = std::conditional_t<IsConst, const value_type*, value_type*>;

		public:
			Iterator(pointer current, meta_pointer_type currentMeta, std::size_t size, pointer end) : m_current(current), m_currentMeta(currentMeta), m_size(size), m_end(end)
			{
				if (m_currentMeta != nullptr && (*m_currentMeta == EMPTY || *m_currentMeta == DELETED))
					this->PrivPlus();
			}
			Iterator(const Iterator&) = default;
			Iterator(Iterator&&) noexcept = default;
			~Iterator() = default;
			Iterator& operator=(const Iterator&) = default;
			Iterator& operator=(Iterator&&) noexcept = default;
		public:
			[[nodiscard]] reference operator*() const
			{
				return *m_current;
			}
			[[nodiscard]] pointer   operator->() const
			{
				return m_current;
			}
			Iterator& operator++()
			{
				this->PrivPlus();
				return *this;
			}
			[[nodiscard]] Iterator  operator++(int)
			{
				Iterator tmp = *this;
				this->PrivPlus();
				return tmp;
			}
			Iterator& operator--()
			{
				this->PrivMinus();
				return *this;
			}
			[[nodiscard]] Iterator  operator--(int)
			{
				Iterator tmp = *this;
				this->PrivMinus();
				return tmp;
			}
			Iterator& operator-=(std::size_t value)
			{
				for (std::size_t i = 0; i < value; ++i)
					this->PrivMinus();
				return *this;
			}
			[[nodiscard]] Iterator  operator-(std::size_t value)
			{
				Iterator tmp = *this;
				for (std::size_t i = 0; i < value; ++i)
					tmp.PrivMinus();
				return tmp;
			}
			Iterator& operator+=(std::size_t value)
			{
				for (std::size_t i = 0; i < value; ++i)
					this->PrivPlus();
				return *this;
			}
			[[nodiscard]] Iterator  operator+(std::size_t value)
			{
				Iterator tmp = *this;
				for (std::size_t i = 0; i < value; ++i)
					tmp.PrivPlus();
				return tmp;
			}
			[[nodiscard]] difference_type operator-(const Iterator& other) const
			{
				if (m_current == other.m_current)
					return 0;
				
				difference_type size = static_cast<difference_type>(m_current - other.m_current);
				const bool forward = size > 0;
				meta_pointer_type start = forward ? other.m_currentMeta : m_current;
				
				std::size_t loopSize = forward ? static_cast<std::size_t>(size) : static_cast<std::size_t>(-size);
				std::size_t result = 0;
					for (std::size_t i = 0; i < loopSize; ++i)
					{
						meta_pointer_type tmpMeta = start + i;
						if (!(*tmpMeta & EMPTY))
							++result;
					}
				
				return forward ? static_cast<difference_type>(result) : -static_cast<difference_type>(result);
			}
			[[nodiscard]] difference_type PhysicalDistance(const Iterator& other) const
			{
				return  m_current - other.m_current;
			}
			[[nodiscard]] bool operator==(const Iterator& other) const
			{
				return m_current == other.m_current;
			}
			[[nodiscard]] bool operator!=(const Iterator& other) const
			{
				return !(*this == other);
			}
		private:
			void PrivPlus()
			{
				// if end just return
				if (m_current >= m_end)
					return;

				std::size_t size = static_cast<std::size_t>((m_end - m_current - 1));
				// try to find next valid element
				for (std::size_t i = 0; i < size; ++i)
				{
					++m_current;
					++m_currentMeta;
					if (!(*m_currentMeta & EMPTY))
						return;
				}
				// if no valid element found, set to end
				++m_current;
				++m_currentMeta;
			}
			void PrivMinus()
			{

				const pointer begin = m_end - m_size;
				// if begin, just return
				if (m_current <= begin)
					return;
				std::size_t start = static_cast<std::size_t>((m_current - begin));
				// try to find previous valid element
				for (std::size_t i = start; i > 0; --i)
				{
					--m_current;
					--m_currentMeta;
					if (!(*m_currentMeta & EMPTY))
						return;
				}
			}
			pointer m_current;
			meta_pointer_type m_currentMeta;
			std::size_t m_size;
			pointer m_end;
		};

	}

	template<typename KeyT, typename ValT,allocation::ValidRebindAllocator AllocatorT = allocation::default_allocator_type<flat_map::Parameters<KeyT, ValT>>,
	hash::ValidHasher HashT = hash::default_hasher_type<KeyT>> requires(KTR::hash::is_hashable_type_v<KeyT, HashT>)
		struct FlatHashMap
	{
	public:
		using key_type = KeyT;
		using hasher_type = HashT;
		using hash_key_type = hasher_type::result_type;
		using meta_key_type = std::uint8_t;
		using value_type = ValT;
		using Parameters = flat_map::Parameters<key_type, value_type>;
		using allocator_type = AllocatorT;
		using map_allocator = std::allocator_traits<allocator_type>::template rebind_alloc<Parameters>;
		using meta_map_allocator = std::allocator_traits<allocator_type>::template rebind_alloc<meta_key_type>;
		using iterator_type = flat_map::Iterator<key_type, value_type, false>;
		using const_iterator_type = flat_map::Iterator<key_type, value_type, true>;
	public:
		static constexpr meta_key_type EMPTY = 0x80; //     1000 0000
		static constexpr meta_key_type DELETED = 0xFE; //   1111 1110

		static constexpr meta_key_type HASH_MASK = 0x7F; // 0111 1111

		static constexpr std::size_t slotSize = 16;
		static constexpr std::size_t loadfactorNum = 65;
		static constexpr std::size_t loadfactorDen = 100;

		static constexpr std::size_t invalidIndex = std::numeric_limits<std::size_t>::max();
	public:
		FlatHashMap() : m_size(0) {}
		FlatHashMap(const allocator_type allocator) : m_meta(allocator), m_data(allocator), m_size(0){}
		FlatHashMap(std::size_t size, const allocator_type& allocator = allocator_type{}) : m_meta(allocator), m_data(allocator), m_size(0){this->Rehash(size);}
		FlatHashMap(const FlatHashMap&) = default;
		FlatHashMap(FlatHashMap&&) noexcept = default;
		~FlatHashMap() = default;
		FlatHashMap& operator=(const FlatHashMap&) = default;
		FlatHashMap& operator=(FlatHashMap&&) noexcept = default;
	public:
		[[nodiscard]] const_iterator_type begin() const
		{
			return const_iterator_type(m_data.data(), m_meta.data(), m_data.size(), m_data.data() + m_data.size());
		}

		[[nodiscard]] const_iterator_type end() const
		{
			return const_iterator_type(m_data.data() + m_data.size(), m_meta.data() + m_meta.size(), m_data.size(), m_data.data() + m_data.size());
		}

		[[nodiscard]] iterator_type begin()
		{
			return iterator_type(m_data.data(), m_meta.data(), m_data.size(), m_data.data() + m_data.size());
		}

		[[nodiscard]] iterator_type end()
		{
			return iterator_type(m_data.data() + m_data.size(), m_meta.data() + m_meta.size(), m_data.size(), m_data.data() + m_data.size());
		}

		[[nodiscard]] const_iterator_type cbegin() const
		{
			return const_iterator_type(m_data.data(), m_meta.data(), m_data.size(), m_data.data() + m_data.size());
		}

		[[nodiscard]] const_iterator_type cend() const
		{
			return const_iterator_type(m_data.data() + m_data.size(), m_meta.data() + m_meta.size(), m_data.size(), m_data.data() + m_data.size());
		}
		void Clear()
		{
			m_size = 0;
			m_meta.clear();
			m_data.clear();
		}
		[[nodiscard]] std::size_t Capacity() const
		{
			return m_data.capacity();
		}
		[[nodiscard]] std::size_t Size() const
		{
			return m_size;
		}
		[[nodiscard]] std::size_t MaxSize() const
		{
			return m_data.max_size();
		}
		[[nodiscard]] bool Empty() const
		{
			return m_size == 0;
		}
		[[nodiscard]] value_type& operator[](const key_type& key)
		{
			hash_key_type hash = FlatHashMap::HashFn(key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			FindResult result = this->FindOrPredictIndex(key, groupIndex, fp);

			if (result.result != invalidIndex)
				return m_data[result.result].value;

			std::size_t index = this->PrivInsert(Parameters{ key,value_type{} }, result, fp, hash);
			return m_data[index].value;

		}
		[[nodiscard]] value_type& operator[](key_type&& key)
		{
			hash_key_type hash = FlatHashMap::HashFn(key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			FindResult result = this->FindOrPredictIndex(key, groupIndex, fp);

			if (result.result != invalidIndex)
				return m_data[result.result].value;

			std::size_t index = this->PrivInsert(Parameters{ std::move(key),value_type{} }, result, fp, hash);
			return m_data[index].value;

		}
		[[nodiscard]] const value_type& At(const key_type& key) const
		{
			hash_key_type hash = FlatHashMap::HashFn(key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			std::size_t index = FlatHashMap::FindValidIndex(key, groupIndex, fp);
			if (index == invalidIndex)
				throw std::out_of_range("not inserted");
			return m_data[index].value;
		}
		[[nodiscard]] value_type& At(const key_type& key)
		{
			hash_key_type hash = FlatHashMap::HashFn(key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			std::size_t index = FlatHashMap::FindValidIndex(key, groupIndex, fp);
			if (index == invalidIndex)
				throw std::out_of_range("not inserted");
			return m_data[index].value;
		}
		[[nodiscard]] bool Has(const key_type& key) const
		{
			hash_key_type hash = FlatHashMap::HashFn(key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			return  this->FindValidIndex(key, groupIndex, fp) != invalidIndex;
		}
		bool Insert(const key_type& key, const value_type& value = value_type{})
		{
			hash_key_type hash = FlatHashMap::HashFn(key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			FindResult result = this->FindOrPredictIndex(key, groupIndex, fp);

			if (result.result != invalidIndex)
				return false;

			std::size_t index = this->PrivInsert(Parameters{ key,value }, result, fp, hash);
			return index != invalidIndex;
		}
		bool Insert(key_type&& key, const value_type& value = value_type{})
		{
			hash_key_type hash = FlatHashMap::HashFn(key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			FindResult result = this->FindOrPredictIndex(key, groupIndex, fp);

			if (result.result != invalidIndex)
				return false;

			std::size_t index = this->PrivInsert(Parameters{ std::move(key),value }, result, fp, hash);
			return index != invalidIndex;
		}
		bool Insert(const key_type& key, value_type&& value)
		{
			hash_key_type hash = FlatHashMap::HashFn(key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			FindResult result = this->FindOrPredictIndex(key, groupIndex, fp);

			if (result.result != invalidIndex)
				return false;

			std::size_t index = this->PrivInsert(Parameters{ key,std::move(value) }, result, fp, hash);
			return index != invalidIndex;
		}
		bool Insert(key_type&& key, value_type&& value)
		{
			hash_key_type hash = FlatHashMap::HashFn(key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			FindResult result = this->FindOrPredictIndex(key, groupIndex, fp);

			if (result.result != invalidIndex)
				return false;

			std::size_t index = this->PrivInsert(Parameters{ std::move(key),std::move(value) }, result, fp, hash);
			return index != invalidIndex;
		}
		bool Insert(const Parameters& param)
		{
			hash_key_type hash = FlatHashMap::HashFn(param.key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			FindResult result = this->FindOrPredictIndex(param.key, groupIndex, fp);

			if (result.result != invalidIndex)
				return false;

			std::size_t index = this->PrivInsert(param, result, fp, hash);
			return index != invalidIndex;
		}
		bool Insert(Parameters&& param)
		{
			hash_key_type hash = FlatHashMap::HashFn(param.key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			FindResult result = this->FindOrPredictIndex(param.key, groupIndex, fp);

			if (result.result != invalidIndex)
				return false;

			std::size_t index = this->PrivInsert(std::move(param), result, fp, hash);
			return index != invalidIndex;
		}
		bool Erase(const key_type& key)
		{
			hash_key_type hash = FlatHashMap::HashFn(key);
			std::size_t groupIndex = this->GroupeIndex(hash);
			meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
			std::size_t index = this->FindValidIndex(key, groupIndex, fp);
			if (index == invalidIndex)
				return false;
			m_meta[index] = DELETED;
			--m_size;
			return true;
		}
			iterator_type Erase(iterator_type it)
			{
				iterator_type next = it;
				++next;
				this->Erase(it->key);
				return next;
			}
			const_iterator_type Erase(const_iterator_type it)
			{
				const_iterator_type next = it;
				++next;
				this->Erase(it->key);
				return next;
			}
			[[nodiscard]] iterator_type Find(const key_type& key)
			{
				hash_key_type hash = FlatHashMap::HashFn(key);
				std::size_t groupIndex = this->GroupeIndex(hash);
				meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
				std::size_t index = this->FindValidIndex(key, groupIndex, fp);
				if (index == invalidIndex)
					return this->end();
				return iterator_type(m_data.data() + index, m_meta.data() + index, m_data.size(), m_data.data() + m_data.size());
			}
			[[nodiscard]] const_iterator_type Find(const key_type& key) const
			{
				hash_key_type hash = FlatHashMap::HashFn(key);
				std::size_t groupIndex = this->GroupeIndex(hash);
				meta_key_type fp = FlatHashMap::GetFingerPrint(hash);
				std::size_t index = this->FindValidIndex(key, groupIndex, fp);
				if (index == invalidIndex)
					return this->cend();
				return const_iterator_type(m_data.data() + index, m_meta.data() + index, m_data.size(), m_data.data() + m_data.size());
			}
		[[nodiscard]] std::size_t GroupeCount() const
		{
			return m_data.size() >> 4;
		}
		void Rehash(std::size_t size)
		{
			PrivRehash(size);
		}
		void Reserve(std::size_t capacity)
		{
			const std::size_t newGroupCount = capacity >> 4;
			assert(((capacity & 0x0F) == 0) && ((newGroupCount & (newGroupCount - 1)) == 0), "capacity must be a power of 2 and a multiple of 16");
			m_data.reserve(capacity);
			m_meta.reserve(capacity);
		}
	private:
		[[nodiscard]] static constexpr hash_key_type HashFn(const key_type& key)
		{
			return hasher_type::Hash(key);
		}
		[[nodiscard]] std::size_t GroupeIndex(hash_key_type hash_) const
		{
			return (hash_ & ((m_data.size() >> 4) - 1)) << 4;
		}
		[[nodiscard]] static constexpr meta_key_type GetFingerPrint(hash_key_type hash_)
		{
			return static_cast<meta_key_type>(hash_) & HASH_MASK;
		}
		[[nodiscard]] bool RehashNeeded() const
		{
			return (m_data.empty() ? true : (m_size * loadfactorDen > loadfactorNum * m_data.size()));
		}
		void InsertIndex(Parameters&& param, meta_key_type fp, std::size_t index)
		{
			assert(index != invalidIndex, "shoul not be invalid");
			m_data[index] = std::move(param);
			m_meta[index] = fp;
			++m_size;
		}
		void PrivRehash(std::size_t size)
		{
			if (m_data.empty() && size == 0)
				return;

			std::vector<meta_key_type, meta_map_allocator> oldMeta = std::move(m_meta);
			std::vector<Parameters, map_allocator> oldData = std::move(m_data);
			assert((oldData.size() & 0x0F) == 0, "oldData.size() must be a multiple of 16");
			const std::size_t newGroupCount = size >> 4;
			assert(((size & 0x0F) == 0) && (size >= oldData.size()) && ((newGroupCount & (newGroupCount - 1)) == 0), "size must be a power of 2, a multiple of 16, and >= oldData.size()");

			m_meta.clear();
			m_meta.resize(size, EMPTY);

			m_data.clear();
			m_data.resize(size);

			m_size = 0;

			for (std::size_t i = 0; i < oldData.size(); ++i)
			{
				if (oldMeta[i] == EMPTY || oldMeta[i] == DELETED)
					continue;
				hash_key_type hash = FlatHashMap::HashFn(oldData[i].key);
				std::size_t groupeIndex = this->GroupeIndex(hash);
				std::size_t index = this->FindInsertIndex(groupeIndex);
				assert(index != invalidIndex, "Rehash: no free slot");
				this->InsertIndex(std::move(oldData[i]), oldMeta[i], index);
			}
		}
		[[nodiscard]] std::size_t FindValidIndex(const key_type& key, std::size_t groupIndex, meta_key_type fp) const
		{
			if (m_size == 0)
				return invalidIndex;
			for (std::size_t i = 0; i < this->GroupeCount(); ++i)
			{
				using pack_type = __m128i;
				pack_type indexGroup = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&m_meta[groupIndex]));
				pack_type fpGroup = _mm_set1_epi8(static_cast<char>(fp));
				pack_type versus = _mm_cmpeq_epi8(indexGroup, fpGroup);
				std::uint32_t mask = static_cast<std::uint32_t>(_mm_movemask_epi8(versus));
				while (mask != 0)
				{
					std::uint32_t val = std::countr_zero(mask);
					if (key == m_data[groupIndex + val].key)
						return groupIndex + val;
					mask &= (mask - 1);
				}
				pack_type emptyGroup = _mm_set1_epi8(static_cast<char>(EMPTY));
				versus = _mm_cmpeq_epi8(indexGroup, emptyGroup);
				mask = static_cast<std::uint32_t>(_mm_movemask_epi8(versus));
				if (mask == 0)
				{
					groupIndex += slotSize;
					if (groupIndex == m_data.size())
						groupIndex = 0;
					continue;
				}
				return invalidIndex;
			}
			return invalidIndex;
		}

		struct FindResult
		{
			std::size_t result;
			std::size_t possibleIndex;
		};

		[[nodiscard]] FindResult FindOrPredictIndex(const key_type& key, std::size_t groupIndex, meta_key_type fp) const
		{
			if (m_data.empty())
				return { invalidIndex,invalidIndex };
			std::size_t predict = invalidIndex;
			for (std::size_t i = 0; i < this->GroupeCount(); ++i)
			{
				using pack_type = __m128i;
				pack_type indexGroup = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&m_meta[groupIndex]));
				pack_type fpGroup = _mm_set1_epi8(static_cast<char>(fp));
				pack_type versus = _mm_cmpeq_epi8(indexGroup, fpGroup);
				std::uint32_t mask = static_cast<std::uint32_t>(_mm_movemask_epi8(versus));
				while (mask != 0)
				{
					std::uint32_t val = std::countr_zero(mask);
					if (key == m_data[groupIndex + val].key)
						return FindResult{ groupIndex + val,invalidIndex };
					mask &= (mask - 1);
				}
				if (predict == invalidIndex)
				{
					pack_type deletedGroup = _mm_set1_epi8(static_cast<char>(DELETED));
					versus = _mm_cmpeq_epi8(indexGroup, deletedGroup);
					mask = static_cast<std::uint32_t>(_mm_movemask_epi8(versus));
					if (mask != 0)
					{
						std::uint32_t val = std::countr_zero(mask);
						predict = groupIndex + val;
					}
				}

				pack_type emptyGroup = _mm_set1_epi8(static_cast<char>(EMPTY));
				versus = _mm_cmpeq_epi8(indexGroup, emptyGroup);
				mask = static_cast<std::uint32_t>(_mm_movemask_epi8(versus));
				if (mask == 0)
				{
					groupIndex += slotSize;
					if (groupIndex == m_data.size())
						groupIndex = 0;
					continue;
				}
				if (predict == invalidIndex)
				{
					std::uint32_t val = std::countr_zero(mask);
					predict = groupIndex + val;
				}
				return FindResult{ invalidIndex,predict };
			}
			return FindResult{ invalidIndex,invalidIndex };
		}
		[[nodiscard]] std::size_t FindInsertIndex(std::size_t groupIndex) const
		{
			for (std::size_t i = 0; i < this->GroupeCount(); ++i)
			{
				using pack_type = __m128i;
				pack_type indexGroup = _mm_loadu_si128(reinterpret_cast<__m128i const*>(&m_meta[groupIndex]));
				pack_type deleteGroup = _mm_set1_epi8(static_cast<char>(DELETED));
				pack_type versus = _mm_cmpeq_epi8(indexGroup, deleteGroup);
				std::uint32_t mask = static_cast<std::uint32_t>(_mm_movemask_epi8(versus));
				if (mask == 0)
				{
					pack_type emptyGroup = _mm_set1_epi8(static_cast<char>(EMPTY));
					versus = _mm_cmpeq_epi8(indexGroup, emptyGroup);
					mask = static_cast<std::uint32_t>(_mm_movemask_epi8(versus));
				}
				if (mask == 0)
				{
					groupIndex += slotSize;
					if (groupIndex == m_data.size())
						groupIndex = 0;
					continue;
				}
				std::uint32_t val = std::countr_zero(mask);
				return groupIndex + val;
			}
			return invalidIndex;
		}
		[[nodiscard]] std::size_t PrivInsert(Parameters&& param, const FindResult& result, meta_key_type fp, hash_key_type hash)
		{
			assert(result.result == invalidIndex, "error of index should not be inserted");
			if (this->RehashNeeded())
			{
				this->Rehash(m_data.empty() ? slotSize : m_data.size() * 2);
				//insert
				std::size_t groupIndex = this->GroupeIndex(hash);
				std::size_t index = this->FindInsertIndex(groupIndex);
				while (index == invalidIndex)
				{
					this->Rehash(m_data.size() * 2);
					groupIndex = this->GroupeIndex(hash);
					index = this->FindInsertIndex(groupIndex);
				}
				this->InsertIndex(std::move(param), fp, index);
				return index;
			}
			assert(result.possibleIndex != invalidIndex, "error of index should be Rehash");
			this->InsertIndex(std::move(param), fp, result.possibleIndex);
			return result.possibleIndex;
		}
		std::vector<meta_key_type, meta_map_allocator> m_meta;
		std::vector<Parameters, map_allocator> m_data;
		std::size_t m_size;
	};


}
#endif
