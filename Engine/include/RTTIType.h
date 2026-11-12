#ifndef OOT_RTTITYPE_F
#define OOT_RTTITYPE_F
#include <KTR/Hash/KTR_Hasher.h>

namespace OOT
{
	using rtti_str_type = std::string;
	using rtti_hasher_type = KTR::hash::default_hasher_type<rtti_str_type>;
	using rtti_id_type = rtti_hasher_type::result_type;
}

#endif
