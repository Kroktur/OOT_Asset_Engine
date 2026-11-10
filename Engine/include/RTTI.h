#ifndef OOT_RTTI_F
#define OOT_RTTI_F
#include <KTR/Meta/KTR_TypeList.h>
#include <type_traits>
#include "KTR/Hash/KTR_Hasher.h"

namespace OOT {

	struct BaseRtti
	{
		virtual ~BaseRtti() = default;
	protected:
		using string_type = const char*;
		using hash_type = KTR::hash::default_hasher_type<string_type>;
		[[nodiscard]] virtual hash_type::result_type Id() const  = 0;
		[[nodiscard]] virtual string_type Name() const = 0;
	};

	template<typename T>
	concept is_rtti_type = requires
	{
		requires std::is_base_of_v<BaseRtti, T>;
	};

	struct TypeInfo
	{
		using string_type = const char*;
		using hash_type = KTR::hash::default_hasher_type<string_type>;
		template<is_rtti_type T>
		[[nodiscard]] static hash_type::result_type Id(T& data)
		{
			return data.Id();
		};
		template<is_rtti_type T>
		[[nodiscard]] static string_type Name(T& data)
		{
			return data.Name();
		}
		template<is_rtti_type T>
		[[nodiscard]] static constexpr hash_type::result_type Id()
		{
			return T::SId();
		};
		template<is_rtti_type T>
		[[nodiscard]] static constexpr  string_type Name()
		{
			return T::SName();
		}
	};
}

#define IMPL_RTTI(className,...)\
	friend class OOT::TypeInfo;\
	protected :\
	[[nodiscard]] OOT::BaseRtti::string_type Name() const  override\
	{\
		return className::SName();\
	}\
	[[nodiscard]] OOT::BaseRtti::hash_type::result_type Id() const  override\
	{\
		return className::SId();\
	}\
	[[nodiscard]] static constexpr OOT::BaseRtti::string_type SName() \
	{\
		return #className;\
	}\
	[[nodiscard]] static constexpr OOT::BaseRtti::hash_type::result_type SId() \
	{\
		return OOT::BaseRtti::hash_type::Hash(className::SName());	\
	}\
	public :



#endif
