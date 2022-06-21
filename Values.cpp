#include "pch.h"

#include "Database.h"

#include "imgui.h"
#include "imgui_stdlib.h"

template <typename ... T> struct concat;
template <typename ... Ts, typename ... Us>
struct concat<tuple<Ts...>, tuple<Us...>>
{
	typedef tuple<Ts..., Us...> type;
};

template <typename T, typename U> struct cross_product;

template <typename ...Us>
struct cross_product<tuple<>, tuple<Us...>> {
	typedef tuple<> type;
};

template <typename T, typename ...Ts, typename ...Us>
struct cross_product<tuple<T, Ts...>, tuple<Us...>> {
	typedef typename concat<
		tuple<pair<T, Us>...>,
		typename cross_product<tuple<Ts...>, tuple<Us...>>::type
	>::type type;
};

string name_of(type_identity<float>) { return "f32"; }
string name_of(type_identity<double>) { return "f64"; }
string name_of(type_identity<int8_t>) { return "i8"; }
string name_of(type_identity<int16_t>) { return "i16"; }
string name_of(type_identity<int32_t>) { return "i32"; }
string name_of(type_identity<int64_t>) { return "i64"; }
string name_of(type_identity<uint8_t>) { return "u8"; }
string name_of(type_identity<uint16_t>) { return "u16"; }
string name_of(type_identity<uint32_t>) { return "u32"; }
string name_of(type_identity<uint64_t>) { return "u64"; }
string name_of(type_identity<bool>) { return "bool"; }
string name_of(type_identity<string>) { return "string"; }

void DataStore::InitializeHandlers()
{
	mBuiltIns = {
		{ "void", { &DataStore::EditVoid, &DataStore::ViewVoid, &DataStore::InitializeVoid,  } },

		{ "f32", { &DataStore::EditF32, &DataStore::ViewF32, &DataStore::InitializeF32,  } },
		{ "f64", { &DataStore::EditF64, &DataStore::ViewF64, &DataStore::InitializeF64,  } },
		{ "i8", { &DataStore::EditI8, &DataStore::ViewI8, &DataStore::InitializeI8,  } },
		{ "i16", { &DataStore::EditI16, &DataStore::ViewI16, &DataStore::InitializeI16,  } },
		{ "i32", { &DataStore::EditI32, &DataStore::ViewI32, &DataStore::InitializeI32,  } },
		{ "i64", { &DataStore::EditI64, &DataStore::ViewI64, &DataStore::InitializeI64,  } },
		{ "u8", { &DataStore::EditU8, &DataStore::ViewU8, &DataStore::InitializeU8,  } },
		{ "u16", { &DataStore::EditU16, &DataStore::ViewU16, &DataStore::InitializeU16,  } },
		{ "u32", { &DataStore::EditU32, &DataStore::ViewU32, &DataStore::InitializeU32,  } },
		{ "u64", { &DataStore::EditU64, &DataStore::ViewU64, &DataStore::InitializeU64,  } },

		{ "bool", { &DataStore::EditBool, &DataStore::ViewBool, &DataStore::InitializeBool,  } },
		{ "string", { &DataStore::EditString, &DataStore::ViewString, &DataStore::InitializeString,  } },
		{ "bytes", { &DataStore::EditBytes, &DataStore::ViewBytes, &DataStore::InitializeBytes,  } },
		{ "flags", { &DataStore::EditFlags, &DataStore::ViewFlags, &DataStore::InitializeFlags,  } },
		{ "list", { &DataStore::EditList, &DataStore::ViewList, &DataStore::InitializeList,  } },
		{ "array", { &DataStore::EditArray, &DataStore::ViewArray, &DataStore::InitializeArray,  } },
		{ "ref", { &DataStore::EditRef, &DataStore::ViewRef, &DataStore::InitializeRef,  } },
		{ "own", { &DataStore::EditOwn, &DataStore::ViewOwn, &DataStore::InitializeOwn,  } },
		{ "variant", { &DataStore::EditVariant, &DataStore::ViewVariant, &DataStore::InitializeVariant,  } },
	};

	auto AddNumericConversion = [&]<typename A, typename B>(type_identity<pair<A, B>>) {
		if constexpr (!is_same_v<A, B>)
		{
			mBuiltIns.at(name_of(type_identity<A>{})).ConversionFuncs[name_of(type_identity<B>{})] = [](DataStore&, json& value, TypeReference const&, TypeReference const&) -> result<void, string> {
				value = (B)(A)value;
				return success();
			};
		}
	};
	auto AddConversionForEachPair = [&]<typename... PAIRS>(type_identity<tuple<PAIRS...>>, auto&& conversion_func) {
		(conversion_func(type_identity<PAIRS>{}), ...);
	};
	auto AddConversionForTypeCrossProduct = [&]<typename... ELEMENTS1, typename... ELEMENTS2>(type_identity<tuple<ELEMENTS1...>>, type_identity<tuple<ELEMENTS2...>>, auto&& conversion_func) {
		AddConversionForEachPair(type_identity<typename cross_product<tuple<ELEMENTS1...>, tuple<ELEMENTS2...>>::type>{}, conversion_func);
	};
	using numeric_type_list = tuple<float, double, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t>;
	AddConversionForTypeCrossProduct(type_identity<numeric_type_list>{}, type_identity<numeric_type_list>{}, AddNumericConversion);
	AddConversionForTypeCrossProduct(type_identity<tuple<bool>>{}, type_identity<numeric_type_list>{}, AddNumericConversion);
	AddConversionForTypeCrossProduct(type_identity<numeric_type_list>{}, type_identity<tuple<bool>>{}, AddNumericConversion);

	AddConversionForTypeCrossProduct(type_identity<numeric_type_list>{}, type_identity<tuple<string>>{}, [&]<typename A, typename B>(type_identity<pair<A, B>>) {
		mBuiltIns.at(name_of(type_identity<A>{})).ConversionFuncs[name_of(type_identity<B>{})] = [](DataStore&, json& value, TypeReference const&, TypeReference const&) -> result<void, string> {
			value = ::std::to_string((A)value);
			/// TODO: using to_chars would be faster
			return success();
		};
	});

	AddConversionForTypeCrossProduct(type_identity<tuple<string>>{}, type_identity<numeric_type_list>{}, [&]<typename A, typename B>(type_identity<pair<A, B>>) {
		mBuiltIns.at(name_of(type_identity<A>{})).ConversionFuncs[name_of(type_identity<B>{})] = [](DataStore&, json& value, TypeReference const&, TypeReference const&) -> result<void, string> {
			auto& strref = value.get_ref<json::string_t const&>();
			B dest_value{};
			ignore = from_chars(to_address(begin(strref)), to_address(end(strref)), dest_value);
			value = dest_value;
			return success();
		};
	});

	/// TODO: Add more conversions:
	/// string <-> bytes
	/// bytes <-> list<u8>
	/// list<T> <-> array<T, N>
	/// flags<E> <-> u64
	/// variant<T1, T2, ...> <-> T1/T2/...
	/// variant<T1, T2, ...> <-> variant<U1, U2, ...> 
}

bool DataStore::IsVoid(TypeReference const& ref) const
{
	return ref.Type == DB.VoidType();
}

result<void, string> DataStore::InitializeValue(TypeReference const& type, json& value)
{
	if (!type)
		return failure("no type provided");

	switch (type.Type->Type())
	{
	case DefinitionType::BuiltIn:
		return mBuiltIns.at(type.Type->Name()).InitializationFunc(*this, type, value);
	case DefinitionType::Enum:
		return failure("TODO: cannot initialize enums");
	case DefinitionType::Struct:
		return failure("TODO: cannot initialize structs");
	case DefinitionType::Class:
		return failure("TODO: cannot initialize classes");
	}
	return failure(format("unknown type type: {}", magic_enum::enum_name(type.Type->Type())));
}

DataStore::ConversionResult DataStore::ResultOfConversion(json const& value, TypeReference const& from, TypeReference const& to)
{
	if (!from) return ConversionResult::ConversionImpossible;
	if (!to) return ConversionResult::ConversionImpossible;

	if (from == to)
		return ConversionResult::DataPreserved;

	if (IsVoid(from))
		return ConversionResult::DataPreserved;
	if (IsVoid(to))
		return ConversionResult::DataLost;

	switch (from.Type->Type())
	{
	case DefinitionType::BuiltIn:
	{
		auto& handler = mBuiltIns.at(from.Type->Name());
		if (auto it = handler.ConversionFuncs.find(to->Name()); it != handler.ConversionFuncs.end())
			return ConversionResult::DataCorrupted; /// TODO: the conversion funcs should be pairs (or a single function<result<ConversionResult,string>(..., bool just_check)>)
		return ConversionResult::DataLost;
	}
	case DefinitionType::Enum:
		return ConversionResult::ConversionImpossible;
	case DefinitionType::Struct:
		return ConversionResult::ConversionImpossible;
	case DefinitionType::Class:
		return ConversionResult::ConversionImpossible;
	}
	throw runtime_error(format("unknown type type: {}", magic_enum::enum_name(from.Type->Type())));
}

result<void, string> DataStore::Convert(json& value, TypeReference const& from, TypeReference const& to)
{
	if (!from) return failure("source type is none");
	if (!to) return failure("destination type is none");

	if (from == to)
		return success();

	if (IsVoid(from))
		return InitializeValue(to, value);
	if (IsVoid(to))
	{
		value = {};
		return success();
	}

	switch (from.Type->Type())
	{
	case DefinitionType::BuiltIn:
	{
		auto& handler = mBuiltIns.at(from.Type->Name());
		if (auto it = handler.ConversionFuncs.find(to->Name()); it != handler.ConversionFuncs.end())
			return it->second(*this, value, from, to);
		return InitializeValue(to, value);
	}
	case DefinitionType::Enum:
		return failure("TODO: cannot convert values to enums");
	case DefinitionType::Struct:
		return failure("TODO: cannot convert values to structs");
	case DefinitionType::Class:
		return failure("TODO: cannot convert values to classes");
	}
	return failure(format("unknown type type: {}", magic_enum::enum_name(from.Type->Type())));
}

template <typename FUNC>
void DataStore::Do(TypeReference const& type, json& value, FUNC&& func)
{
	if (!type)
	{
		ImGui::TextColored({ 1,0,0,1 }, "Error: Value has no type");
		return;
	}

	switch (type.Type->Type())
	{
	case DefinitionType::BuiltIn:
		func(mBuiltIns.at(type.Type->Name()), type, value);
		break;
	case DefinitionType::Enum:
		break;
	case DefinitionType::Struct:
		break;
	case DefinitionType::Class:
		break;
	}
}

void DataStore::ViewValue(TypeReference const& type, json& value, json const& field_properties)
{
	//Do(type, value, [&](auto& handlers, TypeReference const& type, json& value) { handlers.ViewFunc(*this, type, value, field_properties); });
}

bool DataStore::EditValue(TypeReference const& type, json& value, json const& field_properties, json::json_pointer value_path)
{
	/*
	Do(type, value, [&](auto& handlers, TypeReference const& type, json& value) {
		//handlers.ViewFunc(*this, type, value, field_properties);
		handlers.EditorFunc(*this, type, value, field_properties); 
	});
	*/
	if (!type)
	{
		ImGui::TextColored({ 1,0,0,1 }, "Error: Value has no type");
		return false;
	}

	switch (type.Type->Type())
	{
	case DefinitionType::BuiltIn:
		return mBuiltIns.at(type.Type->Name()).EditorFunc(*this, type, value, field_properties, move(value_path));
	case DefinitionType::Enum:
		break;
	case DefinitionType::Struct:
		break;
	case DefinitionType::Class:
		break;
	}

	return false;
}

result<void, string> DataStore::InitializeVoid(TypeReference const& type, json& value) { value = {}; return success(); }
result<void, string> DataStore::InitializeF32(TypeReference const& type, json& value) { value = float{}; return success(); }
result<void, string> DataStore::InitializeF64(TypeReference const& type, json& value) { value = double{}; return success(); }
result<void, string> DataStore::InitializeI8(TypeReference const& type, json& value) { value = int8_t{}; return success(); }
result<void, string> DataStore::InitializeI16(TypeReference const& type, json& value) { value = int16_t{}; return success(); }
result<void, string> DataStore::InitializeI32(TypeReference const& type, json& value) { value = int32_t{}; return success(); }
result<void, string> DataStore::InitializeI64(TypeReference const& type, json& value) { value = int64_t{}; return success(); }
result<void, string> DataStore::InitializeU8(TypeReference const& type, json& value) { value = uint8_t{}; return success(); }
result<void, string> DataStore::InitializeU16(TypeReference const& type, json& value) { value = uint16_t{}; return success(); }
result<void, string> DataStore::InitializeU32(TypeReference const& type, json& value) { value = uint32_t{}; return success(); }
result<void, string> DataStore::InitializeU64(TypeReference const& type, json& value) { value = uint64_t{}; return success(); }
result<void, string> DataStore::InitializeBool(TypeReference const& type, json& value) { value = bool{}; return success(); }
result<void, string> DataStore::InitializeString(TypeReference const& type, json& value) { value = string{}; return success(); }
result<void, string> DataStore::InitializeBytes(TypeReference const& type, json& value) { value = json::binary_t{}; return success(); }
result<void, string> DataStore::InitializeFlags(TypeReference const& type, json& value) { value = uint64_t{}; return success(); }
result<void, string> DataStore::InitializeList(TypeReference const& type, json& value) { value = json::array(); return success(); }
result<void, string> DataStore::InitializeArray(TypeReference const& type, json& value)
{
	value = json::array();
	auto& arr = value.get_ref<json::array_t&>();
	arr.resize(get<uint64_t>(type.TemplateArguments.at(1)), json{});
	auto& element_type = get<TypeReference>(type.TemplateArguments.at(0));
	for (auto& el : arr)
	{
		auto result = InitializeValue(element_type, el);
		if (result.has_error())
			return result;
	}
	return success();
}
result<void, string> DataStore::InitializeRef(TypeReference const& type, json& value) { value = json{}; return success(); }
result<void, string> DataStore::InitializeOwn(TypeReference const& type, json& value) { value = json{}; return success(); }
result<void, string> DataStore::InitializeVariant(TypeReference const& type, json& value)
{
	value = json{};
	value = json::array();
	auto& arr = value.get_ref<json::array_t&>();
	arr.resize(2, json{});
	arr.at(0) = 0; /// first element of variant is active by default

	auto& element_type = get<TypeReference>(type.TemplateArguments.at(0));
	return InitializeValue(element_type, arr.at(1));
}

template <typename... ARGS>
void Text(string_view str, ARGS&&... args)
{
	auto f = vformat(str, make_format_args(forward<ARGS>(args)...));
	ImGui::Text("%s", f.c_str());
}

void DataStore::ViewVoid(TypeReference const& type, json const& value, json const& field_properties) { Text("void"); }
void DataStore::ViewF32(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (float)value); }
void DataStore::ViewF64(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (double)value); }
void DataStore::ViewI8(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (int8_t)value); }
void DataStore::ViewI16(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (int16_t)value); }
void DataStore::ViewI32(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (int32_t)value); }
void DataStore::ViewI64(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (int64_t)value); }
void DataStore::ViewU8(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (uint8_t)value); }
void DataStore::ViewU16(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (uint16_t)value); }
void DataStore::ViewU32(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (uint32_t)value); }
void DataStore::ViewU64(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (uint64_t)value); }
void DataStore::ViewBool(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (bool)value); }
void DataStore::ViewString(TypeReference const& type, json const& value, json const& field_properties) { Text("{}", (string_view)value); }
void DataStore::ViewBytes(TypeReference const& type, json const& value, json const& field_properties) { Text("<bytes>"); }
void DataStore::ViewFlags(TypeReference const& type, json const& value, json const& field_properties) { Text("<flags>"); }
void DataStore::ViewList(TypeReference const& type, json const& value, json const& field_properties) { Text("<list>"); }
void DataStore::ViewArray(TypeReference const& type, json const& value, json const& field_properties) { Text("<array>"); }
void DataStore::ViewRef(TypeReference const& type, json const& value, json const& field_properties) { Text("<ref>"); }
void DataStore::ViewOwn(TypeReference const& type, json const& value, json const& field_properties) { Text("<own>"); }
void DataStore::ViewVariant(TypeReference const& type, json const& value, json const& field_properties) { Text("<variant>"); }

using namespace ImGui;

static constexpr bool log_changes = false;

void DataStore::LogDataChange(json::json_pointer const& value_path, json const& from, json const& value)
{
	if constexpr (log_changes)
		cout << format("Data store '{}': '{}' changed from '{}' to '{}'\n", "...", value_path.to_string(), from.dump(), value.dump());
}

bool DataStore::EditVoid(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	if (!value.is_null())
	{
		auto issue = format("WARNING: The data stored in this value is not void (but '{}')", value.type_name());
		TextColored({ 0,1,1,1 }, "%s", issue.c_str());
		if (SmallButton("Reset Value"))
		{
			CheckError(InitializeValue(type, value));
			if constexpr (log_changes)
				LogDataChange(value_path, json{}, value);
			return true;
		}
		return false;
	}
	Text("void");
	return false;
}

template <typename JSON_TYPE, typename FUNC>
bool DataStore::EditScalar(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path, FUNC&& func)
{
	bool edited = false;
	json previous_val{};
	PushID(&value);
	if (auto ptr = value.get_ptr<JSON_TYPE*>())
	{
		if constexpr (log_changes)
			previous_val = value;
		edited = func(type, *ptr, field_properties, value_path);
	}
	else
	{
		auto issue = format("WARNING: The data stored in this value is not of the expected type (expecting '{}', got '{}')", typeid(JSON_TYPE).name(), magic_enum::enum_name(value.type()));
		TextColored({ 0,1,1,1 }, "%s", issue.c_str());
		if (SmallButton("Reset Value"))
		{
			CheckError(InitializeValue(type, value));
			edited = true;
		}
	}

	if constexpr (log_changes)
	{
		if (edited)
			LogDataChange(value_path, previous_val, value);
	}
	PopID();
	return edited;
}

#define EDIT(json_type) EditScalar<json::json_type>(type, value, field_properties, value_path, [](TypeReference const& type, json::json_type& value, json const& field_properties, json::json_pointer const& value_path) {
#define EDITEND() }); return false;

bool DataStore::EditF32(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(number_float_t)
		InputDouble("", &value, 0, 0, "%g");
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditF64(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(number_float_t)
		InputDouble("", &value, 0, 0, "%g");
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditI8(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{ 
	EDIT(number_integer_t)
		static constexpr json::number_integer_t min = std::numeric_limits<int8_t>::lowest();
		static constexpr json::number_integer_t max = std::numeric_limits<int8_t>::max();
		DragScalar("", ImGuiDataType_S64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditI16(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(number_integer_t)
		static constexpr json::number_integer_t min = std::numeric_limits<int16_t>::lowest();
		static constexpr json::number_integer_t max = std::numeric_limits<int16_t>::max();
		DragScalar("", ImGuiDataType_S64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}
bool DataStore::EditI32(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(number_integer_t)
		static constexpr json::number_integer_t min = std::numeric_limits<int32_t>::lowest();
		static constexpr json::number_integer_t max = std::numeric_limits<int32_t>::max();
		DragScalar("", ImGuiDataType_S64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}
bool DataStore::EditI64(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(number_integer_t)
		DragScalar("", ImGuiDataType_S64, &value);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditU8(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(number_unsigned_t)
		static constexpr json::number_unsigned_t min = std::numeric_limits<uint8_t>::lowest();
		static constexpr json::number_unsigned_t max = std::numeric_limits<uint8_t>::max();
		DragScalar("", ImGuiDataType_U64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditU16(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(number_unsigned_t)
		static constexpr json::number_unsigned_t min = std::numeric_limits<uint16_t>::lowest();
		static constexpr json::number_unsigned_t max = std::numeric_limits<uint16_t>::max();
		DragScalar("", ImGuiDataType_U64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}
bool DataStore::EditU32(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(number_unsigned_t)
		static constexpr json::number_unsigned_t min = std::numeric_limits<uint32_t>::lowest();
		static constexpr json::number_unsigned_t max = std::numeric_limits<uint32_t>::max();
		DragScalar("", ImGuiDataType_U64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}
bool DataStore::EditU64(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(number_unsigned_t)
		DragScalar("", ImGuiDataType_U64, &value);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditBool(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(boolean_t)
		return Checkbox("Value", &value);
	EDITEND()
}

bool DataStore::EditString(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(string_t)
		InputText("", &value);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditList(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path)
{
	EDIT(array_t)

		return false;
	//return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditBytes(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path) { return false; }
bool DataStore::EditFlags(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path) { return false; }
bool DataStore::EditArray(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path) { return false; }
bool DataStore::EditRef(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path) { return false; }
bool DataStore::EditOwn(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path) { return false; }
bool DataStore::EditVariant(TypeReference const& type, json& value, json const& field_properties, json::json_pointer const& value_path) { return false; }
