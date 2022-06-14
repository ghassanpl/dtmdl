#pragma once

struct Database;

struct DataStore
{
	json Storage;

	DataStore(Database const& db);

	/// The functions below assume the schema is correct

	void AddNewStruct(string_view name);
	void AddNewField(string_view record, string_view field);

	void SetTypeName(string_view old_name, string_view new_name);
	void SetFieldName(string_view record, string_view old_name, string_view new_name);

	bool HasFieldData(string_view record, string_view name) const;
	void DeleteField(string_view record, string_view name);

	bool HasTypeData(string_view type_name) const;
	void DeleteType(string_view type_name);
};
