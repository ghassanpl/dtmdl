#pragma once

result<void, string> InitializeValue(Schema const& schema, TypeReference const& type, json& value);
void ViewValue(Schema const& schema, TypeReference const& type, json& value, json const& field_attributes);
bool EditValue(Schema const& schema, TypeReference const& type, json& value, json const& field_attributes, json::json_pointer value_path);

enum class ConversionResult
{
	ConversionImpossible,
	DataPreserved,
	DataCorrupted,
	DataLost,
};

ConversionResult ResultOfConversion(Schema const& schema, TypeReference const& from, TypeReference const& to, json const& value);
result<void, string> Convert(TypeReference const& from, TypeReference const& to, json const& value);

using VisitorFunc = function<bool(Schema const&, TypeReference const&, json::json_pointer, json&)>;
using ConstVisitorFunc = function<bool(Schema const&, TypeReference const&, json::json_pointer, json const&)>;
[[nodiscard]] bool VisitValue(Schema const& schema, TypeReference const& type, json& value, VisitorFunc visitor);
[[nodiscard]] bool VisitValue(Schema const& schema, TypeReference const& type, json const& value, ConstVisitorFunc visitor);

[[nodiscard]] bool ForEveryObjectWithTypeName(Schema const& schema, TypeReference const& type, json& value, string_view type_name, function<bool(json&)> const& object_func);
[[nodiscard]] bool ForEveryObjectWithTypeName(Schema const& schema, TypeReference const& type, json const& value, string_view type_name, function<bool(json const&)> const& object_func);
