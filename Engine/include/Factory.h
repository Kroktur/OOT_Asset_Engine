#ifndef OOT_FACTORY_F
#define OOT_FACTORY_F
#include <cassert>
#include <type_traits>
#include <KTR/Containers/KTR_FlatHashMap.h>
#include <KTR/Meta/KTR_MetaTools.h>

namespace OOT
{
	template<typename T>
	struct is_smart_ptr : KTR::Meta::false_type{};

	template<typename T>
	struct is_smart_ptr<std::unique_ptr<T>> : KTR::Meta::true_type{};
	template<typename T>
	struct is_smart_ptr<std::shared_ptr<T>> : KTR::Meta::true_type {};
	template<typename T>
	struct is_smart_ptr<std::weak_ptr<T>> : KTR::Meta::true_type {};

	template<typename T>
	static constexpr  bool is_smart_ptr_v = is_smart_ptr<T>::value;

	template<typename T>
	static constexpr bool is_ptr_v = std::is_pointer_v<T> || is_smart_ptr_v<T>;


	using creator_fn = void* (*)();
	template<typename idT = KTR::hash::default_hasher_type<std::string>::result_type>
	struct Factory
	{
		using id_type = idT;
		using value_type = creator_fn;
		using map_type = KTR::container::FlatHashMap<id_type, value_type>;
		template<typename T> /*requires(std::is_default_constructible_v<T>)*/
		void Register(id_type id);

		template<typename T>requires(std::is_default_constructible_v<T> && is_ptr_v<T>)
		[[nodiscard]] T Create(id_type id);
	private:
		 map_type m_map = {};
	};
	template<typename idT>
	template <typename T> /*requires (std::is_default_constructible_v<T>)*/
	void Factory<idT>::Register(id_type id)
	{
		assert(!m_map.Has(id), "key already register");
		creator_fn create = []() { return static_cast<void*>(new T());};
		m_map[id] = create;
	}

	template<typename idT>
	template <typename T> requires (std::is_default_constructible_v<T>&& is_ptr_v<T>)
	T Factory<idT>::Create(id_type id)
	{
		assert(m_map.Has(id), "key not register");
		if constexpr (is_smart_ptr_v<T>)
			return T(static_cast<T::pointer>(m_map[id]()));
		else
			return static_cast<T>(m_map[id]());
	}
}

#endif
