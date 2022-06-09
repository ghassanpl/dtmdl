#include "pch.h"

#include "Database.h"

#include <ghassanpl/wilson.h>

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

void TypeReference::FromJSON(Database const& db, json const& value)
{
	TemplateArguments.clear();
	if (value.is_null())
	{
		Type = {};
	}
	else
	{
		auto& type = value.get_ref<json::object_t const&>();
		Type = db.ResolveType(type.at("name"));
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
					ref.FromJSON(db, arg);
					TemplateArguments.push_back(move(ref));
				}
			}
		}
	}
}

TypeReference::TypeReference(TypeDefinition const* value) noexcept : Type(value), TemplateArguments(value ? value->TemplateParameters().size() : 0) { }

TypeDefinition const* Database::ResolveType(string_view name) const
{
	if (auto it = mSchema.Definitions.find(name); it != mSchema.Definitions.end())
		return it->second.get();
	return nullptr;
}

string Database::FreshTypeName(string_view base) const
{
	string candidate = string{ base };
	size_t num = 1;
	while (mSchema.Definitions.contains(candidate))
		candidate = format("{}{}", base, num++);
	return candidate;
}

static string_view cpp_keywords[] = {
	"alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char8_t",
	"char16_t", "char32_t", "class", "compl", "concept", "const", "consteval", "constexpr", "constinit", "const_cast", "continue",
	"co_await", "co_return", "co_yield", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit",
	"export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new",
	"noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public", "reflexpr", "register",
	"reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
	"switch", "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned",
	"using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq "
};

result<void, string> ValidateIdentifierName(string const& new_name)
{
	if (new_name.empty())
		return failure("name cannot be empty");
	if (!string_ops::ascii::isalpha(new_name[0]) && new_name[0] != '_')
		return failure("name must start with a letter or underscore");
	if (!ranges::all_of(new_name, string_ops::ascii::isident))
		return failure("name must contain only letters, numbers, or underscores");
	if (ranges::find(cpp_keywords, new_name) != ranges::end(cpp_keywords))
		return failure("name cannot be a C++ keyword");
	if (new_name.find("__") != string::npos)
		return failure("name cannot contain two consecutive underscores (__)");
	if (new_name.size() >= 2 && new_name[0] == '_' && string_ops::ascii::isupper(new_name[1]))
		return failure("name cannot begin with an underscore followed by a capital letter (_X)");
	return success();
}

result<void, string> Database::ValidateTypeName(Def def, string const& new_name)
{
	if (auto result = ValidateIdentifierName(new_name); result.has_error())
		return result;

	auto it = mSchema.Definitions.find(new_name);
	if (it == mSchema.Definitions.end())
		return success();
	if (it->second.get() == def)
		return success();
	return failure("a type with that name already exists");
}

result<void, string> Database::ValidateFieldName(Fld def, string const& new_name)
{
	if (auto result = ValidateIdentifierName(new_name); result.has_error())
		return result;

	auto rec = def->ParentRecord;
	for (auto& field : rec->mFields)
	{
		if (field.get() == def)
			continue;
		if (field->Name == new_name)
			return failure("a field with that name already exists");
	}
	return success();
}

result<void, string> Database::ValidateRecordBaseType(Rec def, TypeReference const& type)
{
	if (IsParent(def, type.Type))
		return failure("cycle in base types");
	return success();
}

result<void, string> Database::ValidateFieldType(Fld def, TypeReference const& type)
{
	return success();
}

result<StructDefinition const*, string> Database::AddNewStruct()
{
	/// 1. Validation
	
	/// 2. Schema Change
	auto result = AddType<StructDefinition>(FreshTypeName("Struct"));
	if (!result)
		return result;

	/// 3. DataStore update

	/// 4. ChangeLog add
	AddChangeLog(json{ {"action", "AddNewStruct"}, {"name", result->Name()} });
	
	/// 5. Save
	SaveAll();

	return success();
}

result<void, string> Database::AddNewField(Rec def)
{
	/// 1. Validation

	/// 2. Schema Change
	auto name = def->FreshFieldName();
	mut(def)->mFields.push_back(make_unique<FieldDefinition>(def, name));

	/// 3. DataStore update

	/// 4. ChangeLog add
	AddChangeLog(json{ {"action", "AddNewField"}, {"record", def->Name()}, {"fieldname", name} });

	/// 5. Save
	SaveAll();

	return success();
}

result<void, string> Database::SetRecordBaseType(Rec def, TypeReference const& type)
{
	/// 1. Validation
	auto result = ValidateRecordBaseType(def, type);
	if (result.has_error())
		return result;

	/// 2. Schema Change
	mut(def)->mBaseType = type;

	/// 3. DataStore update

	/// 4. ChangeLog add
	AddChangeLog(json{ {"action", "SetRecordBaseType"}, {"type", def->Name()}, {"basetype", type.ToJSON() } });

	/// 5. Save
	SaveAll();

	return success();
}

result<void, string> Database::SetTypeName(Def def, string const& new_name)
{
	/// 1. Validation
	auto result = ValidateTypeName(def, new_name);
	if (result.has_error())
		return result;

	/// 2. Schema Change
	auto old_name = def->Name();
	mut(def)->mName = new_name;

	/// 3. DataStore update

	/// 4. ChangeLog add
	AddChangeLog(json{ {"action", "SetTypeName"}, {"oldname", old_name}, {"newname", new_name } });

	/// 5. Save
	SaveAll();

	return success();
}

result<void, string> Database::SetFieldName(Fld def, string const& new_name)
{
	/// 1. Validation
	auto result = ValidateFieldName(def, new_name);
	if (result.has_error())
		return result;

	/// 2. Schema Change
	auto old_name = def->Name;
	mut(def)->Name = new_name;

	/// 3. DataStore update

	/// 4. ChangeLog add
	AddChangeLog(json{ {"action", "SetFieldName"}, {"oldname", old_name}, {"newname", new_name } });

	/// 5. Save
	SaveAll();

	return success();
}

result<void, string> Database::SetFieldType(Fld def, TypeReference const& type)
{
	/// 1. Validation
	auto result = ValidateFieldType(def, type);
	if (result.has_error())
		return result;

	/// 2. Schema Change
	mut(def)->FieldType = type;

	/// 3. DataStore update

	/// 4. ChangeLog add
	AddChangeLog(json{ {"action", "SetFieldType"}, {"field", def->Name}, {"type", type.ToJSON()}});

	/// 5. Save
	SaveAll();

	return success();
}

bool Database::IsParent(Def parent, Def potential_child)
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

/*
bool Database::IsParentOrChild(TypeDefinition const* a, TypeDefinition const* b)
{
	if (a == b)
		return true;
	if ((a && !a->IsRecord()) || (b && !b->IsRecord()))
		return false;
	//if (!a->mBaseType.Type && !b->mBaseType.Type)
		//return false;
	auto type_a = a;/// ->mBaseType.Type;
	auto type_b = b;/// ->mBaseType.Type;

	while (type_a)
	{
		if (type_a == type_b)
			return true;
		if (type_a->mBaseType.Type)
			type_a = type_a->mBaseType.Type;
		else
			break;
	}

	type_a = a;/// ->mBaseType.Type;

	while (type_b)
	{
		if (type_a == type_b)
			return true;
		if (type_b->mBaseType.Type)
			type_b = type_b->mBaseType.Type;
		else
			break;
	}

	return false;
}
*/

Database::Database(filesystem::path dir)
{
	if (filesystem::exists(dir) && !filesystem::is_directory(dir))
		throw std::invalid_argument("argument must be a directory");

	mDirectory = move(dir);

	error_code ec;
	filesystem::create_directories(mDirectory, ec);
	if (ec)
		throw std::invalid_argument("could not create target directory");

	mChangeLog.open(mDirectory / "changelog.wilson", ios::app|ios::out);

	AddNative("void", "::DataModel::NativeTypes::Void");
	AddNative("f32", "float");
	AddNative("f64", "double");
	AddNative("i8", "int8_t");
	AddNative("i16", "int16_t");
	AddNative("i32", "int32_t");
	AddNative("i64", "int64_t");
	AddNative("u8", "uint8_t");
	AddNative("u16", "uint16_t");
	AddNative("u32", "uint32_t");
	AddNative("u64", "uint64_t");
	AddNative("bool", "bool");
	AddNative("string", "::DataModel::NativeTypes::String");
	AddNative("bytes", "::DataModel::NativeTypes::Bytes");
	auto flags = AddNative("flags", "::DataModel::NativeTypes::Flags", vector{
		TemplateParameter{ "ENUM", TemplateParameterQualifier::Enum }
	});
	auto list = AddNative("list", "::DataModel::NativeTypes::List", vector{
		TemplateParameter{ "ELEMENT_TYPE", TemplateParameterQualifier::NotClass }
	}, true);
	auto arr = AddNative("array", "::DataModel::NativeTypes::Array", vector{
		TemplateParameter{ "ELEMENT_TYPE", TemplateParameterQualifier::NotClass },
		TemplateParameter{ "SIZE", TemplateParameterQualifier::Size }
	}, true);
	auto ref = AddNative("ref", "::DataModel::NativeTypes::Ref", vector{
		TemplateParameter{ "POINTEE", TemplateParameterQualifier::Class }
	}, true);
	auto own = AddNative("own", "::DataModel::NativeTypes::Own", vector{
		TemplateParameter{ "POINTEE", TemplateParameterQualifier::Class }
	}, true);

	auto variant = AddNative("variant", "::DataModel::NativeTypes::Variant", vector{
		TemplateParameter{ "TYPES", TemplateParameterQualifier::NotClass, true }
	}, true);

	if (filesystem::exists(mDirectory / "schema.json"))
	{
		LoadSchema(ghassanpl::load_json_file(mDirectory / "schema.json"));
	}

	SaveAll();
}

void Database::AddChangeLog(json log)
{
	mChangeLog << ghassanpl::to_wilson_string(log) << "\n";
}

void Database::SaveAll()
{
	ghassanpl::save_json_file(mDirectory / "schema.json", SaveSchema());
	ghassanpl::save_text_file(mDirectory / "header.hpp", "/// yay");
	ghassanpl::save_text_file(mDirectory / "wilson_data.wilson", "");
	mChangeLog.flush();
}

BuiltinDefinition const* Database::AddNative(string name, string native_name, vector<TemplateParameter> params, bool markable)
{
	return AddType<BuiltinDefinition>(move(name), move(native_name), move(params), markable);
}

FieldDefinition const* RecordDefinition::Field(string_view name) const
{
	for (auto& field : mFields)
	{
		if (field->Name == name)
			return field.get();
	}
	return nullptr;
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

void RecordDefinition::FromJSON(Database const& db, json const& value)
{
	TypeDefinition::FromJSON(db, value);
	mFields.clear();
	auto& fields = value.at("fields").get_ref<json::array_t const&>();
	for (auto& field : fields)
	{
		auto new_field = make_unique<FieldDefinition>();
		new_field->FromJSON(db, field);
		mFields.push_back(move(new_field));
	}
}

json Database::SaveSchema() const
{
	json result = json::object();
	result["version"] = 1;
	{
		auto& types = result["types"] = json::object();
		for (auto& [name, type] : mSchema.Definitions)
		{
			if (!type->IsBuiltIn())
				types[name] = magic_enum::enum_name(type->Type());
		}
	}

	{
		auto& types = result["typedesc"] = json::object();
		for (auto& [name, type] : mSchema.Definitions)
		{
			if (!type->IsBuiltIn())
				types[name] = type->ToJSON();
		}
	}

	return result;
}

void Database::LoadSchema(json const& from)
{
	auto& schema = from.get_ref<json::object_t const&>();

	auto version = (int)schema.at("version");
	if (!(version == 1))
		throw std::runtime_error("invalid schema version number");

	for (auto&& [name, type] : schema.at("types").get_ref<json::object_t const&>())
	{
		auto& type_def = mSchema.Definitions[name];
		type_def.reset();
		auto type_type = magic_enum::enum_cast<DefinitionType>(type.get_ref<json::string_t const&>()).value();
		switch (type_type)
		{
		case DefinitionType::Class: type_def = unique_ptr<ClassDefinition>{ new ClassDefinition(name) }; break;
		case DefinitionType::Struct: type_def = unique_ptr<StructDefinition>{ new StructDefinition(name) }; break;
		case DefinitionType::Enum: type_def = unique_ptr<EnumDefinition>{ new EnumDefinition(name) }; break;
		}
	}

	for (auto&& [name, typedesc] : schema.at("typedesc").get_ref<json::object_t const&>())
	{
		auto& type_def = mSchema.Definitions.at(name);
		type_def->FromJSON(*this, typedesc);
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

void TypeDefinition::FromJSON(Database const& db, json const& value)
{
	mName = value.at("name").get_ref<json::string_t const&>();
	mBaseType.FromJSON(db, value.at("base"));
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
	Multiple = value.at("multiple");
}

void FieldDefinition::FromJSON(Database const& db, json const& value)
{
	Name = value.at("name").get_ref<json::string_t const&>();
	FieldType.FromJSON(db, value.at("type"));
	InitialValue = value.at("initial");
}
