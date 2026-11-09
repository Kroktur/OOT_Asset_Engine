#ifndef OOT_RTTI_F
#define OOT_RTTI_F
#include "KTR/Hash/KTR_Hasher.h"


namespace OOT {
	template<typename T>
	struct TypeName;
}

#define REG_RTTI(className)\					
template<>\
struct OOT::TypeName<className>\
{\
	using rtti_type = className;\
	using string_type = std::string_view;\
	using hash_type = KTR::hash::default_hasher_type<string_type>;\
	[[nodiscard]] static constexpr string_type Name()\
	{\
		return #className;\
	}\
	[[nodiscard]] static constexpr hash_type::result_type Id()\
	{\
		return hash_type::Hash(Name());\
	}\
};



namespace OOT
{
	
}


#endif
