#pragma once

enum class DefinitionType
{
	BuiltIn, /// TODO: Maybe name it scalar instead?
	Enum,
	Struct,
	Class,

	/// Union
	/// Alias
};

struct TypeDefinition;
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
	///Scalar, /// Scalar/Simple - wtf does that even mean?!

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
struct Schema;

struct TypeDefinition
{
	virtual ~TypeDefinition() noexcept = default;

	TypeDefinition(Schema const& schema) noexcept : mSchema(schema) {}

	virtual DefinitionType Type() const noexcept = 0;

	bool IsClass() const noexcept { return Type() == DefinitionType::Class; }
	bool IsStruct() const noexcept { return Type() == DefinitionType::Struct; }
	bool IsRecord() const noexcept { return Type() == DefinitionType::Struct || Type() == DefinitionType::Class; }
	bool IsBuiltIn() const noexcept { return Type() == DefinitionType::BuiltIn; }
	bool IsEnum() const noexcept { return Type() == DefinitionType::Enum; }

	RecordDefinition const* AsRecord() const noexcept;

	auto const& Schema() const noexcept { return mSchema; }
	auto const& Name() const noexcept { return mName; }
	auto const& BaseType() const noexcept { return mBaseType; }
	auto const& TemplateParameters() const noexcept { return mTemplateParameters; }
	auto const& Attributes() const noexcept { return mAttributes; }

	virtual json ToJSON() const;
	virtual void FromJSON(json const& value);

protected:

	friend struct Schema;
	friend struct Database;

	bool mCompleted = false;

	::Schema const& mSchema;

	string mName;
	TypeReference mBaseType{};
	vector<TemplateParameter> mTemplateParameters{};
	json mAttributes;

	TypeDefinition(::Schema const& schema, string name)
		: mSchema(schema), mName(move(name))
	{

	}

	TypeDefinition(::Schema const& schema, string name, TypeReference base_type)
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

	FieldDefinition(RecordDefinition const* parent, string name, TypeReference ref) : ParentRecord(parent), Name(move(name)), FieldType(move(ref)) {}
	FieldDefinition(RecordDefinition const* parent, json const& def) : ParentRecord(parent) { FromJSON(def); }
	
	json ToJSON() const { return json::object({ {"name", Name }, {"type", FieldType.ToJSON()}, {"attributes", Attributes} }); }
	void FromJSON(json const& value);

	string ToString() const { return format("var {} : {}; // {}", Name, FieldType.ToString(), Attributes.dump()); }
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

	virtual json ToJSON() const override;
	virtual void FromJSON(json const& value) override;

protected:

	friend struct Schema;
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

struct EnumDefinition;

struct EnumeratorDefinition
{
	EnumDefinition const* ParentEnum = nullptr;
	string Name;
	int64_t Value{};
	string DescriptiveName;
	json Attributes;

	EnumeratorDefinition(EnumDefinition const* parent, string name, int64_t value) : ParentEnum(parent), Name(name), Value(value), DescriptiveName(name) {}
	EnumeratorDefinition(EnumDefinition const* parent, json const& def) : ParentEnum(parent) { FromJSON(def); }
	
	json ToJSON() const { return json::object({ {"name", Name }, {"value", Value}, {"descriptive", DescriptiveName}, {"attributes", Attributes}}); }
	void FromJSON(json const& value);

	string ToString() const { return format("{} = {}; /// {} /// {}", Name, Value, DescriptiveName, Attributes.dump()); }
};

struct EnumDefinition : TypeDefinition
{
	virtual DefinitionType Type() const noexcept override { return DefinitionType::Enum; }
	//virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

	virtual json ToJSON() const override;
	virtual void FromJSON(json const& value) override;

	auto const& Enumerators() const noexcept { return mEnumerators; }

protected:

	friend struct Schema;
	friend struct Database;

	vector<unique_ptr<EnumeratorDefinition>> mEnumerators;

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

	friend struct Schema;

	BuiltinDefinition(::Schema const& schema, string name, string native, vector<TemplateParameter> template_params, bool markable, ghassanpl::enum_flags<TemplateParameterQualifier> applicable_qualifiers)
		: TypeDefinition(schema, move(name), {}), mNativeEquivalent(move(native)), mMarkable(markable)
	{
		mTemplateParameters = move(template_params);
	}

	string mNativeEquivalent;
	bool mMarkable = false;
	ghassanpl::enum_flags<TemplateParameterQualifier> mApplicableQualifiers;

};

struct Schema
{
	Schema();

	auto Definitions() const noexcept { return mDefinitions | views::transform([](unique_ptr<TypeDefinition> const& element) -> TypeDefinition const* const { return element.get(); }); }

	TypeDefinition const* ResolveType(string_view name) const;

	BuiltinDefinition const* VoidType() const noexcept { return mVoid; }

	/// TODO: These
	size_t Version() const { return 1; }
	size_t Hash() const { return 0; }

	static bool IsParent(TypeDefinition const* parent, TypeDefinition const* potential_child);

private:

	template <typename T, typename... ARGS>
	T const* AddType(ARGS&&... args)
	{
		auto ptr = unique_ptr<T>(new T{ *this, forward<ARGS>(args)... });
		auto result = ptr.get();
		mDefinitions.push_back(move(ptr));
		return result;
	}

	BuiltinDefinition const* AddNative(string name, string native_name, vector<TemplateParameter> params, bool markable, ghassanpl::enum_flags<TemplateParameterQualifier> applicable_qualifiers);

	vector<unique_ptr<TypeDefinition>> mDefinitions;
	BuiltinDefinition const* mVoid = nullptr;

	TypeDefinition* ResolveType(string_view name);

	friend struct Database;
};
