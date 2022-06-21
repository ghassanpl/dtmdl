#pragma once
#include <fstream>
#include "Schema.h"
#include "Formats.h"
#include "DataStore.h"

struct TypeUsedInFieldType
{
	FieldDefinition* Field;
	vector<vector<size_t>> References;
};

struct TypeIsBaseTypeOf
{
	RecordDefinition const* ChildType;
};

struct TypeHasDataInDataStore
{
	string StoreName;
};

using TypeUsage = std::variant<TypeUsedInFieldType, TypeIsBaseTypeOf, TypeHasDataInDataStore>;

string Describe(TypeUsedInFieldType const& usage);
string Describe(TypeIsBaseTypeOf const& usage);
string Describe(TypeHasDataInDataStore const& usage);
inline
string Describe(TypeUsage const& usage) { return visit([](auto const& usage) { return Describe(usage); }, usage); }

struct Database
{
	using Def = TypeDefinition const*;
	using Rec = RecordDefinition const*;
	using Fld = FieldDefinition const*;

	auto Definitions() const noexcept { return mSchema.Definitions(); }

	//TypeDefinition const* ResolveType(string_view name) const;

	template <typename T>
	inline constexpr static T* mut(T const* v) noexcept { return const_cast<T*>(v); }

	//string FreshTypeName(string_view base) const;

	/// Validations

	result<void, string> ValidateRecordBaseType(Rec def, TypeReference const& type);
	result<void, string> ValidateTypeName(Def def, string const& new_name);
	result<void, string> ValidateFieldName(Fld def, string const& new_name);
	result<void, string> ValidateFieldType(Fld def, TypeReference const& type);

	vector<TypeUsage> LocateTypeUsages(Def type) const;

	vector<string> StoresWithFieldData(Fld field) const;

	/// Actions

	result<StructDefinition const*, string> AddNewStruct();
	result<void, string> AddNewField(Rec def);

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

	result<void, string> MoveField(Rec from_record, string_view field_name, Rec to_record);

	result<void, string> CopyFieldsAndMoveUpBaseTypeHierarchy(Rec def);

	result<void, string> DeleteField(Fld def);
	result<void, string> DeleteType(Def type);

	/// Database Operations
	Database(filesystem::path dir);

	void SaveAll();
	void LoadAll();
	result<void, string> CreateBackup();
	result<void, string> CreateBackup(filesystem::path in_directory);

	/// Accessors and Queries

	auto const& Directory() const noexcept { return mDirectory; }
	auto const& Schema() const noexcept { return mSchema; }
	auto& DataStores() noexcept { return mDataStores; }

	auto VoidType() const noexcept { return mVoid; }

	static bool IsParent(Def parent, Def potential_child);

	string Namespace;

private:

	filesystem::path mDirectory;

	void AddFormatPlugin(unique_ptr<FormatPlugin> plugin);
	map<string, unique_ptr<FormatPlugin>, less<>> mFormatPlugins;

	ofstream mChangeLog;
	void AddChangeLog(json log);

	json SaveSchema() const;
	void LoadSchema(json const& from);

	template <typename T, typename... ARGS>
	T const* AddType(ARGS&&... args)
	{
		auto ptr = unique_ptr<T>(new T{ forward<ARGS>(args)... });
		auto result = ptr.get();
		mSchema.mDefinitions.push_back(move(ptr));
		return result;
	}

	BuiltinDefinition const* AddNative(string name, string native_name, vector<TemplateParameter> params, bool markable, ghassanpl::enum_flags<TemplateParameterQualifier> applicable_qualifiers);

	BuiltinDefinition const* mVoid = nullptr;

	::Schema mSchema;
	map<string, DataStore, less<>> mDataStores;

	void UpdateDataStores(function<void(DataStore&)> update_func);
};
