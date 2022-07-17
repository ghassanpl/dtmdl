#pragma once

#include "dtmdl.h"

namespace dtmdl
{
	static constexpr const char* IconsForDefinitionType[magic_enum::enum_count<DefinitionType>()] = {
		"",
		ICON_VS_SYMBOL_ENUM,
		ICON_VS_SYMBOL_STRUCTURE,
		ICON_VS_SYMBOL_CLASS,
		ICON_VS_SYMBOL_MISC,
		ICON_VS_ARROW_SMALL_RIGHT,
	};

	json ToJSON(TypeReference const&);
	void FromJSON(TypeReference&, Schema const& schema, json const& value);
	void CalculateDependencies(TypeReference const& ref, set<TypeDefinition const*>& dependencies) ;

	bool IsCopyable(TypeReference const& ref);

	inline void to_json(json& j, TypeReference const& v) { j = ToJSON(v); }

	inline TypeReference TypeFromJSON(Schema const& schema, json const& arg)
	{
		TypeReference result;
		FromJSON(result, schema, arg);
		return result;
	}

	enum class TemplateParameterQualifier
	{
		AnyType,
		Struct,
		NotClass,
		Enum,
		Integral,
		Floating,
		Scalar, /// means comparable by

		Size,
		Pointer,
		Class,
	};

	enum class TemplateParameterFlags
	{
		Multiple,
		CanBeIncomplete,
	};

	struct TemplateParameter
	{
		string Name;
		TemplateParameterQualifier Qualifier{};
		enum_flags<TemplateParameterFlags> Flags;

		bool MustBeComplete() const noexcept { return !Flags.contain(TemplateParameterFlags::CanBeIncomplete); }

		json ToJSON() const { return json::object({ {"name", Name }, {"qualifier", magic_enum::enum_name(Qualifier)}, {"flags", string_ops::join(Flags, ",", [](auto e) { return magic_enum::enum_name(e); })} }); }
		void FromJSON(json const& value);
	};

	struct RecordDefinition;
	struct StructDefinition;
	struct ClassDefinition;
	struct EnumDefinition;
	struct Schema;

	struct TypeDefinition
	{
		virtual ~TypeDefinition() noexcept = default;

		TypeDefinition(Schema const& schema) noexcept : mSchema(schema) {}

		virtual DefinitionType Type() const noexcept = 0;

		virtual bool IsCopyable() const noexcept { return true; }

		bool IsClass() const noexcept { return Type() == DefinitionType::Class; }
		bool IsStruct() const noexcept { return Type() == DefinitionType::Struct; }
		bool IsRecord() const noexcept { return Type() == DefinitionType::Struct || Type() == DefinitionType::Class; }
		bool IsBuiltIn() const noexcept { return Type() == DefinitionType::BuiltIn; }
		bool IsEnum() const noexcept { return Type() == DefinitionType::Enum; }

		RecordDefinition const* AsRecord() const noexcept;
		StructDefinition const* AsStruct() const noexcept;
		ClassDefinition const* AsClass() const noexcept;
		EnumDefinition const* AsEnum() const noexcept;

		auto const& Schema() const noexcept { return mSchema; }
		auto const& Name() const noexcept { return mName; }
		//string QualifiedName(string_view sep = "::") const noexcept;
		string IconName() const noexcept { return string{ Icon() } + Name(); }
		string IconNameWithParent() const noexcept {
			if (mBaseType)
				return format("{}{} : {}", Icon(), Name(), mBaseType->Name());
			return IconName();
		}
		auto const& BaseType() const noexcept { return mBaseType; }
		auto const& TemplateParameters() const noexcept { return mTemplateParameters; }
		auto const& Attributes() const noexcept { return mAttributes; }

		bool IsChildOf(TypeDefinition const* other) const
		{
			if (mBaseType.Type == other)
				return true;
			if (mBaseType.Type)
				return mBaseType.Type->IsChildOf(other);
			return false;
		}

		template <typename FUNC>
		void ForEachBase(FUNC&& func) const
		{
			auto base = mBaseType;
			while (base)
			{
				if (!func(base))
					break;
				base = base.Type->BaseType();
			}
		}

		virtual json ToJSON() const;
		virtual void FromJSON(json const& value);

		virtual string_view Icon() const noexcept = 0;

		virtual void CalculateDependencies(set<TypeDefinition const*>& dependencies) const = 0;

	protected:

		friend struct Schema;
		friend struct Database;

		bool mCompleted = false;

		dtmdl::Schema const& mSchema;

		string mName;
		TypeReference mBaseType{};
		vector<TemplateParameter> mTemplateParameters{};
		json mAttributes;

		TypeDefinition(dtmdl::Schema const& schema, string name)
			: mSchema(schema), mName(move(name))
		{

		}

		TypeDefinition(dtmdl::Schema const& schema, string name, TypeReference base_type)
			: mSchema(schema), mName(move(name)), mBaseType(move(base_type))
		{
		}

	};

	struct FieldDefinition
	{
		RecordDefinition const* ParentRecord = nullptr;
		string Name;
		TypeReference FieldType{};
		json Attributes;
		enum_flags<FieldFlags> Flags;

		FieldDefinition(RecordDefinition const* parent, string name, TypeReference ref) : ParentRecord(parent), Name(move(name)), FieldType(move(ref)) {}
		FieldDefinition(RecordDefinition const* parent, json const& def) : ParentRecord(parent) { FromJSON(def); }

		json ToJSON() const;
		void FromJSON(json const& value);

		string ToString() const { return format("var {} : {}; // {}", Name, FieldType.ToString(), Attributes.dump()); }

		void CalculateDependencies(set<TypeDefinition const*>& dependencies) const
		{
			dtmdl::CalculateDependencies(FieldType, dependencies);
		}
	};

	bool IsFlagAvailable(FieldDefinition const* fld, FieldFlags flag);

	struct RecordDefinition : TypeDefinition
	{
		/// TODO: Properties and Methods, maybe

		FieldDefinition const* Field(size_t i) const;
		FieldDefinition const* OwnField(string_view name) const;
		FieldDefinition const* OwnOrBaseField(string_view name) const;
		size_t FieldIndexOf(FieldDefinition const* field) const;

		set<string> OwnFieldNames() const;
		set<string> AllFieldNames() const;

		auto const& Fields() const noexcept { return mFields; }

		vector<FieldDefinition const*> AllFieldsOrdered() const;

		virtual json ToJSON() const override;
		virtual void FromJSON(json const& value) override;

		virtual void CalculateDependencies(set<TypeDefinition const*>& dependencies) const override
		{
			dtmdl::CalculateDependencies(mBaseType, dependencies);
			for (auto& field : mFields)
				field->CalculateDependencies(dependencies);
		}

	protected:

		friend struct Schema;
		friend struct Database;

		vector<unique_ptr<FieldDefinition>> mFields;

		using TypeDefinition::TypeDefinition;
	};

	struct StructDefinition : RecordDefinition
	{
		enum_flags<StructFlags> Flags;
		/// string PrimaryKey;

		/// TODO: When CreateTableType is set, give the option to choose a primary key - either a rowid or a Unique field
		/// This will allow us to optimize the table types, same as WITHOUT ROWID (https://www.sqlite.org/withoutrowid.html)
		/// Note: This will also mean that deleting (or moving) a field that is the primary key will need to be validated

		virtual DefinitionType Type() const noexcept override { return DefinitionType::Struct; }

		virtual string_view Icon() const noexcept { return ICON_VS_SYMBOL_STRUCTURE; };

		virtual json ToJSON() const override;
		virtual void FromJSON(json const& value) override;

	protected:

		using RecordDefinition::RecordDefinition;
	};

	bool IsFlagAvailable(StructDefinition const* fld, StructFlags flag);

	struct AttributeDefinition : RecordDefinition
	{
		virtual DefinitionType Type() const noexcept override { return DefinitionType::Attribute; }

		virtual string_view Icon() const noexcept { return ICON_VS_TAG; };

	protected:

		using RecordDefinition::RecordDefinition;
	};

	struct ClassDefinition : RecordDefinition
	{
		enum_flags<ClassFlags> Flags;

		virtual DefinitionType Type() const noexcept override { return DefinitionType::Class; }

		virtual string_view Icon() const noexcept { return ICON_VS_SYMBOL_CLASS; };

		virtual json ToJSON() const override;
		virtual void FromJSON(json const& value) override;

	protected:

		using RecordDefinition::RecordDefinition;

	};

	bool IsFlagAvailable(ClassDefinition const* fld, ClassFlags flag);

	struct EnumDefinition;

	inline json ToJSON(optional<int64_t> const& val)
	{
		if (val.has_value())
			return json{ val.value() };
		return json{};
	}

	struct EnumeratorDefinition
	{
		EnumDefinition const* ParentEnum = nullptr;
		string Name;
		optional<int64_t> Value{};
		string DescriptiveName;
		json Attributes;

		EnumeratorDefinition(EnumDefinition const* parent, string name, optional<int64_t> value) : ParentEnum(parent), Name(name), Value(value) {}
		EnumeratorDefinition(EnumDefinition const* parent, json const& def) : ParentEnum(parent) { FromJSON(def); }

		int64_t ActualValue() const;

		json ToJSON() const { return json::object({ {"name", Name }, {"value", dtmdl::ToJSON(Value)}, {"descriptive", DescriptiveName}, {"attributes", Attributes} }); }
		void FromJSON(json const& value);

		string ToString() const {
			if (Value.has_value())
				return format("{} = {}, /// {} /// {}", Name, Value.value(), DescriptiveName, Attributes.dump());
			else
				return format("{}, /// {} /// {}", Name, DescriptiveName, Attributes.dump());
		}
	};

	struct EnumDefinition : TypeDefinition
	{
		virtual DefinitionType Type() const noexcept override { return DefinitionType::Enum; }

		EnumeratorDefinition const* Enumerator(size_t i) const;
		EnumeratorDefinition const* Enumerator(string_view name) const;
		size_t EnumeratorIndexOf(EnumeratorDefinition const* field) const;

		auto DefaultEnumerator() const { return Enumerator(0); }

		virtual json ToJSON() const override;
		virtual void FromJSON(json const& value) override;

		auto Enumerators() const noexcept { return mEnumerators | views::transform([](unique_ptr<EnumeratorDefinition> const& element) -> EnumeratorDefinition const* const { return element.get(); }); }
		auto EnumeratorCount() const noexcept { return mEnumerators.size(); }

		virtual string_view Icon() const noexcept { return ICON_VS_SYMBOL_ENUM; };

		virtual void CalculateDependencies(set<TypeDefinition const*>& dependencies) const override {}

	protected:

		friend struct Schema;
		friend struct Database;

		vector<unique_ptr<EnumeratorDefinition>> mEnumerators;

		using TypeDefinition::TypeDefinition;
	};

	enum class BuiltInFlags
	{
		Markable,
		NonCopyable,
	};

	struct BuiltinDefinition : TypeDefinition
	{
		//virtual string NativeName() const override { return mNativeEquivalent; }

		auto Markable() const noexcept { return mFlags.contain(BuiltInFlags::Markable); }
		virtual bool IsCopyable() const noexcept { return !mFlags.contain(BuiltInFlags::NonCopyable); }
		auto ApplicableQualifiers() const noexcept { return mApplicableQualifiers; }

		virtual DefinitionType Type() const noexcept override { return DefinitionType::BuiltIn; }


		virtual string_view Icon() const noexcept { return mIcon; };

		virtual void CalculateDependencies(set<TypeDefinition const*>& dependencies) const override {}

	protected:

		friend struct Schema;

		BuiltinDefinition(dtmdl::Schema const& schema, string name, string native, vector<TemplateParameter> template_params, enum_flags<BuiltInFlags> flags, ghassanpl::enum_flags<TemplateParameterQualifier> applicable_qualifiers, string icon = ICON_VS_SYMBOL_MISC)
			: TypeDefinition(schema, move(name), {}), mNativeEquivalent(move(native)), mApplicableQualifiers(applicable_qualifiers), mFlags(flags), mIcon(move(icon))
		{
			mTemplateParameters = move(template_params);
		}

		string mNativeEquivalent;
		enum_flags<BuiltInFlags> mFlags = {};
		ghassanpl::enum_flags<TemplateParameterQualifier> mApplicableQualifiers;
		string mIcon;
	};

	struct Schema
	{
		Schema();

		auto Definitions() const noexcept { return mDefinitions | views::transform([](unique_ptr<TypeDefinition> const& element) -> TypeDefinition const* const { return element.get(); }); }
		auto UserDefinitions() const noexcept { return Definitions() | views::filter([](TypeDefinition const* def) { return !def->IsBuiltIn(); }); }

		TypeDefinition const* ResolveType(string_view name) const;
		template <typename T>
		T const* ResolveType(string_view name) const { return dynamic_cast<T const*>(ResolveType(name)); }

		BuiltinDefinition const* VoidType() const noexcept { return mVoid; }

		/// TODO: These
		size_t Version() const { return 1; }
		size_t Hash() const { return 0; }

		static bool IsParent(TypeDefinition const* parent, TypeDefinition const* potential_child);

		string Namespace = "database";

	private:

		template <typename T, typename... ARGS>
		T const* AddType(ARGS&&... args)
		{
			auto ptr = unique_ptr<T>(new T{ *this, forward<ARGS>(args)... });
			auto result = ptr.get();
			mDefinitions.push_back(move(ptr));
			return result;
		}

		BuiltinDefinition const* AddNative(string name, string native_name, vector<TemplateParameter> params, enum_flags<BuiltInFlags> flags, ghassanpl::enum_flags<TemplateParameterQualifier> applicable_qualifiers, string icon = ICON_VS_SYMBOL_MISC);

		vector<unique_ptr<TypeDefinition>> mDefinitions;
		BuiltinDefinition const* mVoid = nullptr;

		TypeDefinition* ResolveType(string_view name);

		friend struct Database;
	};

}