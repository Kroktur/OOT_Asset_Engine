#include <iostream>

#include "Factory.h"
#include "RTTI.h"
#include "KTR/Hash/KTR_Hasher.h"
#include "KTR/Containers/KTR_FlatHashMap.h"

namespace OOT
{
	struct BaseObject : OOT::BaseRtti
	{
	public:
		BaseObject() = default;
		IMPL_RTTI(BaseObject)
		virtual void Serialize(){}
		virtual void Deserialize(){}
	};
}




struct test : OOT::BaseObject
{
	IMPL_RTTI(test)
};
struct test2 : OOT::BaseObject
{
	IMPL_RTTI(test2)
};
struct test3 : test,test2
{
	IMPL_RTTI(test3)
};






int main()
{
	

	// simulate load 
	OOT::ObjectData data = OOT::GetDataRegistry().Create(OOT::TypeInfo::Id<test3>());
	OOT::ObjectData data2 = OOT::GetDataRegistry().Create(OOT::TypeInfo::Id<test2>());

	std::unique_ptr<OOT::BaseObject> create2 = OOT::GetRTTIFactoryId().Create<std::unique_ptr<OOT::BaseObject>>(data2.id);
	OOT::BaseObject* create = OOT::GetRTTIFactoryId().Create<OOT::BaseObject*>(data.id);

	std::cout << OOT::TypeInfo::Name(*create);
	std::cout << OOT::TypeInfo::Name(*create2);

	
	delete create;
	return 0;
}