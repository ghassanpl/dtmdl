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
	explicit TypeReference(TypeDefinition const* value, vector<variant<uint64_t, TypeReference>> args);

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
	string IconName() const noexcept { return string{ Icon() } + Name(); }
	auto const& BaseType() const noexcept { return mBaseType; }
	auto const& TemplateParameters() const noexcept { return mTemplateParameters; }
	auto const& Attributes() const noexcept { return mAttributes; }

	virtual json ToJSON() const;
	virtual void FromJSON(json const& value);

	virtual string_view Icon() const noexcept = 0;

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

enum class FieldFlags
{
	Private,
	Transient,
	Getter,
	Setter
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
	
	json ToJSON() const { return json::object({ {"name", Name }, {"type", FieldType.ToJSON()}, {"attributes", Attributes}, {"flags", Flags} }); }
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

	virtual string_view Icon() const noexcept { return ICON_VS_SYMBOL_STRUCTURE; };

protected:

	using RecordDefinition::RecordDefinition;
};

struct ClassDefinition : RecordDefinition
{
	virtual DefinitionType Type() const noexcept override { return DefinitionType::Class; }
	//virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

	virtual string_view Icon() const noexcept { return ICON_VS_SYMBOL_CLASS; };

protected:

	using RecordDefinition::RecordDefinition;

};

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
	
	json ToJSON() const { return json::object({ {"name", Name }, {"value", ::ToJSON(Value)}, {"descriptive", DescriptiveName}, {"attributes", Attributes}}); }
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

	virtual string_view Icon() const noexcept { return ICON_VS_SYMBOL_ENUM; };

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

	virtual string_view Icon() const noexcept { return mIcon; };

protected:

	friend struct Schema;

	BuiltinDefinition(::Schema const& schema, string name, string native, vector<TemplateParameter> template_params, bool markable, ghassanpl::enum_flags<TemplateParameterQualifier> applicable_qualifiers, string icon = ICON_VS_SYMBOL_MISC)
		: TypeDefinition(schema, move(name), {}), mNativeEquivalent(move(native)), mApplicableQualifiers(applicable_qualifiers), mMarkable(markable), mIcon(move(icon))
	{
		mTemplateParameters = move(template_params);
	}

	string mNativeEquivalent;
	bool mMarkable = false;
	ghassanpl::enum_flags<TemplateParameterQualifier> mApplicableQualifiers;
	string mIcon;

};

struct Schema
{
	Schema();

	auto Definitions() const noexcept { return mDefinitions | views::transform([](unique_ptr<TypeDefinition> const& element) -> TypeDefinition const* const { return element.get(); }); }

	TypeDefinition const* ResolveType(string_view name) const;
	template <typename T>
	T const* ResolveType(string_view name) const { return dynamic_cast<T const*>(ResolveType(name)); }

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

	BuiltinDefinition const* AddNative(string name, string native_name, vector<TemplateParameter> params, bool markable, ghassanpl::enum_flags<TemplateParameterQualifier> applicable_qualifiers, string icon = ICON_VS_SYMBOL_MISC);

	vector<unique_ptr<TypeDefinition>> mDefinitions;
	BuiltinDefinition const* mVoid = nullptr;

	TypeDefinition* ResolveType(string_view name);

	friend struct Database;
};
