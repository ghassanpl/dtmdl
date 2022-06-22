#include "pch.h"

#include "imgui.h"
#include "imgui_stdlib.h"

#include "Database.h"
#include "Values.h"

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

/*
static constexpr bool log_changes = false;

void DataStore::LogDataChange(json::json_pointer const& value_path, json const& from, json const& value)
{
	if constexpr (log_changes)
		cout << format("Data store '{}': '{}' changed from '{}' to '{}'\n", "...", value_path.to_string(), from.dump(), value.dump());
}

bool DataStore::EditVoid(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
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
bool DataStore::EditScalar(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path, FUNC&& func)
{
	bool edited = false;
	json previous_val{};
	if constexpr (log_changes)
		previous_val = value;
	PushID(&value);
	if (auto ptr = value.get_ptr<JSON_TYPE*>())
	{
		edited = func(type, *ptr, field_attributes, value_path);
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

#define EDIT(json_type) EditScalar<json::json_type>(type, value, field_attributes, value_path, [](TypeReference const& type, json::json_type& value, json const& field_attributes, json::json_pointer const& value_path) {
#define EDITEND() }); return false;

bool DataStore::EditF32(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(number_float_t)
		InputDouble("", &value, 0, 0, "%g");
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditF64(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(number_float_t)
		InputDouble("", &value, 0, 0, "%g");
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditI8(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{ 
	EDIT(number_integer_t)
		static constexpr json::number_integer_t min = std::numeric_limits<int8_t>::lowest();
		static constexpr json::number_integer_t max = std::numeric_limits<int8_t>::max();
		DragScalar("", ImGuiDataType_S64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditI16(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(number_integer_t)
		static constexpr json::number_integer_t min = std::numeric_limits<int16_t>::lowest();
		static constexpr json::number_integer_t max = std::numeric_limits<int16_t>::max();
		DragScalar("", ImGuiDataType_S64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}
bool DataStore::EditI32(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(number_integer_t)
		static constexpr json::number_integer_t min = std::numeric_limits<int32_t>::lowest();
		static constexpr json::number_integer_t max = std::numeric_limits<int32_t>::max();
		DragScalar("", ImGuiDataType_S64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}
bool DataStore::EditI64(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(number_integer_t)
		DragScalar("", ImGuiDataType_S64, &value);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditU8(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(number_unsigned_t)
		static constexpr json::number_unsigned_t min = std::numeric_limits<uint8_t>::lowest();
		static constexpr json::number_unsigned_t max = std::numeric_limits<uint8_t>::max();
		DragScalar("", ImGuiDataType_U64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditU16(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(number_unsigned_t)
		static constexpr json::number_unsigned_t min = std::numeric_limits<uint16_t>::lowest();
		static constexpr json::number_unsigned_t max = std::numeric_limits<uint16_t>::max();
		DragScalar("", ImGuiDataType_U64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}
bool DataStore::EditU32(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(number_unsigned_t)
		static constexpr json::number_unsigned_t min = std::numeric_limits<uint32_t>::lowest();
		static constexpr json::number_unsigned_t max = std::numeric_limits<uint32_t>::max();
		DragScalar("", ImGuiDataType_U64, &value, 1.0f, &min, &max, nullptr, ImGuiSliderFlags_AlwaysClamp);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}
bool DataStore::EditU64(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(number_unsigned_t)
		DragScalar("", ImGuiDataType_U64, &value);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditBool(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(boolean_t)
		return Checkbox("Value", &value);
	EDITEND()
}

bool DataStore::EditString(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(string_t)
		InputText("", &value);
		return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditList(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path)
{
	EDIT(array_t)

		return false;
	//return IsItemDeactivatedAfterEdit();
	EDITEND()
}

bool DataStore::EditBytes(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path) { return false; }
bool DataStore::EditFlags(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path) { return false; }
bool DataStore::EditArray(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path) { return false; }
bool DataStore::EditRef(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path) { return false; }
bool DataStore::EditOwn(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path) { return false; }
bool DataStore::EditVariant(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer const& value_path) { return false; }
*/

struct IBuiltInHandler
{
	virtual ~IBuiltInHandler() noexcept = default;

	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const = 0;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const = 0;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const = 0;
	virtual bool Visit(json& value, VisitorFunc visitor) const = 0;
	virtual bool Visit(json const& value, ConstVisitorFunc visitor) const = 0;
};

struct IScalarHandler : IBuiltInHandler
{
	virtual bool Visit(json& value, VisitorFunc visitor) const { return false; }
	virtual bool Visit(json const& value, ConstVisitorFunc visitor) const override { return false; }
};

struct VoidHandler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;

} mVoidHandler;

struct F32Handler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;

} mF32Handler;

struct F64Handler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;

} mF64Handler;

struct I8Handler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;

} mI8Handler;

struct I16Handler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;

} mI16Handler;

struct I32Handler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;

} mI32Handler;

struct I64Handler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;

} mI64Handler;

struct U8Handler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;


} mU8Handler;

struct U16Handler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;


} mU16Handler;

struct U32Handler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;


} mU32Handler;

struct U64Handler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;


} mU64Handler;

struct BoolHandler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;


} mBoolHandler;

struct StringHandler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;


} mStringHandler;

struct BytesHandler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;


} mBytesHandler;

struct FlagsHandler : IScalarHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;


} mFlagsHandler;

struct ListHandler : IBuiltInHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;
	virtual bool Visit(json& value, VisitorFunc visitor) const override;
	virtual bool Visit(json const& value, ConstVisitorFunc visitor) const override;


} mListHandler;

struct ArrayHandler : IBuiltInHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;
	virtual bool Visit(json& value, VisitorFunc visitor) const override;
	virtual bool Visit(json const& value, ConstVisitorFunc visitor) const override;


} mArrayHandler;

struct RefHandler : IBuiltInHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;
	virtual bool Visit(json& value, VisitorFunc visitor) const override;
	virtual bool Visit(json const& value, ConstVisitorFunc visitor) const override;


} mRefHandler;

struct OwnHandler : IBuiltInHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;
	virtual bool Visit(json& value, VisitorFunc visitor) const override;
	virtual bool Visit(json const& value, ConstVisitorFunc visitor) const override;


} mOwnHandler;

struct VariantHandler : IBuiltInHandler
{
	virtual result<void, string> Initialize(TypeReference const& to_type, json& value) const override;
	virtual void View(TypeReference const& value_type, json const& value, json const& attributes, DataStore const* store = nullptr) const override;
	virtual bool Edit(TypeReference const& value_type, json& value, json const& attributes, json::json_pointer path, DataStore* store = nullptr) const override;
	virtual bool Visit(json& value, VisitorFunc visitor) const override;
	virtual bool Visit(json const& value, ConstVisitorFunc visitor) const override;


} mVariantHandler;


result<void, string> VoidHandler::Initialize(TypeReference const& type, json& value) const { value = {}; return success(); }
result<void, string> F32Handler::Initialize(TypeReference const& type, json& value) const { value = float{}; return success(); }
result<void, string> F64Handler::Initialize(TypeReference const& type, json& value) const { value = double{}; return success(); }
result<void, string> I8Handler::Initialize(TypeReference const& type, json& value) const { value = int8_t{}; return success(); }
result<void, string> I16Handler::Initialize(TypeReference const& type, json& value) const { value = int16_t{}; return success(); }
result<void, string> I32Handler::Initialize(TypeReference const& type, json& value) const { value = int32_t{}; return success(); }
result<void, string> I64Handler::Initialize(TypeReference const& type, json& value) const { value = int64_t{}; return success(); }
result<void, string> U8Handler::Initialize(TypeReference const& type, json& value) const { value = uint8_t{}; return success(); }
result<void, string> U16Handler::Initialize(TypeReference const& type, json& value) const { value = uint16_t{}; return success(); }
result<void, string> U32Handler::Initialize(TypeReference const& type, json& value) const { value = uint32_t{}; return success(); }
result<void, string> U64Handler::Initialize(TypeReference const& type, json& value) const { value = uint64_t{}; return success(); }
result<void, string> BoolHandler::Initialize(TypeReference const& type, json& value) const { value = bool{}; return success(); }
result<void, string> StringHandler::Initialize(TypeReference const& type, json& value) const { value = string{}; return success(); }
result<void, string> BytesHandler::Initialize(TypeReference const& type, json& value) const { value = json::binary_t{}; return success(); }
result<void, string> FlagsHandler::Initialize(TypeReference const& type, json& value) const { value = uint64_t{}; return success(); }
result<void, string> ListHandler::Initialize(TypeReference const& type, json& value) const { value = json::array(); return success(); }
result<void, string> ArrayHandler::Initialize(TypeReference const& type, json& value) const
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
result<void, string> RefHandler::Initialize(TypeReference const& type, json& value) const { value = json{}; return success(); }
result<void, string> OwnHandler::Initialize(TypeReference const& type, json& value) const { value = json{}; return success(); }
result<void, string> VariantHandler::Initialize(TypeReference const& type, json& value) const
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

void VoidHandler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("void"); }
void F32Handler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (float)value); }
void F64Handler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (double)value); }
void I8Handler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (int8_t)value); }
void I16Handler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (int16_t)value); }
void I32Handler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (int32_t)value); }
void I64Handler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (int64_t)value); }
void U8Handler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (uint8_t)value); }
void U16Handler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (uint16_t)value); }
void U32Handler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (uint32_t)value); }
void U64Handler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (uint64_t)value); }
void BoolHandler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (bool)value); }
void StringHandler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("{}", (string_view)value); }
void BytesHandler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("<bytes>"); }
void FlagsHandler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("<flags>"); }
void ListHandler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("<list>"); }
void ArrayHandler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("<array>"); }
void RefHandler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("<ref>"); }
void OwnHandler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("<own>"); }
void VariantHandler::View(TypeReference const& type, json const& value, json const& field_attributes, DataStore const* store) const { Text("<variant>"); }

map<string, IBuiltInHandler const*, less<>> const mBuiltIns = {
	{"void", &mVoidHandler},
	{"f32", &mF32Handler},
	{"f64", &mF64Handler},
	{"i8", &mI8Handler},
	{"i16", &mI16Handler},
	{"i32", &mI32Handler},
	{"i64", &mI64Handler},
	{"u8", &mU8Handler},
	{"u16", &mU16Handler},
	{"u32", &mU32Handler},
	{"u64", &mU64Handler},
	{"bool", &mBoolHandler},
	{"string", &mStringHandler},
	{"bytes", &mBytesHandler},
	{"flags", &mFlagsHandler},
	{"list", &mListHandler},
	{"array", &mArrayHandler},
	{"ref", &mRefHandler},
	{"own", &mOwnHandler},
	{"variant", &mVariantHandler},
};

using ConversionFunction = result<void, string>(json&, TypeReference const&, TypeReference const&);
map<pair<string, string>, function<ConversionFunction>, less<>> const mConversionFuncs = [] {
	map<pair<string, string>, function<ConversionFunction>, less<>> conversion_funcs;

	auto AddConversion = [&]<typename A, typename B>(type_identity<pair<A, B>>, function<ConversionFunction> func) {
		if constexpr (!is_same_v<A, B>)
		{
			conversion_funcs[{name_of(type_identity<A>{}), name_of(type_identity<B>{})}] = move(func);
		}
	};

	auto AddNumericConversion = [&]<typename A, typename B>(type_identity<pair<A, B>> type_pair) {
		AddConversion(type_pair, [](json& value, TypeReference const&, TypeReference const&) -> result<void, string> {
			value = (B)(A)value;
			return success();
		});
	};
	auto AddConversionForEachPair = [&]<typename... PAIRS>(type_identity<tuple<PAIRS...>>, auto && conversion_func) {
		(conversion_func(type_identity<PAIRS>{}), ...);
	};
	auto AddConversionForTypeCrossProduct = [&]<typename... ELEMENTS1, typename... ELEMENTS2>(type_identity<tuple<ELEMENTS1...>>, type_identity<tuple<ELEMENTS2...>>, auto&& conversion_func) {
		AddConversionForEachPair(type_identity<typename cross_product<tuple<ELEMENTS1...>, tuple<ELEMENTS2...>>::type>{}, conversion_func);
	};
	using numeric_type_list = tuple<float, double, int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t>;
	AddConversionForTypeCrossProduct(type_identity<numeric_type_list>{}, type_identity<numeric_type_list>{}, AddNumericConversion);
	AddConversionForTypeCrossProduct(type_identity<tuple<bool>>{}, type_identity<numeric_type_list>{}, AddNumericConversion);
	AddConversionForTypeCrossProduct(type_identity<numeric_type_list>{}, type_identity<tuple<bool>>{}, AddNumericConversion);

	AddConversionForTypeCrossProduct(type_identity<numeric_type_list>{}, type_identity<tuple<string>>{}, [&]<typename A, typename B>(type_identity<pair<A, B>> type_pair) {
		AddConversion(type_pair, [](json& value, TypeReference const&, TypeReference const&) -> result<void, string> {
			value = ::std::to_string((A)value);
			/// TODO: using to_chars would be faster
			return success();
		});
	});

	AddConversionForTypeCrossProduct(type_identity<tuple<string>>{}, type_identity<numeric_type_list>{}, [&]<typename A, typename B>(type_identity<pair<A, B>> type_pair) {
		AddConversion(type_pair, [](json& value, TypeReference const&, TypeReference const&) -> result<void, string> {
			auto& strref = value.get_ref<json::string_t const&>();
			B dest_value{};
			ignore = from_chars(to_address(begin(strref)), to_address(end(strref)), dest_value);
			value = dest_value;
			return success();
		});
	});

	/// TODO: Add more conversions:
	/// string <-> bytes
	/// bytes <-> list<u8>
	/// list<T> <-> array<T, N>
	/// flags<E> <-> u64
	/// variant<T1, T2, ...> <-> T1/T2/...
	/// variant<T1, T2, ...> <-> variant<U1, U2, ...> 

	return conversion_funcs;
}();

result<void, string> InitializeValue(TypeReference const& type, json& value)
{
	if (!type)
		return failure("no type provided");

	switch (type.Type->Type())
	{
	case DefinitionType::BuiltIn:
		return mBuiltIns.at(type.Type->Name())->Initialize(type, value);
	case DefinitionType::Enum:
		return failure("TODO: cannot initialize enums");
	case DefinitionType::Struct:
		return failure("TODO: cannot initialize structs");
	case DefinitionType::Class:
		return failure("TODO: cannot initialize classes");
	}
	return failure(format("unknown type type: {}", magic_enum::enum_name(type.Type->Type())));
}

void ViewValue(TypeReference const& type, json& value, json const& field_attributes, DataStore const* store)
{
	if (!type)
	{
		ImGui::TextColored({ 1,0,0,1 }, "Error: Value has no type");
		return;
	}

	switch (type.Type->Type())
	{
	case DefinitionType::BuiltIn:
		mBuiltIns.at(type.Type->Name())->View(type, value, field_attributes, store);
		return;
	case DefinitionType::Enum:
		break;
	case DefinitionType::Struct:
		break;
	case DefinitionType::Class:
		break;
	}

	return;
}

bool EditValue(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer value_path, DataStore* store)
{
	if (!type)
	{
		ImGui::TextColored({ 1,0,0,1 }, "Error: Value has no type");
		return false;
	}

	switch (type.Type->Type())
	{
	case DefinitionType::BuiltIn:
		return mBuiltIns.at(type.Type->Name())->Edit(type, value, field_attributes, move(value_path), store);
	case DefinitionType::Enum:
		break;
	case DefinitionType::Struct:
		break;
	case DefinitionType::Class:
		break;
	}

	return false;
}

ConversionResult ResultOfConversion(TypeReference const& from, TypeReference const& to, json const& value)
{
	if (!from) return ConversionResult::ConversionImpossible;
	if (!to) return ConversionResult::ConversionImpossible;

	if (from == to)
		return ConversionResult::DataPreserved;

	if (from->Name() == "void")
		return ConversionResult::DataPreserved;
	if (to->Name() == "void")
		return ConversionResult::DataLost;

	switch (from.Type->Type())
	{
	case DefinitionType::BuiltIn:
	{
		auto& handler = mBuiltIns.at(from.Type->Name());
		if (auto it = mConversionFuncs.find({ from->Name(), to->Name() }); it != mConversionFuncs.end())
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

result<void, string> Convert(TypeReference const& from, TypeReference const& to, json& value)
{
	if (!from) return failure("source type is none");
	if (!to) return failure("destination type is none");

	if (from == to)
		return success();

	if (from->Name() == "void")
		return InitializeValue(to, value);
	if (to->Name() == "void")
	{
		value = {};
		return success();
	}

	switch (from.Type->Type())
	{
	case DefinitionType::BuiltIn:
		/// TODO: Also search mConversionFuncs for {from, "*"} and {"*", to}
	{
		auto& handler = mBuiltIns.at(from.Type->Name());
		if (auto it = mConversionFuncs.find({ from->Name(), to->Name() }); it != mConversionFuncs.end())
			return it->second(value, from, to);
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

bool VisitValue(TypeReference const& type, json& value, VisitorFunc visitor)
{
	if (!type)
	{
		ImGui::TextColored({ 1,0,0,1 }, "Error: Value has no type");
		return false;
	}

	switch (type.Type->Type())
	{
	case DefinitionType::BuiltIn:
		return mBuiltIns.at(type.Type->Name())->Visit(value, visitor);
	case DefinitionType::Enum:
		break;
	case DefinitionType::Struct:
		break;
	case DefinitionType::Class:
		break;
	}

	return false;
}

bool VisitValue(TypeReference const& type, json const& value, ConstVisitorFunc visitor)
{
	if (!type)
	{
		ImGui::TextColored({ 1,0,0,1 }, "Error: Value has no type");
		return false;
	}

	switch (type.Type->Type())
	{
	case DefinitionType::BuiltIn:
		return mBuiltIns.at(type.Type->Name())->Visit(value, visitor);
	case DefinitionType::Enum:
		break;
	case DefinitionType::Struct:
		break;
	case DefinitionType::Class:
		break;
	}

	return false;
}

bool ForEveryObjectWithTypeName(TypeReference const& type, json& value, string_view type_name, function<bool(json&)> const& object_func)
{
	VisitorFunc visitor = [&](TypeReference const& child_type, json::json_pointer index, json& child_value) {
		if (child_type->Name() == type_name)
		{
			if (object_func(child_value))
				return true;
		}
		return VisitValue(child_type, child_value, visitor);
	};

	return visitor(type, json::json_pointer{}, value);
}

bool ForEveryObjectWithTypeName(TypeReference const& type, json const& value, string_view type_name, function<bool(json const&)> const& object_func)
{
	ConstVisitorFunc visitor = [&](TypeReference const& child_type, json::json_pointer index, json const& child_value) {
		if (child_type->Name() == type_name)
		{
			if (object_func(child_value))
				return true;
		}
		return VisitValue(child_type, child_value, visitor);
	};

	return visitor(type, json::json_pointer{}, value);
}
