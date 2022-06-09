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
  while (mSchema.Definitions.contains(candidate))
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

static string_view cpp_keywords[] = {
  "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char8_t",
  "char16_t", "char32_t", "class", "compl", "concept", "const", "consteval", "constexpr", "constinit", "const_cast", "continue",
  "co_await", "co_return", "co_yield", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit",
  "export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new",
  "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public", "reflexpr", "register",
  "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
  "switch", "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned",
  "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq "
};

result<void, string> ValidateIdentifierName(string const& new_name)
{
  if (new_name.empty())
    return failure("name cannot be empty");
  if (!string_ops::ascii::isalpha(new_name[0]) && new_name[0] != '_')
    return failure("name must start with a letter or underscore");
  if (!ranges::all_of(new_name, string_ops::ascii::isident))
    return failure("name must contain only letters, numbers, or underscores");
  if (ranges::find(cpp_keywords, new_name) != ranges::end(cpp_keywords))
    return failure("name cannot be a C++ keyword");
  if (new_name.find("__") != string::npos)
    return failure("name cannot contain two consecutive underscores (__)");
  if (new_name.size() >= 2 && new_name[0] == '_' && string_ops::ascii::isupper(new_name[1]))
    return failure("name cannot begin with an underscore followed by a capital letter (_X)");
  return success();
}

result<void, string> Database::ValidateTypeName(Def def, string const& new_name)
{
  if (auto result = ValidateIdentifierName(new_name); result.has_error())
    return result;

  auto it = mSchema.Definitions.find(new_name);
  if (it == mSchema.Definitions.end())
    return success();
  if (it->second.get() == def)
    return success();
  return failure("a type with that name already exists");
}

result<void, string> Database::ValidateFieldName(Fld def, string const& new_name)
{
  if (auto result = ValidateIdentifierName(new_name); result.has_error())
    return result;

  auto rec = def->ParentRecord;
  for (auto& field : rec->mFields)
  {
    if (field.get() == def)
      continue;
    if (field->Name == new_name)
      return failure("a field with that name already exists");
  }
  return success();
}

result<void, string> Database::ValidateRecordBaseType(Rec def, TypeReference const& type)
{
  if (IsParent(def, type.Type))
    return failure("cycle in base types");
  return success();
}

result<void, string> Database::ValidateFieldType(Fld def, TypeReference const& type)
{
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

result<void, string> Database::SetTypeName(Def def, string const& new_name)
{
  auto result = ValidateTypeName(def, new_name);
  if (result.has_error())
    return result;

  /// TODO: 2. DataStore update; 3. Changelog add; 4. Save
  mut(def)->mName = new_name;
  return success();
}

result<void, string> Database::SetFieldName(Fld def, string const& new_name)
{
  auto result = ValidateFieldName(def, new_name);
  if (result.has_error())
    return result;

  /// TODO: 2. DataStore update; 3. Changelog add; 4. Save
  mut(def)->Name = new_name;
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

bool Database::IsParent(Def parent, Def potential_child)
{
  if ((parent && !parent->IsRecord()) || (potential_child && !potential_child->IsRecord()))
    return false;

  while (potential_child)
  {
    if (parent == potential_child)
      return true;
    potential_child = potential_child->mBaseType.Type;
  }

  return false;
}

/*
bool Database::IsParentOrChild(TypeDefinition const* a, TypeDefinition const* b)
{
  if (a == b)
    return true;
  if ((a && !a->IsRecord()) || (b && !b->IsRecord()))
    return false;
  //if (!a->mBaseType.Type && !b->mBaseType.Type)
    //return false;
  auto type_a = a;/// ->mBaseType.Type;
  auto type_b = b;/// ->mBaseType.Type;

  while (type_a)
  {
    if (type_a == type_b)
      return true;
    if (type_a->mBaseType.Type)
      type_a = type_a->mBaseType.Type;
    else
      break;
  }

  type_a = a;/// ->mBaseType.Type;

  while (type_b)
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
*/
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

FieldDefinition const* RecordDefinition::Field(string_view name) const
{
  for (auto& field : mFields)
  {
    if (field->Name == name)
      return field.get();
  }
  return nullptr;
}

string RecordDefinition::FreshFieldName() const
{
  string candidate = "Field";
  size_t num = 1;
  while (Field(candidate))
    candidate = format("Field{}", num++);
  return candidate;
}
