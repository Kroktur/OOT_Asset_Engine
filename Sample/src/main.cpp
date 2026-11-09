#include <iostream>

#include "RTTI.h"
#include "KTR/Hash/KTR_Hasher.h"
#include "KTR/Hash/KTR_FNV_1AString.h"
struct Noe
{
};

REG_RTTI(Noe)






int main()

{
	std::cout << KTR::hash::default_hasher_type<const char*>::Hash("toto") << " " << KTR::hash::default_hasher_type<const char*>::Hash("toto") << " " << KTR::hash::default_hasher_type<const char*>::Hash("titi");
	return 0;
}