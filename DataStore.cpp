#include "pch.h"

#include "DataStore.h"
#include "Database.h"
#include "Values.h"

void DataStore::SetTypeName(string_view old_name, string_view new_name)
{
	/// NOTE: Add this point, the database/schema has done everything it could
	/// to remove any reference to the old name, so the only place
	/// it could have been left is the root table

	for (auto&& item : mStorage.at("roots").items())
	{
		auto& type = item.value().at("type");
		if (type.get_ref<json::string_t&>() == old_name)
			type = new_name;
	}
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

void DataStore::SetFieldType(string_view record, string_view field, TypeReference const& old_type, TypeReference const& new_type)
{
	this->ForEveryObjectWithTypeName(record, [=](json& record_data) {
		if (auto it = record_data.find(field); it != record_data.end())
		{
			if (!Convert(old_type, new_type, *it) && !InitializeValue(new_type, *it))
				record_data.erase(it);
		}
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
	/// NOTE: Add this point, the database/schema has done everything it could
	/// to remove any fields or field data with this type, so the only place
	/// it could have been left is the root table
	erase_if(mStorage.at("roots").get_ref<json::object_t&>(), [this, type_name](auto& kvp) { 
		TypeReference ref{ mSchema, kvp.second.at("type") };
		return ref->Name() == type_name;
	});
}

bool DataStore::HasValue(string_view name) const
{
	return mStorage.at("roots").contains(name);
}

bool DataStore::ForEveryObjectWithTypeName(string_view type_name, function<bool(json&)> const& object_func)
{
	for (auto&& item : mStorage.at("roots").items())
	{
		//string current_name = item.at("name");
		TypeReference current_type{ mSchema, item.value().at("type") };
		json& current_value = item.value().at("value");

		if (::ForEveryObjectWithTypeName(current_type, current_value, type_name, object_func))
			return true;
	}
	return false;
}

bool DataStore::ForEveryObjectWithTypeName(string_view type_name, function<bool(json const&)> const& object_func) const
{
	for (auto&& item : mStorage.at("roots").items())
	{
		TypeReference current_type{ mSchema, item.value().at("type") };
		json const& current_value = item.value().at("value");

		if (::ForEveryObjectWithTypeName(current_type, current_value, type_name, object_func))
			return true;
	}
	return false;
}
