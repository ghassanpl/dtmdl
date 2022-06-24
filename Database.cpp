#include "pch.h"

#include "Database.h"
#include "Validation.h"

#include <ghassanpl/wilson.h>
#include <kubazip/zip/zip.h>

string Describe(TypeUsedInFieldType const& usage)
{
	auto field = usage.Field;
	return format("Type is used in field '{}' of type '{}': {}", field->Name, field->ParentRecord->Name(), field->ToString());
}
string Describe(TypeIsBaseTypeOf const& usage)
{
	return format("Type is the base type of type '{}'", usage.ChildType->Name());
}

string Describe(TypeHasDataInDataStore const& usage)
{
	return format("Type has data stored in store named '{}'", usage.StoreName);
}

result<StructDefinition const*, string> Database::AddNewStruct()
{
	/// Validation
	
	/// Schema Change
	auto result = mSchema.AddType<StructDefinition>(FreshName("Struct", [this](string_view sv) { return !!mSchema.ResolveType(sv); }));
	if (!result)
		return result;

	/// DataStore update (v2)
	/// Adding a new struct should not change the data stores

	/// ChangeLog add
	AddChangeLog(json{ {"action", "AddNewStruct"}, {"name", result->Name()} });
	
	/// Save
	SaveAll();

	return success();
}

result<EnumDefinition const*, string> Database::AddNewEnum()
{
	/// Validation
	
	/// Schema Change
	auto result = mSchema.AddType<EnumDefinition>(FreshName("Enum", [this](string_view sv) { return !!mSchema.ResolveType(sv); }));
	if (!result)
		return result;

	/// DataStore update (v2)
	/// Adding a new type should not change the data stores

	/// ChangeLog add
	AddChangeLog(json{ {"action", "AddNewEnum"}, {"name", result->Name()} });

	/// Save
	SaveAll();

	return success();
}

result<void, string> Database::AddNewField(Rec def)
{
	/// Validation

	/// Schema Change
	auto name = FreshName("Field", [def](string_view name) { return !!def->OwnOrBaseField(name); });
	mut(def)->mFields.push_back(make_unique<FieldDefinition>(def, name, TypeReference{ VoidType() }));

	/// DataStore update
	/// Adding a new field to a type should not change the data stores - we treat
	/// a non-existent object entry as the "default value" for that field

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
	auto old_name = exchange(mut(def)->mName, new_name);

	/// DataStore update
	UpdateDataStores([&](DataStore& store) {
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
	UpdateDataStores([&](DataStore& store) {
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
	TypeReference old_type = def->FieldType;
	mut(def)->FieldType = type;

	/// DataStore update
	UpdateDataStores([&](DataStore& store) {
		store.SetFieldType(def->ParentRecord->Name(), def->Name, old_type, type);
	});

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

result<void, string> Database::MoveField(Rec from_record, string_view field_name, Rec to_record)
{
	/// Validation
	if (!from_record || !to_record)
		throw invalid_argument("both records must be non-null");

	if (!Schema().IsParent(from_record, to_record))
		return failure(format("record '{}' must a base type of record '{}'", from_record->Name(), to_record->Name()));

	if (to_record->OwnField(field_name))
		return failure(format("record '{}' already has field '{}'", to_record->Name(), field_name));
	
	auto src_field = from_record->OwnField(field_name);
	if (!src_field)
		return failure(format("record '{}' does not have field '{}'", from_record->Name(), field_name));

	/// DataStore update
	/// Since a record holds the values for all fields (including parent fields)
	/// moving a field should only be a destructive data operation if the types are not related

	/// Schema Change

	mut(to_record)->mFields.push_back(make_unique<FieldDefinition>(to_record, move(mut(src_field)->Name), src_field->FieldType));
	auto it = ranges::find_if(from_record->mFields, [src_field](auto const& f) { return f.get() == src_field; });
	mut(from_record)->mFields.erase(it);

	/// ChangeLog add
	AddChangeLog(json{ {"action", "MoveField"}, {"from_record", from_record->Name()}, {"fieldname", field_name}, { "to_record", to_record->Name() }});

	/// Save
	SaveAll();

	return success();
}

result<void, string> Database::CopyFieldsAndMoveUpBaseTypeHierarchy(Rec def)
{
	if (!def->BaseType())
		return SetRecordBaseType(def, {});

	auto base_type = def->BaseType().Type->AsRecord();
	if (!base_type)
		return SetRecordBaseType(def, {});

	auto result = ValidateRecordBaseType(def, TypeReference{ base_type });
	if (result.has_error())
		return result;

	auto base_field_names = base_type->OwnFieldNames();
	for (auto& base_field_name : base_field_names)
	{
		if (auto result = MoveField(base_type, base_field_name, def); result.has_error())
			return result;
	}

	return SetRecordBaseType(def, base_type->BaseType());
}

result<void, string> Database::DeleteField(Fld def)
{
	/// Validation
	/// So far deleting a field should always be allowed

	/// ChangeLog add
	AddChangeLog(json{ {"action", "DeleteField"},  {"record", def->ParentRecord->Name()}, {"field", def->Name}, {"backup", def->ToJSON() } });

	/// DataStore update
	/// Need to update data stores - if we delete a field and create a new one with the same name, that
	/// will be an issue

	UpdateDataStores([&](DataStore& store) {
		store.DeleteField(def->ParentRecord->Name(), def->Name);
	});

	/// Schema Change
	auto index = def->ParentRecord->FieldIndexOf(def);
	mut(def->ParentRecord)->mFields.erase(def->ParentRecord->mFields.begin() + index);

	/// Save
	SaveAll();

	return success();
}

template <typename CALLBACK>
void LocateTypeReference(CALLBACK&& callback, TypeDefinition const* type, TypeReference& start_reference, vector<size_t> ref = {})
{
	if (start_reference.Type == type)
		callback(ref);

	size_t i = 0;
	for (auto& templ : start_reference.TemplateArguments)
	{
		if (auto arg = get_if<TypeReference>(&templ))
		{
			ref.push_back(i);
			LocateTypeReference(callback, type, *arg, ref);
			ref.pop_back();
		}
		++i;
	}
}

void LocateTypeReference(vector<TypeUsage>& usage_list, TypeDefinition const* to_type, FieldDefinition* in_field)
{
	LocateTypeReference([&usage_list, in_field](vector<size_t> ref) {
		auto existing = ranges::find_if(usage_list, [in_field](auto const& usage) {
			if (auto tr = get_if<TypeUsedInFieldType>(&usage); tr && tr->Field == in_field)
				return true;
			return false;
			});
		if (existing == usage_list.end())
			usage_list.push_back(TypeUsedInFieldType{ in_field, { move(ref) } });
		else
			get<TypeUsedInFieldType>(*existing).References.push_back(move(ref));
		}, to_type, in_field->FieldType);
}

vector<TypeUsage> Database::LocateTypeUsages(Def type) const
{
	vector<TypeUsage> usage_list;

	for (auto def : mSchema.Definitions())
	{
		if (auto record = def->AsRecord())
		{
			if (record->BaseType().Type == type)
				usage_list.push_back(TypeIsBaseTypeOf{ record });

			for (auto& field : record->Fields())
				LocateTypeReference(usage_list, type, field.get());
		}
	}

	for (auto& [name, store] : mDataStores)
	{
		if (store.HasTypeData(type->Name()))
			usage_list.push_back(TypeHasDataInDataStore{ name });
	}

	return usage_list;
}

vector<string> Database::StoresWithFieldData(Fld field) const
{
	vector<string> result;
	ranges::copy(
		mDataStores
			| views::filter([field](auto& kvp) { return kvp.second.HasFieldData(field->ParentRecord->Name(), field->Name); })
			| views::transform([](auto& kvp) { return kvp.first; }),
		back_inserter(result));
	return result;
}

result<void, string> Database::DeleteType(Def type)
{
	/// Validation
	{
		auto usages = LocateTypeUsages(type);

		/// We can delete a type if it has data in storage
		std::erase_if(usages, [](TypeUsage const& usage) { return holds_alternative<TypeHasDataInDataStore>(usage); });

		/// If there are some other reasons, we can't delete the type
		if (!usages.empty())
			return failure(format("cannot delete type due to following reasons:\n{}", string_ops::join(usages, "\n", [this](TypeUsage const& usage) { return Describe(usage); })));
	}

	/// ChangeLog add
	AddChangeLog(json{ {"action", "DeleteType"}, {"type", type->Name()}, {"backup", type->ToJSON() } });

	/// DataStore update
	UpdateDataStores([type](DataStore& store) {
		store.DeleteType(type->Name());
	});

	/// Schema Change
	auto it = ranges::find_if(mSchema.mDefinitions, [type](auto& def) { return def.get() == type; });
	mSchema.mDefinitions.erase(it);

	/// Save
	SaveAll();

	return success();
}

Database::Database(filesystem::path dir)
{
	if (filesystem::exists(dir) && !filesystem::is_directory(dir))
		throw std::invalid_argument("argument must be a directory");

	error_code ec;
	filesystem::create_directories(dir, ec);
	if (ec)
		throw std::invalid_argument("could not create target directory");

	mDirectory = canonical(move(dir));

	mChangeLog.open(mDirectory / "changelog.wilson", ios::app|ios::out);

	AddFormatPlugin(make_unique<JSONSchemaFormat>());
	AddFormatPlugin(make_unique<CppDeclarationFormat>());

	mDataStores.emplace("main", DataStore(mSchema));
	
	LoadAll();
	SaveAll();
}

void Database::AddChangeLog(json log)
{
	log["timestamp"] = format("{}", std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::system_clock::now()});
	mChangeLog << ghassanpl::to_wilson_string(log) << "\n";
}

result<void, string> Database::CreateBackup()
{
	return CreateBackup(mDirectory);
}

result<void, string> Database::CreateBackup(filesystem::path in_directory)
{
	mChangeLog.flush();
	mChangeLog.close();
	
	vector<string> filenames;
	for (auto it = filesystem::directory_iterator{ absolute(mDirectory) }; it != filesystem::directory_iterator{}; ++it)
	{
		if (it->path().extension() != ".zip")
			filenames.push_back(it->path().string());
	}

	auto zip_path = (in_directory / format("backup_{}_{:%F-%Hh%Mm%Ss%z}.zip", mDirectory.filename().string(), chrono::zoned_time{chrono::current_zone(), chrono::system_clock::now()})).string();
	vector<const char*> filename_cstrs;
	ranges::transform(filenames, back_inserter(filename_cstrs), [](string const& s) { return s.c_str(); });
	auto error = zip_create(zip_path.c_str(), filename_cstrs.data(), filename_cstrs.size());

	mChangeLog.open(mDirectory / "changelog.wilson", ios::app | ios::out);

	AddChangeLog({ {"action", "Backup" }, {"filename", zip_path} });

	if (error == 0)
		return success();
	return failure("zipping backup file failed");
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
		save_ubjson_file(mDirectory / format("{}.datastore", name), store.Storage());
	}

	mChangeLog.flush();
}

void Database::LoadAll()
{
	if (filesystem::exists(mDirectory / "schema.json"))
	{
		LoadSchema(ghassanpl::load_json_file(mDirectory / "schema.json"));
	}

	for (auto it = filesystem::directory_iterator{ mDirectory }; it != filesystem::directory_iterator{}; ++it)
	{
		auto& path = it->path();
		if (path.extension() == ".datastore")
		{
			mDataStores.erase(path.stem().string());
			mDataStores.insert({ path.stem().string(), DataStore{mSchema, try_load_ubjson_file(path)} });
		}
	}
}

void Database::UpdateDataStores(function<void(DataStore&)> update_func)
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
		for (auto type : mSchema.Definitions())
		{
			if (!type->IsBuiltIn())
				types[type->Name()] = magic_enum::enum_name(type->Type());
		}
	}

	{
		auto& types = result["typedesc"] = json::object();
		for (auto type : mSchema.Definitions())
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

	std::erase_if(mSchema.mDefinitions, [](auto& type) { return !type->IsBuiltIn(); });

	for (auto&& [name, type] : schema.at("types").get_ref<json::object_t const&>())
	{
		auto type_type = magic_enum::enum_cast<DefinitionType>(type.get_ref<json::string_t const&>()).value();
		switch (type_type)
		{
		case DefinitionType::Class: mSchema.AddType<ClassDefinition>(name); break;
		case DefinitionType::Struct: mSchema.AddType<StructDefinition>(name); break;
		case DefinitionType::Enum: mSchema.AddType<EnumDefinition>(name); break;
		default:
			throw std::runtime_error(format("invalid type definition type: {}", type.get_ref<json::string_t const&>()));
		}
	}

	for (auto&& [name, typedesc] : schema.at("typedesc").get_ref<json::object_t const&>())
	{
		auto type_def = mSchema.ResolveType(name);
		if (!type_def)
			throw std::runtime_error(format("invalid type defined: {}", name));
		type_def->FromJSON(typedesc);
	}
}

void Database::AddFormatPlugin(unique_ptr<FormatPlugin> plugin)
{
	if (!plugin) return;
	auto name = plugin->FormatName();
	mFormatPlugins[name] = move(plugin);
}
