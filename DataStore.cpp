#include "pch.h"

#include "DataStore.h"
#include "Database.h"
#include "Values.h"

/*
DataStore::DataStore(Database const& db)
	: DB(db)
{
	Storage = json::object({
		{ "format", "json-tool-editable-v1" },
		{ "gcheap", json::object() },
		{ "roots", json::array() },
		{ "schema", json::object({
			{ "uri", db.Directory().string() },
			{ "version", db.Schema().Version() }, /// TODO: These values are not updated when we change the schema in the editor!
			{ "hash", db.Schema().Hash() }
		})},
		{ "fielddata", json::object() }
	});

	InitializeHandlers();
}

DataStore::DataStore(Database const& db, json storage)
	: DB(db) 
	, Storage(move(storage))
{
	InitializeHandlers();
}

void DataStore::AddNewStruct(string_view name)
{
	Storage.at("fielddata")[string{name}] = json::object();
}

void DataStore::AddNewField(string_view record, string_view field)
{
	Storage.at("fielddata").find(record)->operator[](string{ field }) = json::array({ json{} });
}

void DataStore::EnsureField(string_view record, string_view field)
{
	auto record_data = Storage.at("fielddata").find(record);
	if (auto field_data = record_data->find(field); field_data == record_data->end())
		record_data->operator[](string{ field }) = json::array({ json{} });
}

void DataStore::SetTypeName(string_view old_name, string_view new_name)
{
	auto& field_data = Storage.at("fielddata");
	json old_data = move(*field_data.find(old_name));
	field_data[string{ new_name }] = move(old_data);
	field_data.erase(field_data.find(old_name));
}

void DataStore::SetFieldName(string_view record, string_view old_name, string_view new_name)
{
	auto& record_data = *Storage.at("fielddata").find(record);
	json old_field_data = move(*record_data.find(old_name));
	record_data[string{ new_name }] = move(old_field_data);
	record_data.erase(record_data.find(old_name));
}

bool DataStore::HasFieldData(string_view record, string_view field_name) const
{
	/// field_data[0] for every field is always the "default value" and is always present
	return Storage.at("fielddata").find(record)->find(field_name)->size() > 1;
}

void DataStore::DeleteField(string_view record, string_view name)
{
	auto record_data = Storage.at("fielddata").find(record);
	record_data->erase(record_data->find(name));
}

bool DataStore::HasTypeData(string_view type_name) const
{
	auto& field_data = Storage.at("fielddata");
	auto record_data = Storage.at("fielddata").find(type_name);
	if (record_data == field_data.end())
		return false;

	for (auto& [field, data] : record_data->items())
	{
		if (data.size() > 1)
			return true;
	}
	return false;
}

void DataStore::DeleteType(string_view type_name)
{
	auto& field_data = Storage.at("fielddata");
	if (auto it = field_data.find(type_name); it != field_data.end())
		field_data.erase(it);
}

bool DataStore::HasValue(string_view name) const
{
	for (auto& value : Storage.at("roots"))
	{
		if (!value.is_object())
			continue;
		if (auto it = value.find("name"); it != value.end())
			if (it->is_string() && it->get_ref<string const&>() == name)
				return true;
	}
	return false;
}

void DataStore::AddValue(json value)
{
	Storage.at("roots").push_back(move(value));
}

*/

void DataStore::SetTypeName(string_view old_name, string_view new_name)
{
	throw 0;
}

void DataStore::SetFieldName(string_view record, string_view old_name, string_view new_name)
{
	this->ForEveryObjectWithTypeName(record, [=](json& record_data) {
		json old_field_data = move(*record_data.find(old_name));
		record_data[string{ new_name }] = move(old_field_data);
		record_data.erase(record_data.find(old_name));
		return false;
	});
}

bool DataStore::HasFieldData(string_view record, string_view name) const
{
	return this->ForEveryObjectWithTypeName(record, [=](json const& record_data) {
		return record_data.find(name) != record_data.end();
	});
}

void DataStore::DeleteField(string_view record, string_view name)
{
	this->ForEveryObjectWithTypeName(record, [=](json& record_data) {
		if (auto it = record_data.find(name); it != record_data.end())
			record_data.erase(it);
		return false;
	});
}

bool DataStore::HasTypeData(string_view type_name) const
{
	return this->ForEveryObjectWithTypeName(type_name, [=](json const& record_data) {
		return true;
	});
}

void DataStore::DeleteType(string_view type_name)
{
	throw 0;
}

bool DataStore::HasValue(string_view name) const
{
	return mStorage.at("roots").contains(name);
}

/*
void DataStore::ForEveryRoot(function<void(string_view name, TypeReference const& type, json& value)> func)
{
	for (auto&& item : mStorage.at("roots").items())
	{
		string current_name = item.key();
		TypeReference current_type{ mDB.Schema(), item.value().at("type")};
		json& current_value = item.value().at("value");

		if (func(current_name, current_type, current_value))
		{
			item.at("name") = current_name;
			item.at("type") = current_type.ToJSON();
		}
	}
}
*/

bool DataStore::ForEveryObjectWithTypeName(string_view type_name, function<bool(json&)> const& object_func)
{
	for (auto&& item : mStorage.at("roots").items())
	{
		//string current_name = item.at("name");
		TypeReference current_type{ mDB.Schema(), item.value().at("type") };
		json& current_value = item.value().at("value");

		if (::ForEveryObjectWithTypeName(mDB.Schema(), current_type, current_value, type_name, object_func))
			return true;
	}
	return false;
}

bool DataStore::ForEveryObjectWithTypeName(string_view type_name, function<bool(json const&)> const& object_func) const
{
	for (auto&& item : mStorage.at("roots").items())
	{
		TypeReference current_type{ mDB.Schema(), item.value().at("type") };
		json const& current_value = item.value().at("value");

		if (::ForEveryObjectWithTypeName(mDB.Schema(), current_type, current_value, type_name, object_func))
			return true;
	}
	return false;
}
