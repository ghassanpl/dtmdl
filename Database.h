#pragma once

enum class DefinitionType
{
  Enum,
  BuiltIn,
  Struct,
  Class,
};

struct TypeDefinition;

struct TypeReference
{
  TypeDefinition const* Type = nullptr;
  vector<variant<uint64_t, TypeReference>> TemplateArguments;

  TypeReference() noexcept = default;
  TypeReference(TypeReference const&) noexcept = default;
  TypeReference(TypeReference&&) noexcept = default;
  TypeReference& operator=(TypeReference const&) noexcept = default;
  TypeReference& operator=(TypeReference&&) noexcept = default;

  explicit TypeReference(TypeDefinition const* value) noexcept;

  TypeDefinition const* operator->() const { return Type; }

  string ToString() const;
};

enum class TemplateParameterQualifier
{
  AnyType,
  Struct,
  NotClass,
  Enum,
  Integral,
  Floating,
  Simple,

  Size,
  Pointer,
  Class,
};

struct TemplateParameter
{
  string Name;
  TemplateParameterQualifier Qualifier{};
  bool Multiple = false;
  /// TypeReference DefaultValue{};

  inline constexpr bool QualifierRequiresCompletedType() const
  {
    switch (Qualifier)
    {
    case TemplateParameterQualifier::Size:
    case TemplateParameterQualifier::Pointer:
    case TemplateParameterQualifier::Class:
      return false;
    }
    return true;
  }
};

struct TypeDefinition
{
  virtual ~TypeDefinition() noexcept = default;

  virtual DefinitionType Type() const noexcept = 0;

  bool IsClass() const noexcept { return Type() == DefinitionType::Class; }
  bool IsStruct() const noexcept { return Type() == DefinitionType::Struct; }
  bool IsRecord() const noexcept { return Type() == DefinitionType::Struct || Type() == DefinitionType::Class; }
  bool IsBuiltIn() const noexcept { return Type() == DefinitionType::BuiltIn; }
  bool IsEnum() const noexcept { return Type() == DefinitionType::Enum; }

  auto const& Name() const noexcept { return mName; }
  auto const& BaseType() const noexcept { return mBaseType; }
  auto const& TemplateParameters() const noexcept { return mTemplateParameters; }

protected:

  friend struct Database;

  bool mCompleted = false;

  string mName;
  TypeReference mBaseType{};
  vector<TemplateParameter> mTemplateParameters{};

  TypeDefinition(string name)
    : mName(move(name))
  {

  }

  TypeDefinition(string name, TypeReference base_type)
    : mName(move(name)), mBaseType(move(base_type))
  {
  }

};

struct RecordDefinition;

struct FieldDefinition
{
  RecordDefinition const* ParentRecord = nullptr;
  string Name;
  TypeReference FieldType{};
  json InitialValue;
};

struct RecordDefinition : TypeDefinition
{
  FieldDefinition const* Field(size_t i) const;
  FieldDefinition const* Field(string_view name) const;

  auto const& Fields() const noexcept { return mFields; }

  string FreshFieldName() const
  {
    return "Field1";
  }

protected:

  friend struct Database;

  vector<unique_ptr<FieldDefinition>> mFields;

  using TypeDefinition::TypeDefinition;
};

struct StructDefinition : RecordDefinition
{
  virtual DefinitionType Type() const noexcept override { return DefinitionType::Struct; }
  //virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

protected:

  using RecordDefinition::RecordDefinition;
};

struct ClassDefinition : RecordDefinition
{
  virtual DefinitionType Type() const noexcept override { return DefinitionType::Class; }
  //virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

protected:

  using RecordDefinition::RecordDefinition;

};

struct EnumDefinition : TypeDefinition
{
  virtual DefinitionType Type() const noexcept override { return DefinitionType::Enum; }
  //virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

protected:

  using TypeDefinition::TypeDefinition;
};

struct BuiltinDefinition : TypeDefinition
{
  //virtual string NativeName() const override { return mNativeEquivalent; }

  auto Markable() const noexcept { return mMarkable; }
  auto ApplicableQualifiers() const noexcept { return mApplicableQualifiers; }

  virtual DefinitionType Type() const noexcept override { return DefinitionType::BuiltIn; }
  //virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

protected:

  friend struct Database;

  BuiltinDefinition(string name, string native, vector<TemplateParameter> template_params = {}, bool markable = false)
    : TypeDefinition(move(name), {}), mNativeEquivalent(move(native)), mMarkable(markable)
  {
    mTemplateParameters = move(template_params);
  }

  string mNativeEquivalent;
  bool mMarkable = false;
  ghassanpl::enum_flags<TemplateParameterQualifier> mApplicableQualifiers;

  /*
  BuiltinDefinition(Definition const* parent, string name, string native, NativeTypeDescriptor descriptor, vector<TemplateParameter> template_params = {}, bool markable = false)
    : TypeDefinition(parent, move(name), move(descriptor), {}), NativeEquivalent(move(native)), Markable(markable)
  {
    TemplateParameters = move(template_params);
  }

  friend struct Schema;
  */
};

struct Database
{
  auto const& Definitions() const noexcept { return mDefinitions; }

  using Def = TypeDefinition const*;
  using Rec = RecordDefinition const*;
  using Fld = FieldDefinition const*;

  template <typename T>
  inline constexpr static T* mut(T const* v) noexcept { return const_cast<T*>(v); }

  string FreshName(string_view base) const;

  result<StructDefinition const*, string> AddNewStruct();

  result<void, string> AddNewField(Rec def);

  result<void, string> ValidateRecordBaseType(Rec def, TypeReference const& type);
  result<void, string> ValidateTypeName(Def def, string new_name);
  result<void, string> ValidateFieldName(Fld def, string new_name);
  result<void, string> ValidateFieldType(Fld def, TypeReference const& type);

  result<void, string> SetRecordBaseType(Rec def, TypeReference const& type);
  result<void, string> SetTypeName(Def def, string new_name);
  result<void, string> SetFieldName(Fld def, string new_name);
  result<void, string> SetFieldType(Fld def, TypeReference const& type);

  bool IsParentOrChild(Def a, Def b);

  Database();

private:

  template <typename T, typename... ARGS>
  T const* AddType(string name, ARGS&&... args)
  {
    auto ptr = unique_ptr<T>(new T{ name, forward<ARGS>(args)... });
    auto result = ptr.get();
    mDefinitions[name] = move(ptr);
    return result;
  }

  BuiltinDefinition const* AddNative(string name, string native_name, vector<TemplateParameter> params = {}, bool markable = false);

  map<string, unique_ptr<TypeDefinition>, less<>> mDefinitions;
  vector<TypeDefinition const*> mChildDefinitionsInOrderOfDependency;
};
