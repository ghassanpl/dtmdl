#pragma once
#include <fstream>
#include "Schema.h"
#include "Formats.h"

struct Database
{
	using Def = TypeDefinition const*;
	using Rec = RecordDefinition const*;
	using Fld = FieldDefinition const*;

	auto const& Definitions() const noexcept { return mSchema.Definitions; }

	TypeDefinition const* ResolveType(string_view name) const;

	template <typename T>
	inline constexpr static T* mut(T const* v) noexcept { return const_cast<T*>(v); }

	string FreshTypeName(string_view base) const;

	/// Actions
	result<StructDefinition const*, string> AddNewStruct();
	result<void, string> AddNewField(Rec def);

	result<void, string> ValidateRecordBaseType(Rec def, TypeReference const& type);
	result<void, string> ValidateTypeName(Def def, string const& new_name);
	result<void, string> ValidateFieldName(Fld def, string const& new_name);
	result<void, string> ValidateFieldType(Fld def, TypeReference const& type);

	result<void, string> SetRecordBaseType(Rec def, TypeReference const& type);
	result<void, string> SetTypeName(Def def, string const& new_name);
	result<void, string> SetFieldName(Fld def, string const& new_name);
	result<void, string> SetFieldType(Fld def, TypeReference const& type);

	result<void, string> SwapFields(Rec def, size_t field_index_a, size_t field_index_b);
	result<void, string> RotateFields(Rec def, size_t field_index, size_t new_position);
	result<void, string> SwapFields(Fld field_a, Fld field_b)
	{
		AssumingNotEqual(field_a->ParentRecord, field_b->ParentRecord);
		return SwapFields(field_a->ParentRecord, field_a->ParentRecord->FieldIndexOf(field_a), field_b->ParentRecord->FieldIndexOf(field_b));
	}

	result<void, string> DeleteField(Fld def);
	result<void, string> DeleteType(Def type);

	bool IsParent(Def parent, Def potential_child);

	Database(filesystem::path dir);

	void SaveAll();

	auto const& Directory() const noexcept { return mDirectory; }
	auto const& Schema() const noexcept { return mSchema; }
	auto const& DataStores() const noexcept { return mDataStores; }

	string Namespace;

private:

	filesystem::path mDirectory;

	void AddFormatPlugin(unique_ptr<FormatPlugin> plugin);
	map<string, unique_ptr<FormatPlugin>, less<>> mFormatPlugins;

	ofstream mChangeLog;
	void AddChangeLog(json log);

	json SaveSchema() const;
	void LoadSchema(json const& from);

	struct DataStore
	{

	};

	map<string, DataStore, less<>> mDataStores;

	template <typename T, typename... ARGS>
	T const* AddType(ARGS&&... args)
	{
		auto ptr = unique_ptr<T>(new T{ forward<ARGS>(args)... });
		auto result = ptr.get();
		mSchema.Definitions.push_back(move(ptr));
		return result;
	}

	TypeDefinition* ResolveType(string_view name);

	BuiltinDefinition const* AddNative(string name, string native_name, vector<TemplateParameter> params = {}, bool markable = false);

	BuiltinDefinition const* mVoid = nullptr;

	::Schema mSchema;
	DataStore mStore;
};
