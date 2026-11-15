#include <iostream>

#include "Factory.h"
#include "RTTI.h"
#include "KTR/Hash/KTR_Hasher.h"
#include "KTR/Containers/KTR_FlatHashMap.h"
#include "filesystem"
#include "fstream"
#include "RTTI.h"
#include <array>
#include <map>
#include <unordered_map>

#include "BaseObject.h"
#include "Serialize.h"






struct test : OOT::BaseObject
{
	IMPL_RTTI(test)
private:
	int t;
};
struct test4 : OOT::BaseObject
{
	IMPL_RTTI(test4)
};
struct test2 : OOT::BaseObject
{
	
	test2()
	{
		t = new test4{};
	}
	~test2()
	{
		delete t;
	}
	void Read(Archive& ar) override
	{
		delete t;
		t = OOT::Serialize<OOT::BaseObject*>::Read(ar);
		toto = OOT::Serialize<int>::Read(ar);
		std::cout << OOT::TypeInfo::Name(*this);
		std::cout << OOT::TypeInfo::Name(*t);
		std::cout << toto;
	}
	void Write(Archive& ar) override
	{
		OOT::Serialize<OOT::BaseObject*>::Write(ar, t);
		OOT::Serialize<int>::Write(ar,toto);

	}
	IMPL_RTTI(test2)
private:
	int toto = 5;
	OOT::BaseObject* t;
};
struct test3 : test,test2
{
	IMPL_RTTI(test3)
};







int main(int argc, char* argv[])
{
	Archive archive;
	std::filesystem::path path = argv[0];
	path = path.parent_path().parent_path().parent_path().parent_path().parent_path() / "Resources";
	archive.OpenFile(path / "test_1.json");
	//archive.ClearFile();
	//OOT::Serialize<float>::Write(archive, 5.53f);
	//std::cout << OOT::Serialize<float>::Read(archive) << std::endl;
	using t = std::vector<
		std::map<
		std::string,
		std::vector<std::pair<int, float>>
		>
	>;
	t test;/* = {
	{
		{
			"Player",
			{
				{100, 10.5f},
				{200, 20.5f}
			}
		},
		{
			"Enemy",
			{
				{50, 5.5f},+
				{75, 7.5f}
			}
		}
	}
	};*/
	//OOT::Serialize<t>::Write(archive, test);
	test = OOT::Serialize<t>::Read(archive);
	archive.CloseFile();
	//delete test;

	// simulate load 
	OOT::ObjectData data = OOT::GetDataRegistry().Create(OOT::TypeInfo::Id<test3>());
	OOT::ObjectData data2 = OOT::GetDataRegistry().Create(OOT::TypeInfo::Id<test2>());

	std::unique_ptr<OOT::BaseObject> create2 = OOT::GetRTTIFactoryId().Create<std::unique_ptr<OOT::BaseObject>>(data2.id);
	OOT::BaseObject* create = OOT::GetRTTIFactoryId().Create<OOT::BaseObject*>(data.id);

	delete create;
	return 0;
}