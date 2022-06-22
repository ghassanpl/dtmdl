#pragma once

struct DataStore;

result<void, string> InitializeValue(TypeReference const& type, json& value);
void ViewValue(TypeReference const& type, json& value, json const& field_attributes, DataStore const* store = nullptr);
bool EditValue(TypeReference const& type, json& value, json const& field_attributes, json::json_pointer value_path, DataStore* store = nullptr);

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
