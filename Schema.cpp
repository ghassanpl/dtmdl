#include "pch.h"

#include "Schema.h"
#include "Database.h"

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
				{
					TypeReference ref;
					ref.FromJSON(schema, arg);
					TemplateArguments.push_back(move(ref));
				}
			}
		}
	}
}

TypeReference::TypeReference(TypeDefinition const* value) noexcept : Type(value), TemplateArguments(value ? value->TemplateParameters().size() : 0) { }


FieldDefinition const* RecordDefinition::Field(string_view name) const
{
	for (auto& field : mFields)
	{
		if (field->Name == name)
			return field.get();
	}
	return nullptr;
}

size_t RecordDefinition::FieldIndexOf(FieldDefinition const* field) const
{
	for (size_t i = 0; i < mFields.size(); ++i)
		if (mFields[i].get() == field)
			return i;
	return -1;
}

string RecordDefinition::FreshFieldName() const
{
	string candidate = "Field";
	size_t num = 1;
	while (Field(candidate))
		candidate = format("Field{}", num++);
	return candidate;
}

json RecordDefinition::ToJSON() const
{
	json result = TypeDefinition::ToJSON();
	auto& fields = result["fields"] = json::array();
	for (auto& field : mFields)
		fields.push_back(field->ToJSON());
	return result;
}

void RecordDefinition::FromJSON(Schema const& schema, json const& value)
{
	TypeDefinition::FromJSON(schema, value);
	mFields.clear();
	auto& fields = value.at("fields").get_ref<json::array_t const&>();
	for (auto& field : fields)
	{
		auto new_field = make_unique<FieldDefinition>();
		new_field->FromJSON(schema, field);
		new_field->ParentRecord = this;
		mFields.push_back(move(new_field));
	}
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

void TypeDefinition::FromJSON(Schema const& schema, json const& value)
{
	mName = value.at("name").get_ref<json::string_t const&>();
	mBaseType.FromJSON(schema, value.at("base"));
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

void FieldDefinition::FromJSON(Schema const& schema, json const& value)
{
	Name = value.at("name").get_ref<json::string_t const&>();
	FieldType.FromJSON(schema, value.at("type"));
	InitialValue = value.at("initial");
}

TypeDefinition const* Schema::ResolveType(string_view name) const
{
	for (auto& def : Definitions)
		if (def->Name() == name)
			return def.get();
	return nullptr;
}

TypeDefinition* Schema::ResolveType(string_view name)
{
	for (auto& def : Definitions)
		if (def->Name() == name)
			return def.get();
	return nullptr;
}