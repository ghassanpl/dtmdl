#pragma once

#include "Schema.h"
#include "Formats.h"
#include "DataStore.h"

namespace dtmdl
{

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
		using Cls = ClassDefinition const*;
		using Str = StructDefinition const*;
		using Enum = EnumDefinition const*;
		using Enumerator = EnumeratorDefinition const*;

		auto Definitions() const noexcept { return mSchema.Definitions(); }
		auto UserDefinitions() const noexcept { return mSchema.UserDefinitions(); }
		auto Classes() const noexcept { return mSchema.Definitions() | views::filter([](auto def) { return def->IsClass(); }) | views::transform([](auto def) { return (ClassDefinition const*)def; }); }
		auto Structs() const noexcept { return mSchema.Definitions() | views::filter([](auto def) { return def->IsStruct(); }) | views::transform([](auto def) { return (StructDefinition const*)def; }); }

		//TypeDefinition const* ResolveType(string_view name) const;

		template <typename T>
		inline constexpr static T* mut(T const* v) noexcept { return const_cast<T*>(v); }

		//string FreshTypeName(string_view base) const;

		/// Validations

		result<void, string> ValidateRecordBaseType(Rec def, TypeReference const& type);
		result<void, string> ValidateTypeName(Def def, string const& new_name);
		result<void, string> ValidateFieldName(Fld def, string const& new_name);
		result<void, string> ValidateEnumeratorName(Enumerator def, string const& new_name);
		result<void, string> ValidateClassFlags(Cls def, enum_flags<ClassFlags> flags);
		result<void, string> ValidateFieldFlags(Fld def, enum_flags<FieldFlags> flags) { return success(); }
		result<void, string> ValidateStructFlags(Str def, enum_flags<StructFlags> flags);

		vector<TypeUsage> LocateTypeUsages(Def type) const;

		vector<string> StoresWithFieldData(Fld field) const;
		vector<string> StoresWithEnumeratorData(Enumerator field) const;

		/// Actions

		result<void, string> SetTypeName(Def def, string const& new_name);

		result<StructDefinition const*, string> AddNewStruct();
		result<ClassDefinition const*, string> AddNewClass();
		result<void, string> AddNewField(Rec def);

		result<void, string> SetRecordBaseType(Rec def, TypeReference const& type);
		result<void, string> SetFieldName(Fld def, string const& new_name);
		result<void, string> SetFieldType(Fld def, TypeReference const& type);
		result<void, string> SetFieldFlags(Fld def, enum_flags<FieldFlags> flags);
		result<void, string> SetClassFlags(Cls def, enum_flags<ClassFlags> flags);
		result<void, string> SetStructFlags(Str def, enum_flags<StructFlags> flags);

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

		result<EnumDefinition const*, string> AddNewEnum();
		result<void, string> AddNewEnumerator(Enum def);
		result<void, string> SwapEnumerators(Enum def, size_t enum_index_a, size_t enum_index_b);
		result<void, string> DeleteEnumerator(Enumerator def);
		result<void, string> SetEnumeratorName(Enumerator def, string const& new_name);
		result<void, string> SetEnumeratorDescriptiveName(Enumerator def, string const& new_name);
		result<void, string> SetEnumeratorValue(Enumerator def, optional<int64_t> value);

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

		auto VoidType() const noexcept { return mSchema.VoidType(); }

		//string Namespace;
		string PrivateFieldPrefix = "m";

	private:

		filesystem::path mDirectory;
		ofstream mChangeLog;
		dtmdl::Schema mSchema;
		map<string, DataStore, less<>> mDataStores;

		json Save() const;
		void Load(json const& j);

		void AddFormatPlugin(unique_ptr<FormatPlugin> plugin);
		map<string, unique_ptr<FormatPlugin>, less<>> mFormatPlugins;

		void AddChangeLog(json log);

		json SaveSchema() const;
		void LoadSchema(json const& from);

		void UpdateDataStores(function<void(DataStore&)> update_func);
	};

}
extern unique_ptr<dtmdl::Database> mCurrentDatabase;