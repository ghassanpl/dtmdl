#include "pch.h"

#include "Schema.h"

RecordDefinition const* TypeDefinition::AsRecord() const noexcept { return IsRecord() ? static_cast<RecordDefinition const*>(this) : nullptr; }
EnumDefinition const* TypeDefinition::AsEnum() const noexcept { return IsEnum() ? static_cast<EnumDefinition const*>(this) : nullptr; }

string to_string(TypeReference const& tr)
{
	return tr.ToString();
}

string TypeReference::ToString() const
{
	if (!Type)
		return "[none]";

	string base = Type->Name();
	if (TemplateArguments.size())
	{
		base += '<';
		bool first = true;
		for (auto& arg : TemplateArguments)
		{
			if (!first)
				base += ", ";
			base += visit([](auto const& val) { return to_string(val); }, arg);
			first = false;
		}
		base += '>';
	}
	return base;
}

json TypeReference::ToJSON() const
{
	if (!Type)
		return json{};
	json result = json::object();
	result["name"] = Type->Name();
	if (TemplateArguments.size())
	{
		auto& args = result["args"] = json::array();
		for (auto& arg : TemplateArguments)
			visit([&args](auto const& val) { args.push_back(val); }, arg);
	}
	return result;
}

void TypeReference::FromJSON(Schema const& schema, json const& value)
{
	TemplateArguments.clear();
	if (value.is_null())
	{
		Type = {};
	}
	else
	{
		auto& type = value.get_ref<json::object_t const&>();
		Type = schema.ResolveType(type.at("name"));
		if (!Type)
			throw std::runtime_error(format("type '{}' not found", (string)type.at("name")));

		if (auto it = type.find("args"); it != type.end())
		{
			TemplateArguments.clear();
			for (auto& arg : it->second.get_ref<json::array_t const&>())
			{
				if (arg.is_number())
					TemplateArguments.emplace_back((uint64_t)arg);
				else
					TemplateArguments.push_back(TypeReference{schema, arg});
			}
		}
	}
}

TypeReference::TypeReference(Schema const& schema, json const& val)
{
	FromJSON(schema, val);
}

TypeReference::TypeReference(TypeDefinition const* value, vector<TemplateArgument> args)
	: Type(value)
	, TemplateArguments(move(args))
{
	if (value && value->TemplateParameters().size() > TemplateArguments.size())
		throw runtime_error(format("invalid number of arguments for type '{}': expected {}, got {}", value->Name(), value->TemplateParameters().size(), TemplateArguments.size()));
}

TypeReference::TypeReference(TypeDefinition const* value) noexcept 
	: Type(value)
	, TemplateArguments(value ? value->TemplateParameters().size() : 0)
{
}

FieldDefinition const* RecordDefinition::Field(size_t index) const
{
	if (index >= mFields.size())
		return nullptr;
	return mFields[index].get();
}

FieldDefinition const* RecordDefinition::OwnField(string_view name) const
{
	for (auto& field : mFields)
	{
		if (field->Name == name)
			return field.get();
	}
	return nullptr;
}

FieldDefinition const* RecordDefinition::OwnOrBaseField(string_view name) const
{
	auto field = OwnField(name);
	if (field)
		return field;
	if (mBaseType.Type)
		return mBaseType.Type->AsRecord()->OwnOrBaseField(name);
	return nullptr;
}

size_t RecordDefinition::FieldIndexOf(FieldDefinition const* field) const
{
	for (size_t i = 0; i < mFields.size(); ++i)
		if (mFields[i].get() == field)
			return i;
	return -1;
}

set<string> RecordDefinition::OwnFieldNames() const
{
	set<string> result;
	for (auto& f : mFields)
		result.insert(f->Name);
	return result;
}

set<string> RecordDefinition::AllFieldNames() const
{
	set<string> result;
	TypeDefinition const* rec = this;
	while (rec)
	{
		for (auto& f : rec->AsRecord()->mFields)
			result.insert(f->Name);
		rec = rec->BaseType().Type;
	}
	return result;
}

vector<FieldDefinition const*> RecordDefinition::AllFieldsOrdered() const
{
	vector<FieldDefinition const*> result;

	if (auto type = BaseType(); type && type->IsRecord())
		result = type->AsRecord()->AllFieldsOrdered();
	ranges::transform(mFields, back_inserter(result), [](auto const& f) { return f.get(); });

	return result;
}

json RecordDefinition::ToJSON() const
{
	json result = TypeDefinition::ToJSON();
	auto& fields = result["fields"] = json::array();
	for (auto& field : mFields)
		fields.push_back(field->ToJSON());
	return result;
}

void RecordDefinition::FromJSON(json const& value)
{
	TypeDefinition::FromJSON(value);
	mFields.clear();
	auto& fields = value.at("fields").get_ref<json::array_t const&>();
	for (auto& field : fields)
		mFields.push_back(make_unique<FieldDefinition>(this, field));
}

void FieldDefinition::FromJSON(json const& value)
{
	Name = value.at("name").get_ref<json::string_t const&>();
	FieldType.FromJSON(ParentRecord->Schema(), value.at("type"));
	Attributes = get(value, "attributes");
	Flags = get_array(value, "flags");
}

int64_t EnumeratorDefinition::ActualValue() const
{
	int64_t current = 0;
	for (auto e : ParentEnum->Enumerators())
	{
		current = e->Value.value_or(current);
		if (e == this)
			return current;
		++current;
	}
	throw "what";
}

void EnumeratorDefinition::FromJSON(json const& value)
{
	Name = value.at("name").get_ref<json::string_t const&>();
	json const& v = value.at("value");
	if (v.is_null())
		Value = nullopt;
	else
		Value = int64_t(v);
	DescriptiveName = value.at("descriptive").get_ref<json::string_t const&>();
	Attributes = get(value, "attributes");
}

json TypeDefinition::ToJSON() const
{
	json result = json::object();
	result["name"] = mName;
	result["base"] = mBaseType;
	if (mTemplateParameters.size())
	{
		auto& params = result["params"] = json::array();
		for (auto& param : mTemplateParameters)
			params.push_back(param.ToJSON());
	}
	return result;
}

void TypeDefinition::FromJSON(json const& value)
{
	mName = value.at("name").get_ref<json::string_t const&>();
	mBaseType.FromJSON(Schema(), value.at("base"));
	if (auto it = value.find("params"); it != value.end())
	{
		mTemplateParameters.clear();
		auto& params = it->get_ref<json::array_t const&>();
		for (auto& param : params)
			mTemplateParameters.emplace_back().FromJSON(param);
	}
}

void TemplateParameter::FromJSON(json const& value)
{
	Name = value.at("name").get_ref<json::string_t const&>();
	Qualifier = magic_enum::enum_cast<TemplateParameterQualifier>(value.at("qualifier").get_ref<json::string_t const&>()).value();
	string s = value.at("flags");
	string_ops::split(s, ",", [this](string_view s, bool) { Flags.set(magic_enum::enum_cast<TemplateParameterFlags>(s).value()); });
}

Schema::Schema()
{
	mVoid = AddNative("void", "::DataModel::NativeTypes::Void", {}, false, {}, ICON_VS_CIRCLE_SLASH);
	AddNative("f32", "float", {}, false, { TemplateParameterQualifier::Floating, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
	AddNative("f64", "double", {}, false, { TemplateParameterQualifier::Floating, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
	AddNative("i8", "int8_t", {}, false, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
	AddNative("i16", "int16_t", {}, false, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
	AddNative("i32", "int32_t", {}, false, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
	AddNative("i64", "int64_t", {}, false, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
	AddNative("u8", "uint8_t", {}, false, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
	AddNative("u16", "uint16_t", {}, false, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
	AddNative("u32", "uint32_t", {}, false, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
	AddNative("u64", "uint64_t", {}, false, { TemplateParameterQualifier::Integral, TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_NUMERIC);
	AddNative("bool", "bool", {}, false, { TemplateParameterQualifier::NotClass }, ICON_VS_SYMBOL_BOOLEAN);
	AddNative("string", "::DataModel::NativeTypes::String", {}, false, { TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_SYMBOL_STRING);
	AddNative("bytes", "::DataModel::NativeTypes::Bytes", {}, false, { TemplateParameterQualifier::NotClass }, ICON_VS_FILE_BINARY);
	AddNative("flags", "::DataModel::NativeTypes::Flags", vector{
		TemplateParameter{ "ENUM", TemplateParameterQualifier::Enum }
	}, false, { TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Scalar }, ICON_VS_CHECKLIST);
	AddNative("list", "::DataModel::NativeTypes::List", vector{
		TemplateParameter{ "ELEMENT_TYPE", TemplateParameterQualifier::NotClass, TemplateParameterFlags::CanBeIncomplete }
	}, true, { TemplateParameterQualifier::NotClass }, ICON_VS_SYMBOL_ARRAY);
	AddNative("array", "::DataModel::NativeTypes::Array", vector{
		TemplateParameter{ "ELEMENT_TYPE", TemplateParameterQualifier::NotClass },
		TemplateParameter{ "SIZE", TemplateParameterQualifier::Size }
	}, true, { TemplateParameterQualifier::NotClass }, ICON_VS_LIST_ORDERED);
	AddNative("ref", "::DataModel::NativeTypes::Ref", vector{
		TemplateParameter{ "POINTEE", TemplateParameterQualifier::Class, TemplateParameterFlags::CanBeIncomplete }
	}, true, { TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Pointer, TemplateParameterQualifier::Scalar }, ICON_VS_REFERENCES);
	AddNative("own", "::DataModel::NativeTypes::Own", vector{
		TemplateParameter{ "POINTEE", TemplateParameterQualifier::Class, TemplateParameterFlags::CanBeIncomplete }
	}, true, { TemplateParameterQualifier::NotClass, TemplateParameterQualifier::Pointer, TemplateParameterQualifier::Scalar }, ICON_VS_REFERENCES);
	AddNative("variant", "::DataModel::NativeTypes::Variant", vector{
		TemplateParameter{ "TYPES", TemplateParameterQualifier::NotClass, TemplateParameterFlags::Multiple }
		}, true, { TemplateParameterQualifier::NotClass }, ICON_VS_TASKLIST);

	AddNative("map", "::DataModel::NativeTypes::Map", vector{
		TemplateParameter{ "KEY_TYPE", TemplateParameterQualifier::Scalar },
		TemplateParameter{ "VALUE_TYPE", TemplateParameterQualifier::NotClass, TemplateParameterFlags::CanBeIncomplete }
	}, true, { TemplateParameterQualifier::NotClass }, ICON_VS_SYMBOL_OBJECT);

	AddNative("json", "::DataModel::NativeTypes::JSON", {}, false, { TemplateParameterQualifier::NotClass }, ICON_VS_JSON);

	string_view prefixes[] = { "b", "i", "u", "d", ""};
	string_view types[] = { "bool", "int", "unsigned", "double", "float"};
	for (int i = 0; i < 5; ++i)
	{
		for (int d = 2; d <= 4; ++d)
		{
			string name = format("{}vec{}", prefixes[i], d);
			string real_name = format("tvec<{}, {}>", types[i], d);

			AddNative(name, real_name, {}, false, { TemplateParameterQualifier::NotClass }, ICON_VS_LOCATION);
		}
	}

	/// TODO: Color ? Or should we leave that to attributes?
	/// TODO: Path ?
}

TypeDefinition const* Schema::ResolveType(string_view name) const
{
	for (auto& def : mDefinitions)
		if (def->Name() == name)
			return def.get();
	return nullptr;
}

TypeDefinition* Schema::ResolveType(string_view name)
{
	for (auto& def : mDefinitions)
		if (def->Name() == name)
			return def.get();
	return nullptr;
}

EnumeratorDefinition const* EnumDefinition::Enumerator(size_t index) const
{
	if (index >= mEnumerators.size())
		return nullptr;
	return mEnumerators[index].get();
}

EnumeratorDefinition const* EnumDefinition::Enumerator(string_view name) const
{
	for (auto& e : mEnumerators)
	{
		if (e->Name == name)
			return e.get();
	}
	return nullptr;
}

size_t EnumDefinition::EnumeratorIndexOf(EnumeratorDefinition const* field) const
{
	for (size_t i = 0; i < mEnumerators.size(); ++i)
		if (mEnumerators[i].get() == field)
			return i;
	return -1;
}

json EnumDefinition::ToJSON() const
{
	json result = TypeDefinition::ToJSON();
	auto& fields = result["enumerators"] = json::array();
	for (auto& enumerator : mEnumerators)
		fields.push_back(enumerator->ToJSON());
	return result;
}

void EnumDefinition::FromJSON(json const& value)
{
	TypeDefinition::FromJSON(value);
	mEnumerators.clear();
	auto& enumerators = value.at("enumerators").get_ref<json::array_t const&>();
	for (auto& enumerator : enumerators)
		mEnumerators.push_back(make_unique<EnumeratorDefinition>(this, enumerator));
}


BuiltinDefinition const* Schema::AddNative(string name, string native_name, vector<TemplateParameter> params, bool markable, ghassanpl::enum_flags<TemplateParameterQualifier> applicable_qualifiers, string icon)
{
	return AddType<BuiltinDefinition>(move(name), move(native_name), move(params), markable, applicable_qualifiers, move(icon));
}

bool Schema::IsParent(TypeDefinition const* parent, TypeDefinition const* potential_child)
{
	if ((parent && !parent->IsRecord()) || (potential_child && !potential_child->IsRecord()))
		return false;

	while (potential_child)
	{
		if (parent == potential_child)
			return true;
		potential_child = potential_child->mBaseType.Type;
	}

	return false;
}

void TypeReference::CalculateDependencies(set<TypeDefinition const*>& dependencies) const
{
	if (!Type)
		return;

	dependencies.insert(Type);

	for (size_t i =0; i< Type->TemplateParameters().size(); ++i)
	{
		auto& param = Type->TemplateParameters()[i];
		auto& arg = TemplateArguments[i];

		if (param.MustBeComplete())
		{
			if (auto ref = get_if<TypeReference>(&arg))
				ref->CalculateDependencies(dependencies);
		}
	}
}

json ClassDefinition::ToJSON() const
{
	auto result = RecordDefinition::ToJSON();
	result["flags"] = Flags;
	return result;
}

void ClassDefinition::FromJSON(json const& value)
{
	RecordDefinition::FromJSON(value);
	Flags = get_array(value, "flags");
}
