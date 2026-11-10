#ifndef OOT_FACTORY_F
#define OOT_FACTORY_F
#include <cassert>
#include <type_traits>
#include <KTR/Containers/KTR_FlatHashMap.h>

namespace OOT
{
	using creator_fn = void* (*)();

	struct Factory
	{
		using id_type = KTR::hash::default_hasher_type<const char*>::result_type;
		using value_type = creator_fn;
		using map_type = KTR::container::FlatHashMap<id_type, value_type>;
		template<typename T> requires(std::is_constructible_v<T>)
			static void Register(id_type id);

		template<typename T>requires(std::is_constructible_v<T>)
			[[nodiscard]] static T* Create(id_type id);

		template<typename T, typename Cr> requires(std::is_constructible_v<Cr, T*>&& std::is_constructible_v<T>)
			[[nodiscard]] static Cr Create(id_type id);

	private:
		static map_type m_map;
	};

	template <typename T> requires (std::is_constructible_v<T>)
	void Factory::Register(id_type id)
	{
		assert(!m_map.Has(id), "key already register");
		creator_fn create = []() { return static_cast<void*>(new T());};
		m_map[id] = create;
	}

	template <typename T> requires (std::is_constructible_v<T>)
	T* Factory::Create(id_type id)
	{
		assert(m_map.Has(id), "key not register");
		return static_cast<T*>(m_map[id]());
	}

	template <typename T, typename Cr> requires (std::is_constructible_v<Cr, T*> && std::is_constructible_v<T>)
	Cr Factory::Create(id_type id)
	{
		assert(m_map.Has(id), "key not register");
		return Cr(static_cast<T*>(m_map[id]()));
	}

	inline  Factory::map_type Factory::m_map = {};
}

#endif
