#include "pch.h"

#include "Database.h"

string to_string(TypeReference const& tr)
{
  return tr.ToString();
}

string TypeReference::ToString() const
{
  if (!Type)
    return "[none]";

  string base = Type->Name();
  if (TemplateArguments.size())
  {
    base += '<';
    bool first = true;
    for (auto& arg : TemplateArguments)
    {
      if (!first)
        base += ", ";
      base += visit([](auto const& val) { return to_string(val); }, arg);
      first = false;
    }
    base += '>';
  }
  return base;
}

TypeReference::TypeReference(TypeDefinition const* value) noexcept : Type(value), TemplateArguments(value ? value->TemplateParameters().size() : 0) { }

string Database::FreshName(string_view base) const
{
  string candidate = string{ base };
  size_t num = 1;
  while (mDefinitions.contains(candidate))
    candidate = format("{}{}", base, num++);
  return candidate;
}

result<StructDefinition const*, string> Database::AddNewStruct()
{
  return AddType<StructDefinition>(FreshName("Struct"));
}

result<void, string> Database::AddNewField(Rec def)
{
  /// TODO: 1. Validation; 2. DataStore update; 3. Save
  mut(def)->mFields.push_back(make_unique<FieldDefinition>(def, def->FreshFieldName()));
  return success();
}

result<void, string> Database::SetRecordBaseType(Rec def, TypeReference const& type)
{
  /// TODO: 2. DataStore update; 3. Changelog add; 4. Save
  auto result = ValidateRecordBaseType(def, type);
  if (result.has_error())
    return result;

  mut(def)->mBaseType = type;
  return success();
}

result<void, string> Database::SetTypeName(Def def, string new_name)
{
  auto result = ValidateTypeName(def, new_name);
  if (result.has_error())
    return result;

  /// TODO: 2. DataStore update; 3. Changelog add; 4. Save
  mut(def)->mName = move(new_name);
  return success();
}

result<void, string> Database::SetFieldName(Fld def, string new_name)
{
  auto result = ValidateFieldName(def, new_name);
  if (result.has_error())
    return result;

  /// TODO: 2. DataStore update; 3. Changelog add; 4. Save
  mut(def)->Name = move(new_name);
  return success();
}

result<void, string> Database::SetFieldType(Fld def, TypeReference const& type)
{
  auto result = ValidateFieldType(def, type);
  if (result.has_error())
    return result;

  /// TODO: 2. DataStore update; 3. Changelog add; 4. Save
  mut(def)->FieldType = type;
  return success();
}

bool Database::IsParentOrChild(TypeDefinition const* a, TypeDefinition const* b)
{
  if (a == b)
    return true;
  if (!a->IsRecord() || !b->IsRecord())
    return false;
  if (!a->mBaseType.Type && !b->mBaseType.Type)
    return false;
  auto type_a = a->mBaseType.Type;
  auto type_b = b->mBaseType.Type;

  while (type_a && type_b)
  {
    if (type_a == type_b)
      return true;
    if (type_a->mBaseType.Type)
      type_a = type_a->mBaseType.Type;
    else
      break;
  }

  type_a = a->mBaseType.Type;

  while (type_a && type_b)
  {
    if (type_a == type_b)
      return true;
    if (type_b->mBaseType.Type)
      type_b = type_b->mBaseType.Type;
    else
      break;
  }

  return false;
}

Database::Database()
{
  AddNative("void", "::DataModel::NativeTypes::Void");
  AddNative("f32", "float");
  AddNative("f64", "double");
  AddNative("i8", "int8_t");
  AddNative("i16", "int16_t");
  AddNative("i32", "int32_t");
  AddNative("i64", "int64_t");
  AddNative("u8", "uint8_t");
  AddNative("u16", "uint16_t");
  AddNative("u32", "uint32_t");
  AddNative("u64", "uint64_t");
  AddNative("bool", "bool");
  AddNative("string", "::DataModel::NativeTypes::String");
  AddNative("bytes", "::DataModel::NativeTypes::Bytes");
  auto flags = AddNative("flags", "::DataModel::NativeTypes::Flags", vector{
    TemplateParameter{ "ENUM", TemplateParameterQualifier::Enum }
    });
  auto list = AddNative("list", "::DataModel::NativeTypes::List", vector{
    TemplateParameter{ "ELEMENT_TYPE", TemplateParameterQualifier::NotClass }
    }, true);
  auto arr = AddNative("array", "::DataModel::NativeTypes::Array", vector{
    TemplateParameter{ "ELEMENT_TYPE", TemplateParameterQualifier::NotClass },
    TemplateParameter{ "SIZE", TemplateParameterQualifier::Size }
    }, true);
  auto ref = AddNative("ref", "::DataModel::NativeTypes::Ref", vector{
    TemplateParameter{ "POINTEE", TemplateParameterQualifier::Class }
    }, true);
  auto own = AddNative("own", "::DataModel::NativeTypes::Own", vector{
    TemplateParameter{ "POINTEE", TemplateParameterQualifier::Class }
    }, true);

  auto variant = AddNative("variant", "::DataModel::NativeTypes::Variant", vector{
    TemplateParameter{ "TYPES", TemplateParameterQualifier::NotClass, true }
    }, true);
}

BuiltinDefinition const* Database::AddNative(string name, string native_name, vector<TemplateParameter> params, bool markable)
{
  return AddType<BuiltinDefinition>(move(name), move(native_name), move(params), markable);
}
