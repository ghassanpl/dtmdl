#pragma once

struct DataStore;
struct TypeReference;

result<void, string> InitializeValue(TypeReference const& type, json& value);

void ViewValue(TypeReference const& type, json& value, json const& field_attributes, DataStore const* store = nullptr);
inline void ViewValue(TypeReference const& type, json& value) { ViewValue(type, value, empty_json, nullptr); }

bool EditValue(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer value_path, DataStore* store = nullptr);
inline bool EditValue(TypeReference const& type, json& value) { return EditValue(type, value, empty_json, json::json_pointer{}, nullptr); }

enum class [[nodiscard]] ConversionResult
{
	ConversionImpossible,
	DataPreserved,
	DataCorrupted,
	DataLost,
};

ConversionResult ResultOfConversion(TypeReference const& from, TypeReference const& to, json const& value);
result<void, string> Convert(TypeReference const& from, TypeReference const& to, json& value);

using VisitorFunc = function<bool(TypeReference const&, json::json_pointer, json&)>;
using ConstVisitorFunc = function<bool(TypeReference const&, json::json_pointer, json const&)>;
[[nodiscard]] bool VisitValue(TypeReference const& type, json& value, VisitorFunc visitor);
[[nodiscard]] bool VisitValue(TypeReference const& type, json const& value, ConstVisitorFunc visitor);

[[nodiscard]] bool ForEveryObjectWithTypeName(TypeReference const& value_type, json& value, string_view type_name, function<bool(json&)> const& object_func);
[[nodiscard]] bool ForEveryObjectWithTypeName(TypeReference const& value_type, json const& value, string_view type_name, function<bool(json const&)> const& object_func);

[[nodiscard]] bool ForEveryObjectWithType(TypeReference const& value_type, json& value, TypeReference const& searched_type, function<bool(json&)> const& object_func);
[[nodiscard]] bool ForEveryObjectWithType(TypeReference const& value_type, json const& value, TypeReference const& searched_type, function<bool(json const&)> const& object_func);

[[nodiscard]] bool ForEveryObjectWithType(TypeReference const& value_type, json& value, json const& serialized_type, function<bool(json&)> const& object_func);
[[nodiscard]] bool ForEveryObjectWithType(TypeReference const& value_type, json const& value, json const& serialized_type, function<bool(json const&)> const& object_func);
