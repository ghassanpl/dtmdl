#include "pch.h"

#include "Database.h"

#include <ghassanpl/wilson.h>
#include "X:\Code\Native\ghassanpl\windows_message_box\windows_message_box.h"

string Database::FreshTypeName(string_view base) const
{
	string candidate = string{ base };
	size_t num = 1;
	while (mSchema.ResolveType(candidate))
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

	auto type = mSchema.ResolveType(new_name);
	if (!type)
		return success();
	if (type == def)
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

result<void, string> ValidateTemplateArgument(TemplateArgument const& arg, TemplateParameter const& param)
{
	if (param.Qualifier == TemplateParameterQualifier::Size)
	{
		if (holds_alternative<uint64_t>(arg))
			return success();
		return failure("template argument must be a size");
	}

	if (!holds_alternative<TypeReference>(arg))
		return failure("template argument must be a type");

	auto& arg_type = get<TypeReference>(arg);

	if (!arg_type.Type)
		return failure("type must be set");

	switch (param.Qualifier)
	{
	case TemplateParameterQualifier::AnyType: break;
	case TemplateParameterQualifier::Struct:
		if (arg_type->Type() != DefinitionType::Struct)
			return "must be a struct type";
		break;
	case TemplateParameterQualifier::NotClass:
		if (arg_type->Type() == DefinitionType::Class)
			return "must be a non-class type";
		break;
	case TemplateParameterQualifier::Enum:
		if (arg_type->Type() != DefinitionType::Enum)
			return "must be an enum type";
		break;
	case TemplateParameterQualifier::Integral:
	case TemplateParameterQualifier::Floating:
	case TemplateParameterQualifier::Simple:
	case TemplateParameterQualifier::Pointer:
		if (arg_type->Type() != DefinitionType::BuiltIn)
			return "must be an integral type";
		else
		{
			auto builtin_type = dynamic_cast<BuiltinDefinition const*>(arg_type.Type);
			if (builtin_type && builtin_type->ApplicableQualifiers().is_set(param.Qualifier) == false)
				return format("must be a {} type", string_ops::ascii::tolower(magic_enum::enum_name(param.Qualifier)));
		}
		break;
	case TemplateParameterQualifier::Class:
		if (arg_type->Type() != DefinitionType::Class)
			return "must be a class type";
		break;
	default:
		throw format("internal error: unimplemented template parameter qualifier `{}`", magic_enum::enum_name(param.Qualifier));
	}

	return success();
}

result<void, string> Database::ValidateFieldType(Fld def, TypeReference const& type)
{
	if (!type.Type)
		return failure("field type is invalid");

	//if (type.Type->Name() == "void") return failure("field type cannot be void");

	set<TypeDefinition const*> open_types;
	open_types.insert(def->ParentRecord);
	auto is_open = [&](TypeDefinition const* type) -> TypeDefinition const* {
		/// If the type itself is open
		if (open_types.contains(type)) return type;

		/// If any base type is open
		auto parent_type = type->BaseType().Type;
		while (parent_type && parent_type->IsRecord())
		{
			if (open_types.contains(parent_type))
			{
				/// For performance, insert all types up to the open base type into the open set
				for (auto t = type; t != parent_type; t = t->BaseType().Type)
					open_types.insert(t);
				return parent_type;
			}
			parent_type = type->BaseType().Type;
		}
		return nullptr;
	};

	auto check_type = [](this auto& check_type, TypeReference const& type, auto& is_open) -> result<void, string> {
		if (auto incomplete = is_open(type.Type))
			return failure(format("{}: type is dependent on an incomplete type: {}", type.ToString(), incomplete->Name()));

		if (type.TemplateArguments.size() < type.Type->TemplateParameters().size())
			return failure(format("{}: invalid number of template arguments given", type.ToString()));

		for (size_t i = 0; i < type.TemplateArguments.size(); ++i)
		{
			auto& arg = type.TemplateArguments[i];
			auto& param = type.Type->TemplateParameters().at(i);
			auto result = ValidateTemplateArgument(arg, param);
			if (result.has_failure())
				return failure(format("{}: in template argument {}:\n{}", type.ToString(), i, move(result).error()));
			if (param.MustBeComplete())
			{
				if (auto ref = get_if<TypeReference>(&arg))
				{
					if (result = check_type(*ref, is_open); result.has_failure())
						return failure(format("{}: in template argument {}:\n{}", type.ToString(), i, move(result).error()));
				}
			}
		}
		return success();
	};

	return check_type(type, is_open);
}

result<StructDefinition const*, string> Database::AddNewStruct()
{
	/// Validation
	
	/// Schema Change
	auto result = AddType<StructDefinition>(FreshTypeName("Struct"));
	if (!result)
		return result;

	/// DataStore update
	UpdateDataStore([name = result->Name()](DataStore& store) {
		store.AddNewStruct(name);
	});

	/// ChangeLog add
	AddChangeLog(json{ {"action", "AddNewStruct"}, {"name", result->Name()} });
	
	/// Save
	SaveAll();

	return success();
}

result<void, string> Database::AddNewField(Rec def)
{
	/// Validation

	/// Schema Change
	auto name = def->FreshFieldName();
	mut(def)->mFields.push_back(make_unique<FieldDefinition>(def, name, TypeReference{ mVoid }));

	/// DataStore update
	UpdateDataStore([record_name = def->Name(), &name](DataStore& store) {
		store.AddNewField(record_name, name);
	});

	/// ChangeLog add
	AddChangeLog(json{ {"action", "AddNewField"}, {"record", def->Name()}, {"fieldname", name} });

	/// Save
	SaveAll();

	return success();
}

result<void, string> Database::SetRecordBaseType(Rec def, TypeReference const& type)
{
	/// Validation
	auto result = ValidateRecordBaseType(def, type);
	if (result.has_error())
		return result;

	/// ChangeLog add
	AddChangeLog(json{ {"action", "SetRecordBaseType"}, {"type", def->Name()}, {"basetype", type.ToJSON() }, {"previous", def->BaseType().ToJSON()} });

	/// Schema Change
	mut(def)->mBaseType = type;

	/// DataStore update
	/// TODO

	/// Save
	SaveAll();

	return success();
}

result<void, string> Database::SetTypeName(Def def, string const& new_name)
{
	/// Validation
	auto result = ValidateTypeName(def, new_name);
	if (result.has_error())
		return result;

	/// ChangeLog add
	AddChangeLog(json{ {"action", "SetTypeName"}, {"oldname", def->Name()}, {"newname", new_name } });

	/// Schema Change
	auto old_name = def->Name();
	mut(def)->mName = new_name;

	/// DataStore update
	UpdateDataStore([&](DataStore& store) {
		store.SetTypeName(old_name, new_name);
	});

	/// Save
	SaveAll();

	return success();
}

result<void, string> Database::SetFieldName(Fld def, string const& new_name)
{
	/// Validation
	auto result = ValidateFieldName(def, new_name);
	if (result.has_error())
		return result;

	/// ChangeLog add
	AddChangeLog(json{ {"action", "SetFieldName"}, {"record", def->ParentRecord->Name()}, {"oldname", def->Name}, {"newname", new_name}});

	/// Schema Change
	auto old_name = def->Name;
	mut(def)->Name = new_name;

	/// DataStore update
	UpdateDataStore([&](DataStore& store) {
		store.SetFieldName(def->ParentRecord->Name(), old_name, new_name);
	});

	/// Save
	SaveAll();

	return success();
}

result<void, string> Database::SetFieldType(Fld def, TypeReference const& type)
{
	/// Validation
	auto result = ValidateFieldType(def, type);
	if (result.has_error())
		return result;

	/// ChangeLog add
	AddChangeLog(json{ {"action", "SetFieldType"}, {"record", def->ParentRecord->Name()}, {"field", def->Name}, {"type", type.ToJSON() }, {"previous", def->FieldType.ToJSON()} });

	/// Schema Change
	mut(def)->FieldType = type;

	/// DataStore update
	/// TODO

	/// Save
	SaveAll();

	return success();
}

result<void, string> Database::SwapFields(Rec def, size_t field_index_a, size_t field_index_b)
{
	/// Validation
	if (field_index_a >= def->mFields.size()) return failure(format("field #{} of record {} does not exist", field_index_a, def->Name()));
	if (field_index_b >= def->mFields.size()) return failure(format("field #{} of record {} does not exist", field_index_b, def->Name()));
	
	/// Schema Change
	swap(mut(def)->mFields[field_index_a], mut(def)->mFields[field_index_b]);
	
	/// DataStore update
	/// No need to update datastore, it doesn't care about field order
	
	/// ChangeLog add
	AddChangeLog(json{ {"action", "SwapFields"}, {"record", def->Name()}, {"field_a", field_index_a}, {"field_b", field_index_b} });

	/// Save
	SaveAll();

	return success();
}

result<void, string> Database::DeleteField(Fld def)
{
	/// Validation
	auto validator = [def](DataStore const& store) -> result<void, string> { 
		if (store.HasFieldData(def->ParentRecord->Name(), def->Name)) 
			return format("field {}.{} has data", def->ParentRecord->Name(), def->Name); 
		return success(); 
	};
	if (auto res = CheckDataStore(validator); res.has_error())
		return move(res).as_failure();

	/// ChangeLog add
	AddChangeLog(json{ {"action", "DeleteField"},  {"record", def->ParentRecord->Name()}, {"field", def->Name}, {"backup", def->ToJSON() } });

	/// DataStore update
	UpdateDataStore([def](DataStore& store) {
		store.DeleteField(def->ParentRecord->Name(), def->Name);
	});

	/// Schema Change
	auto index = def->ParentRecord->FieldIndexOf(def);
	mut(def->ParentRecord)->mFields.erase(def->ParentRecord->mFields.begin() + index);

	/// Save
	SaveAll();

	return success();
}

result<void, string> Database::DeleteType(Def type)
{
	/// Validation
	auto validator = [type](DataStore const& store) -> result<void, string> {
		if (store.HasTypeData(type->Name()))
			return format("type {} has data", type->Name());
		return success();
	};
	if (auto res = CheckDataStore(validator); res.has_error())
		return move(res).as_failure();

	/// ChangeLog add
	AddChangeLog(json{ {"action", "DeleteType"}, {"type", type->Name()}, {"backup", type->ToJSON() } });

	/// DataStore update
	UpdateDataStore([type](DataStore& store) {
		store.DeleteType(type->Name());
	});

	/// Schema Change
	auto it = ranges::find_if(mSchema.Definitions, [type](auto& def) { return def.get() == type; });
	mSchema.Definitions.erase(it);

	/// Save
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

	mVoid = AddNative("void", "::DataModel::NativeTypes::Void");
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
		TemplateParameter{ "ELEMENT_TYPE", TemplateParameterQualifier::NotClass, TemplateParameterFlags::CanBeIncomplete }
	}, true);
	auto arr = AddNative("array", "::DataModel::NativeTypes::Array", vector{
		TemplateParameter{ "ELEMENT_TYPE", TemplateParameterQualifier::NotClass },
		TemplateParameter{ "SIZE", TemplateParameterQualifier::Size }
	}, true);
	auto ref = AddNative("ref", "::DataModel::NativeTypes::Ref", vector{
		TemplateParameter{ "POINTEE", TemplateParameterQualifier::Class, TemplateParameterFlags::CanBeIncomplete }
	}, true);
	auto own = AddNative("own", "::DataModel::NativeTypes::Own", vector{
		TemplateParameter{ "POINTEE", TemplateParameterQualifier::Class, TemplateParameterFlags::CanBeIncomplete }
	}, true);

	auto variant = AddNative("variant", "::DataModel::NativeTypes::Variant", vector{
		TemplateParameter{ "TYPES", TemplateParameterQualifier::NotClass, TemplateParameterFlags::Multiple }
	}, true);

	AddFormatPlugin(make_unique<JSONSchemaFormat>());
	AddFormatPlugin(make_unique<CppDeclarationFormat>());

	mDataStores.emplace("main", DataStore(*this));

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
	/// TODO: This
	/*
	auto result = ValidateAll();
	if (result.has_error())
		return result;
		*/

	for (auto& [name, plugin] : mFormatPlugins)
	{
		ghassanpl::save_text_file(mDirectory / plugin->ExportFileName(), plugin->Export(*this));
	}

	for (auto& [name, store] : mDataStores)
	{
		std::ofstream out{ mDirectory / format("{}_datastore.wilson", name) };
		out.exceptions(std::ios::badbit | std::ios::failbit);
		to_wilson_stream(out, store.Storage);
	}

	mChangeLog.flush();
}

BuiltinDefinition const* Database::AddNative(string name, string native_name, vector<TemplateParameter> params, bool markable)
{
	return AddType<BuiltinDefinition>(move(name), move(native_name), move(params), markable);
}

result<void, string> Database::CheckDataStore(function<result<void, string>(DataStore const&)> validate_safety_func)
{
	map<string, string> safety_concerns;
	for (auto& [name, store] : mDataStores)
	{
		if (auto result = validate_safety_func(store); result.has_error())
		{
			safety_concerns[name] = result.error();
		}
	}

	if (!safety_concerns.empty())
	{
		string msg = format("There is data in the data stores which will be lost or corrupted if you perform this change:\n\n{}\n\nAre you sure you want to perform this change?",
			string_ops::join(safety_concerns, "\n", [](auto&& kvp) { return format("Data store '{}': {}", kvp.first, kvp.second); }));
		if (!msg::confirm(msg))
			return failure("canceled");
	}

	return success();
}

void Database::UpdateDataStore(function<void(DataStore&)> update_func)
{
	for (auto& [name, store] : mDataStores)
	{
		update_func(store);
	}
}

json Database::SaveSchema() const
{
	json result = json::object();
	result["version"] = 1;
	{
		auto& types = result["types"] = json::object();
		for (auto& type : mSchema.Definitions)
		{
			if (!type->IsBuiltIn())
				types[type->Name()] = magic_enum::enum_name(type->Type());
		}
	}

	{
		auto& types = result["typedesc"] = json::object();
		for (auto& type : mSchema.Definitions)
		{
			if (!type->IsBuiltIn())
				types[type->Name()] = type->ToJSON();
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

	std::erase_if(mSchema.Definitions, [](auto& type) { return !type->IsBuiltIn(); });

	for (auto&& [name, type] : schema.at("types").get_ref<json::object_t const&>())
	{
		//auto& type_def = mSchema.Definitions.emplace_back();
		auto type_type = magic_enum::enum_cast<DefinitionType>(type.get_ref<json::string_t const&>()).value();
		switch (type_type)
		{
		case DefinitionType::Class: AddType<ClassDefinition>(name); break;//type_def = unique_ptr<ClassDefinition>{ new ClassDefinition(name) }; break;
		case DefinitionType::Struct: AddType<StructDefinition>(name); break;//type_def = unique_ptr<StructDefinition>{ new StructDefinition(name) }; break;
		case DefinitionType::Enum: AddType<EnumDefinition>(name); break;//type_def = unique_ptr<EnumDefinition>{ new EnumDefinition(name) }; break;
		default:
			throw std::runtime_error(format("invalid type definition type: {}", type.get_ref<json::string_t const&>()));
		}
	}

	for (auto&& [name, typedesc] : schema.at("typedesc").get_ref<json::object_t const&>())
	{
		auto type_def = mSchema.ResolveType(name);
		if (!type_def)
			throw std::runtime_error(format("invalid type defined: {}", name));
		type_def->FromJSON(mSchema, typedesc);
	}
}

void Database::AddFormatPlugin(unique_ptr<FormatPlugin> plugin)
{
	if (!plugin) return;
	auto name = plugin->FormatName();
	mFormatPlugins[name] = move(plugin);
}
