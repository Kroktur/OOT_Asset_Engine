#include <iostream>

#include "Factory.h"
#include "RTTI.h"
#include "KTR/Hash/KTR_Hasher.h"
#include "KTR/Containers/KTR_FlatHashMap.h"

struct test : OOT::BaseRtti
{
	IMPL_RTTI(test)
};
struct test2 : OOT::BaseRtti
{
	IMPL_RTTI(test2)
};
struct test3 : test,test2
{
	IMPL_RTTI(test3)
};





int main()
{
	OOT::Factory::Register<test>(OOT::TypeInfo::Id<test>());
	OOT::Factory::Register<test2>(OOT::TypeInfo::Id<test2>());
	OOT::Factory::Register<test3>(OOT::TypeInfo::Id<test3>());
	OOT::Factory::Register<int>(0);
	std::unique_ptr<int> t = OOT::Factory::Create<int,std::unique_ptr<int>>(0);
	test* create = OOT::Factory::Create<test>(OOT::TypeInfo::Id<test2>());
	std::cout << OOT::TypeInfo::Name(*create);

	delete create;

	return 0;
}