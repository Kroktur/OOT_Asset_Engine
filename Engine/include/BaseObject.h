#ifndef OOT_BASEOBJECT_F
#define OOT_BASEOBJECT_F

#include "RTTI.h"
#include "Serialize.h"
#include "Serialization/ArchiveFile.h"

namespace OOT
{
	struct BaseObject : OOT::BaseRtti
	{
	public:
		BaseObject() = default;
		IMPL_RTTI(BaseObject)
		// arthur
		virtual void Read(Archive&) {}
		virtual void Write(Archive&) {}
		// noe
		virtual void Serialize(oot::serialization::Archive&) {}
		virtual void Deserialize(oot::serialization::Archive&) {}
	};
}



template<>
struct OOT::Serialize<std::unique_ptr<OOT::BaseObject>>
{
	using value_type = std::unique_ptr<OOT::BaseObject>;
	static void Write(Archive& archive, const value_type& ptr)
	{
		OOT::Serialize<OOT::rtti_id_type>::Write(archive, OOT::TypeInfo::Id(*ptr));
		ptr->Write(archive);
	}
	static value_type Read(Archive& archive)
	{
		OOT::rtti_id_type id = OOT::Serialize<OOT::rtti_id_type>::Read(archive);
		OOT::ObjectData data = OOT::GetDataRegistry().Create(id);
		value_type result = OOT::GetRTTIFactoryId().Create<value_type>(data.id);
		result->Read(archive);
		return std::move(result);
	}
};
template<>
struct OOT::Serialize<std::shared_ptr<OOT::BaseObject>>
{
	using value_type = std::shared_ptr<OOT::BaseObject>;
	static void Write(Archive& archive, const value_type& ptr)
	{
		OOT::Serialize<OOT::rtti_id_type>::Write(archive, OOT::TypeInfo::Id(*ptr));
		ptr->Write(archive);
	}
	static value_type Read(Archive& archive)
	{
		OOT::rtti_id_type id = OOT::Serialize<OOT::rtti_id_type>::Read(archive);
		OOT::ObjectData data = OOT::GetDataRegistry().Create(id);
		value_type result = OOT::GetRTTIFactoryId().Create<value_type>(data.id);
		result->Read(archive);
		return result;
	}
};

template<>
struct OOT::Serialize<OOT::BaseObject*>
{
	using value_type = OOT::BaseObject*;
	static void Write(Archive& archive, const value_type& ptr)
	{
		OOT::Serialize<OOT::rtti_id_type>::Write(archive, OOT::TypeInfo::Id(*ptr));
		ptr->Write(archive);
	}
	static value_type Read(Archive& archive)
	{
		OOT::rtti_id_type id = OOT::Serialize<OOT::rtti_id_type>::Read(archive);
		OOT::ObjectData data = OOT::GetDataRegistry().Create(id);
		value_type result = OOT::GetRTTIFactoryId().Create<value_type>(data.id);
		result->Read(archive);
		return result;
	}
};

#endif
