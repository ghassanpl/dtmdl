#include "pch.h"

#include "Values.h"
#include "Database.h"

#include "imgui.h"

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

	/// TODO: Add conversions
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
	Do(type, value, [&](auto& handlers, TypeReference const& type, json& value) { handlers.ViewFunc(*this, type, value, field_properties); });
}

void DataStore::EditValue(TypeReference const& type, json& value, json const& field_properties)
{
	Do(type, value, [&](auto& handlers, TypeReference const& type, json& value) {
		handlers.ViewFunc(*this, type, value, field_properties);
		handlers.EditorFunc(*this, type, value, field_properties); 
	});
}

void DataStore::EditVoid(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditF32(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditF64(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditI8(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditI16(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditI32(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditI64(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditU8(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditU16(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditU32(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditU64(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditBool(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditString(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditBytes(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditFlags(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditList(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditArray(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditRef(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditOwn(TypeReference const& type, json& value, json const& field_properties) {}
void DataStore::EditVariant(TypeReference const& type, json& value, json const& field_properties) {}

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
