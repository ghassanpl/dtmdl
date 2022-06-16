#pragma once

enum class DefinitionType
{
	BuiltIn,
	Enum,
	Struct,
	Class,
};

struct TypeDefinition;
struct Database;
struct Schema;

struct TypeReference
{
	TypeDefinition const* Type = nullptr;
	vector<variant<uint64_t, TypeReference>> TemplateArguments;

	TypeReference() noexcept = default;
	TypeReference(TypeReference const&) noexcept = default;
	TypeReference(TypeReference&&) noexcept = default;
	TypeReference& operator=(TypeReference const&) noexcept = default;
	TypeReference& operator=(TypeReference&&) noexcept = default;

	TypeReference(Schema const& schema, json const& val);

	bool operator==(TypeReference const& other) const noexcept = default;

	explicit TypeReference(TypeDefinition const* value) noexcept;

	TypeDefinition const* operator->() const { return Type; }

	string ToString() const;
	json ToJSON() const;
	void FromJSON(Schema const& schema, json const& value);

	explicit operator bool() const noexcept { return Type != nullptr; }
};

using TemplateArgument = variant<uint64_t, TypeReference>;

inline void to_json(json& j, TypeReference const& v) { j = v.ToJSON(); }

enum class TemplateParameterQualifier
{
	AnyType,
	Struct,
	NotClass,
	Enum,
	Integral,
	Floating,
	Simple,

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

result<void, string> ValidateTemplateArgument(TemplateArgument const& arg, TemplateParameter const& param);

struct RecordDefinition;

struct TypeDefinition
{
	virtual ~TypeDefinition() noexcept = default;

	virtual DefinitionType Type() const noexcept = 0;

	bool IsClass() const noexcept { return Type() == DefinitionType::Class; }
	bool IsStruct() const noexcept { return Type() == DefinitionType::Struct; }
	bool IsRecord() const noexcept { return Type() == DefinitionType::Struct || Type() == DefinitionType::Class; }
	bool IsBuiltIn() const noexcept { return Type() == DefinitionType::BuiltIn; }
	bool IsEnum() const noexcept { return Type() == DefinitionType::Enum; }

	RecordDefinition const* AsRecord() const noexcept;

	auto const& Name() const noexcept { return mName; }
	auto const& BaseType() const noexcept { return mBaseType; }
	auto const& TemplateParameters() const noexcept { return mTemplateParameters; }

	virtual json ToJSON() const;
	virtual void FromJSON(Schema const& schema, json const& value);

protected:

	friend struct Database;

	bool mCompleted = false;

	string mName;
	TypeReference mBaseType{};
	vector<TemplateParameter> mTemplateParameters{};

	TypeDefinition(string name)
		: mName(move(name))
	{

	}

	TypeDefinition(string name, TypeReference base_type)
		: mName(move(name)), mBaseType(move(base_type))
	{
	}

};

struct FieldDefinition
{
	RecordDefinition const* ParentRecord = nullptr;
	string Name;
	TypeReference FieldType{};
	json InitialValue;

	json ToJSON() const { return json::object({ {"name", Name }, {"type", FieldType.ToJSON()}, {"initial", InitialValue} }); }
	void FromJSON(Schema const& schema, json const& value);

	string ToString() const { return format("var {} : {} = {};", Name, FieldType.ToString(), InitialValue.dump()); }
};

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

	//string FreshFieldName() const;

	virtual json ToJSON() const override;
	virtual void FromJSON(Schema const& schema, json const& value) override;

protected:

	friend struct Database;

	vector<unique_ptr<FieldDefinition>> mFields;

	using TypeDefinition::TypeDefinition;
};

struct StructDefinition : RecordDefinition
{
	virtual DefinitionType Type() const noexcept override { return DefinitionType::Struct; }
	//virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

protected:

	using RecordDefinition::RecordDefinition;
};

struct ClassDefinition : RecordDefinition
{
	virtual DefinitionType Type() const noexcept override { return DefinitionType::Class; }
	//virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

protected:

	using RecordDefinition::RecordDefinition;

};

struct EnumDefinition : TypeDefinition
{
	virtual DefinitionType Type() const noexcept override { return DefinitionType::Enum; }
	//virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

	virtual json ToJSON() const override
	{
		return TypeDefinition::ToJSON();
	}
	virtual void FromJSON(Schema const& schema, json const& value) override
	{
		TypeDefinition::FromJSON(schema, value);
	}

protected:

	using TypeDefinition::TypeDefinition;
};

struct BuiltinDefinition : TypeDefinition
{
	//virtual string NativeName() const override { return mNativeEquivalent; }

	auto Markable() const noexcept { return mMarkable; }
	auto ApplicableQualifiers() const noexcept { return mApplicableQualifiers; }

	virtual DefinitionType Type() const noexcept override { return DefinitionType::BuiltIn; }
	//virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

protected:

	friend struct Database;

	BuiltinDefinition(string name, string native, vector<TemplateParameter> template_params = {}, bool markable = false)
		: TypeDefinition(move(name), {}), mNativeEquivalent(move(native)), mMarkable(markable)
	{
		mTemplateParameters = move(template_params);
	}

	string mNativeEquivalent;
	bool mMarkable = false;
	ghassanpl::enum_flags<TemplateParameterQualifier> mApplicableQualifiers;

};

struct Schema
{
	//map<string, unique_ptr<TypeDefinition>, less<>> Definitions;
	vector<unique_ptr<TypeDefinition>> Definitions;

	TypeDefinition const* ResolveType(string_view name) const;
	TypeDefinition* ResolveType(string_view name);

	/// TODO: These
	size_t Version() const { return 1; }
	size_t Hash() const { return 0; }
};
