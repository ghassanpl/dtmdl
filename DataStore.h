#pragma once

struct Database;
struct TypeReference;

struct DataStore
{
	DataStore(Database const& db) : mDB(db) {}
	DataStore(Database const& db, json storage) : mDB(db), mStorage(move(storage)) {}

	auto const& Storage() const noexcept { return mStorage; }

	void SetTypeName(string_view old_name, string_view new_name);
	void SetFieldName(string_view record, string_view old_name, string_view new_name);

	bool HasFieldData(string_view record, string_view name) const;
	void DeleteField(string_view record, string_view name);

	bool HasTypeData(string_view type_name) const;
	void DeleteType(string_view type_name);

	bool HasValue(string_view name) const;
	//void AddValue(json value);

	void ForEveryRoot(function<bool(string_view, TypeReference const&, json&)>);

private:

	bool ForEveryObjectWithTypeName(string_view type_name, function<bool(json&)> const& object_func);
	bool ForEveryObjectWithTypeName(string_view type_name, function<bool(json const&)> const& object_func) const;

	Database const& mDB;
	json mStorage = json::object({
		{ "format", "json-simple-v1" },
		{ "gcheap", json::array() }, 
		{ "roots", json::object() },
		{ "schema", "undefined" }
	});
};

/*
struct DataStore
{
	Database const& DB;
	json Storage;

	DataStore(Database const& db);
	DataStore(Database const& db, json storage);

	/// The functions below assume the schema is correct

	void AddNewStruct(string_view name);
	void AddNewField(string_view record, string_view field);
	void EnsureField(string_view record, string_view field);

	bool HasFieldData(string_view record, string_view name) const;
	void DeleteField(string_view record, string_view name);

	bool HasTypeData(string_view type_name) const;
	void DeleteType(string_view type_name);

	bool HasValue(string_view name) const;
	void AddValue(json value);

	decltype(auto) Roots()
	{
		return Storage.at("roots");
	}

	/// ////////////////////////////////////// ///


	void ViewValue(TypeReference const& type, json& value, json const& field_attributes);
	bool EditValue(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer value_path);

	enum class ConversionResult
	{
		ConversionImpossible,
		DataPreserved,
		DataCorrupted,
		DataLost,
	};

	ConversionResult ResultOfConversion(json const& value, TypeReference const& from, TypeReference const& to);
	result<void, string> Convert(json& value, TypeReference const& from, TypeReference const& to);
	result<void, string> InitializeValue(TypeReference const& type, json& value);

private:

	using EditorFunction = bool(DataStore&, TypeReference const&, json&, json const&, json::json_pointer);
	using ViewFunction = void(DataStore&, TypeReference const&, json const&, json const&);
	using ConversionFunction = result<void, string>(DataStore&, json&, TypeReference const&, TypeReference const&);
	using InitializationFunction = result<void, string>(DataStore&, TypeReference const&, json&);

	struct BuiltinTypeHandler
	{
		function<EditorFunction> EditorFunc;
		function<ViewFunction> ViewFunc;
		function<InitializationFunction> InitializationFunc;
		map<string, function<ConversionFunction>, less<>> ConversionFuncs;
	};

	map<string, BuiltinTypeHandler, less<>> mBuiltIns;

	bool EditVoid(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditF32(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditF64(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditI8(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditI16(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditI32(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditI64(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditU8(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditU16(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditU32(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditU64(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditBool(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditString(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditBytes(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditFlags(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditList(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditArray(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditRef(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditOwn(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);
	bool EditVariant(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path);

	result<void, string> InitializeVoid(TypeReference const& type, json& value);
	result<void, string> InitializeF32(TypeReference const& type, json& value);
	result<void, string> InitializeF64(TypeReference const& type, json& value);
	result<void, string> InitializeI8(TypeReference const& type, json& value);
	result<void, string> InitializeI16(TypeReference const& type, json& value);
	result<void, string> InitializeI32(TypeReference const& type, json& value);
	result<void, string> InitializeI64(TypeReference const& type, json& value);
	result<void, string> InitializeU8(TypeReference const& type, json& value);
	result<void, string> InitializeU16(TypeReference const& type, json& value);
	result<void, string> InitializeU32(TypeReference const& type, json& value);
	result<void, string> InitializeU64(TypeReference const& type, json& value);
	result<void, string> InitializeBool(TypeReference const& type, json& value);
	result<void, string> InitializeString(TypeReference const& type, json& value);
	result<void, string> InitializeBytes(TypeReference const& type, json& value);
	result<void, string> InitializeFlags(TypeReference const& type, json& value);
	result<void, string> InitializeList(TypeReference const& type, json& value);
	result<void, string> InitializeArray(TypeReference const& type, json& value);
	result<void, string> InitializeRef(TypeReference const& type, json& value);
	result<void, string> InitializeOwn(TypeReference const& type, json& value);
	result<void, string> InitializeVariant(TypeReference const& type, json& value);

	void ViewVoid(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewF32(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewF64(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewI8(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewI16(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewI32(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewI64(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewU8(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewU16(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewU32(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewU64(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewBool(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewString(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewBytes(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewFlags(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewList(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewArray(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewRef(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewOwn(TypeReference const& type, json const& value, json const& field_attributes);
	void ViewVariant(TypeReference const& type, json const& value, json const& field_attributes);

	template <typename FUNC>
	void Do(TypeReference const& type, json& value, FUNC&& func);

	void InitializeHandlers();

	bool IsVoid(TypeReference const& ref) const;

	template <typename JSON_TYPE, typename FUNC>
	bool EditScalar(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path, FUNC&& func);

	void LogDataChange(json::json_pointer const& value_path, json const& from, json const& value);
};

*/