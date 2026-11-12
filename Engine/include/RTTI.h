#ifndef OOT_RTTI_F
#define OOT_RTTI_F
#include "Factory.h"
#include <type_traits>
#include "KTR/Hash/KTR_Hasher.h"
#include "RTTIType.h"
namespace OOT {

	template<void(*function)()>
	struct MagicStatic
	{
		MagicStatic()
		{
			function();
		}
	};


	struct BaseRtti
	{
		using string_type = rtti_str_type;
		using hash_type = rtti_hasher_type;
		using id_type = rtti_id_type;
		virtual ~BaseRtti() = default;
		BaseRtti() = default;
	protected:
		[[nodiscard]] virtual id_type Id() const  = 0;
		[[nodiscard]] virtual string_type Name() const = 0;
	};

	template<typename T>
	concept is_rtti_type = requires
	{
		requires std::is_base_of_v<BaseRtti, T>;
	};

	struct TypeInfo
	{
		using string_type = BaseRtti::string_type;
		using id_type = BaseRtti::id_type;
		template<is_rtti_type T>
		[[nodiscard]] static id_type Id(T& data)
		{
			return data.Id();
		};
		template<is_rtti_type T>
		[[nodiscard]] static string_type Name(T& data)
		{
			return data.Name();
		}
		template<is_rtti_type T>
		[[nodiscard]] static constexpr id_type Id()
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

namespace OOT
{
	struct ObjectData
	{
		using id_type = OOT::BaseRtti::id_type;
		using str_type = OOT::BaseRtti::string_type;
		id_type id = std::numeric_limits<id_type>::max();
		str_type str = " ";
	};
}


namespace OOT
{
	struct DataRegistry
	{
		using key_type_id = BaseRtti::id_type;
		using key_type_str = BaseRtti::string_type;
		using value_type = ObjectData;
		template<typename T>
		void Register()
		{
			ObjectData data;
			data.id = OOT::TypeInfo::Id<T>();
			data.str = OOT::TypeInfo::Name<T>();
			m_idMap[OOT::TypeInfo::Id<T>()] = data;
			m_strMap[OOT::TypeInfo::Name<T>()] = data;
		}
		[[nodiscard]] ObjectData Create(key_type_id id)
		{
			return m_idMap[id];
		}
		[[nodiscard]] ObjectData Create(const key_type_str& str)
		{
			return m_strMap[str];
		}
	private:
		 KTR::container::FlatHashMap<key_type_id, value_type> m_idMap = {};
		 KTR::container::FlatHashMap<key_type_str, value_type> m_strMap ={};

	};

	
}

namespace OOT
{
	
	static Factory<rtti_id_type>& GetRTTIFactoryId()
	{
		static Factory<rtti_id_type> factory ={};
		return factory;
	}
	static Factory<rtti_str_type>& GetRTTIFactoryStr()
	{
		static Factory<rtti_str_type> factory = {};
		return factory;
	}
	static DataRegistry& GetDataRegistry()
	{
		static DataRegistry reg = {};
		return reg;
	}
}


#define IMPL_RTTI(className,...)\
	friend class OOT::TypeInfo;\
	protected :\
	[[nodiscard]] OOT::BaseRtti::string_type Name() const  override\
	{\
		return className::SName();\
	}\
	[[nodiscard]] OOT::BaseRtti::id_type Id() const  override\
	{\
		return className::SId();\
	}\
	[[nodiscard]] static constexpr OOT::BaseRtti::string_type SName() \
	{\
		return #className;\
	}\
	[[nodiscard]] static constexpr OOT::BaseRtti::id_type SId() \
	{\
		return OOT::BaseRtti::hash_type::Hash(className::SName());	\
	}\
	private:\
    inline static OOT::MagicStatic<[](){ OOT::GetRTTIFactoryId().Register<className>(SId());OOT::GetRTTIFactoryStr().Register<className>(SName()); OOT::GetDataRegistry().Register<className>();}> m = {}; \
	public :



#endif
