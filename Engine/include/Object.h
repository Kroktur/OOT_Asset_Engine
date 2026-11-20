#pragma once
#include "BaseObject.h"

struct MapObject : OOT::BaseObject
{
	IMPL_RTTI(MapObject);
	MapObject() = default;
	std::map<std::string, std::pair<int, int>>& Entries()
	{
		return m_map;
	}

	void Read(Archive& archive) override
	{
		m_map = OOT::Serialize<std::map<std::string, std::pair<int, int>>>::Read(archive);
	}
	void Write(Archive& archive) override
	{
		OOT::Serialize<std::map<std::string, std::pair<int, int>>>::Write(archive, m_map);
	}
	void Serialize(oot::serialization::Archive& ar) override
	{
		ar.serialize(m_map);	
	}
	void Deserialize(oot::serialization::Archive& ar) override
	{
		ar.deserialize(m_map);
	}
private:
	std::map<std::string, std::pair<int, int>> m_map;
};

struct IntegralObject : OOT::BaseObject
{
	IMPL_RTTI(IntegralObject);
	IntegralObject() = default;
	int& Value()
	{
		return m_value;
	}

	float& FloatValue()
	{
		return m_valueF;
	}

	void Read(Archive& archive) override
	{
		m_value = OOT::Serialize<int>::Read(archive);
		m_valueF = OOT::Serialize<float>::Read(archive);
	}
	void Write(Archive& archive) override
	{
		OOT::Serialize<int>::Write(archive, m_value);
		OOT::Serialize<float>::Write(archive, m_valueF);
	}
	void Serialize(oot::serialization::Archive& ar) override
	{
		ar.serialize(m_value);
		ar.serialize(m_valueF);

	}
	void Deserialize(oot::serialization::Archive& ar) override
	{
		ar.deserialize(m_value);
		ar.deserialize(m_valueF);
	}
private:
	int m_value = 0;
	float m_valueF = 0.0f;
};

struct PolyMorphObject : OOT::BaseObject
{
	IMPL_RTTI(PolyMorphObject);
	PolyMorphObject()
	{
		object = std::make_unique<IntegralObject>();
	}
	OOT::BaseObject* GetObject() { return object.get(); }
	void SetObject(std::unique_ptr<OOT::BaseObject> value)
	{
		if (!value)
		{
			throw std::invalid_argument("A child object is required");
		}

		object = std::move(value);
	}
	void Serialize(oot::serialization::Archive& ar) override
	{
		ar.serialize(object);
	}
	void Deserialize(oot::serialization::Archive& ar) override
	{
		ar.deserialize(object);
	}
	void Read(Archive& archive) override
	{
		object = OOT::Serialize<std::unique_ptr<OOT::BaseObject>>::Read(archive);
	}
	void Write(Archive& archive) override
	{
		OOT::Serialize<std::unique_ptr<OOT::BaseObject>>::Write(archive, object);
	}
private:
	std::unique_ptr<OOT::BaseObject> object;
};


