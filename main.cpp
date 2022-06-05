// Dear ImGui: standalone example application for SDL2 + SDL_Renderer
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

// Important to understand: SDL_Renderer is an _optional_ component of SDL. We do not recommend you use SDL_Renderer
// because it provide a rather limited API to the end-user. We provide this backend for the sake of completeness.
// For a multi-platform app consider using e.g. SDL+DirectX on Windows and SDL+OpenGL on Linux/OSX.

#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include <imgui_stdlib.h>
#include <SDL2/SDL.h>
#undef main
#include <vector>
#include <map>
#include <variant>
#include <string>
#include <array>
#include <filesystem>
#include <memory>
#include <functional>
#include <format>

#include <nlohmann/json.hpp>
#include <magic_enum.hpp>
#include <ghassanpl/enum_flags.h>
#include <ghassanpl/string_ops.h>

using namespace std;
using namespace ghassanpl;
using nlohmann::json;

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

  explicit TypeReference(TypeDefinition const* value) noexcept : Type(value) { }

  TypeDefinition const* operator->() const { return Type; }

  string ToString() const;
};

struct TypeDefinition
{
  virtual ~TypeDefinition() noexcept = default;

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

  string Name;
  TypeReference BaseType{};
  vector<TemplateParameter> TemplateParameters{};

  TypeDefinition(string name)
    : Name(move(name))
  {

  }

  TypeDefinition(string name, TypeReference base_type)
    : Name(move(name)), BaseType(move(base_type))
  {
  }

  virtual DefinitionType Type() const noexcept = 0;

  bool IsClass() const noexcept { return Type() == DefinitionType::Class; }
  bool IsStruct() const noexcept { return Type() == DefinitionType::Struct; }
  bool IsRecord() const noexcept { return Type() == DefinitionType::Struct || Type() == DefinitionType::Class; }
  bool IsBuiltIn() const noexcept { return Type() == DefinitionType::BuiltIn; }
  bool IsEnum() const noexcept { return Type() == DefinitionType::Enum; }

protected:

  bool mCompleted = false;

};

string TypeReference::ToString() const
{
  if (!Type)
    return "[none]";
  
  string base = Type->Name;
  if (TemplateArguments.size())
  {
    base += '<';
    bool first = true;
    for (auto& arg : TemplateArguments)
    {
      if (!first)
        base += ", ";
      base += arg.ToString();
      first = false;
    }
    base += '>';
  }
  return base;
}

struct FieldDefinition
{
  string Name;
  TypeReference FieldType{};
  json InitialValue;
};

struct RecordDefinition : TypeDefinition
{
  vector<FieldDefinition> Fields;

  FieldDefinition const* Field(size_t i) const;
  FieldDefinition const* Field(string_view name) const;

protected:

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
  //virtual string NativeName() const override { return NativeEquivalent; }

  BuiltinDefinition(string name, string native, vector<TemplateParameter> template_params = {}, bool markable = false)
    : TypeDefinition(move(name), {}), NativeEquivalent(move(native)), Markable(markable)
  {
    TemplateParameters = move(template_params);
  }

  string NativeEquivalent;
  bool Markable = false;
  ghassanpl::enum_flags<TypeDefinition::TemplateParameterQualifier> ApplicableQualifiers;

  virtual DefinitionType Type() const noexcept override { return DefinitionType::BuiltIn; }
  //virtual void Visit(Visitor& visitor) const override { visitor.Visit(*this); }

protected:

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
  vector<shared_ptr<TypeDefinition>> Definitions;
  vector<TypeDefinition const*> ChildDefinitionsInOrderOfDependency;

  TypeDefinition* AddNative(string name, string native_name, vector<TypeDefinition::TemplateParameter> params = {}, bool markable = false)
  {
    auto ptr = make_shared<BuiltinDefinition>(name, move(native_name), move(params), markable);
    auto result = ptr.get();
    Definitions.push_back(move(ptr));
    return result;
  }

  void AddNewStruct()
  {
    Definitions.push_back(make_shared<StructDefinition>("struct1"));
  }

  bool IsParentOrChild(TypeDefinition const* a, TypeDefinition const* b)
  {
    if (a == b)
      return true;
    if (!a->IsRecord() || !b->IsRecord())
      return false;
    if (!a->BaseType.Type && !b->BaseType.Type)
      return false;
    auto type_a = a->BaseType.Type;
    auto type_b = b->BaseType.Type;

    while (type_a && type_b)
    {
      if (type_a == type_b)
        return true;
      if (type_a->BaseType.Type)
        type_a = type_a->BaseType.Type;
      else
        break;
    }

    type_a = a->BaseType.Type;

    while (type_a && type_b)
    {
      if (type_a == type_b)
        return true;
      if (type_b->BaseType.Type)
        type_b = type_b->BaseType.Type;
      else
        break;
    }

    return false;
  }

  Database()
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
      TypeDefinition::TemplateParameter{"ENUM", TypeDefinition::TemplateParameterQualifier::Enum}
    });
    auto list = AddNative("list", "::DataModel::NativeTypes::List", vector{
      TypeDefinition::TemplateParameter{"ELEMENT_TYPE", TypeDefinition::TemplateParameterQualifier::NotClass}
    }, true);
    auto arr = AddNative("array", "::DataModel::NativeTypes::Array", vector{
      TypeDefinition::TemplateParameter{"ELEMENT_TYPE", TypeDefinition::TemplateParameterQualifier::NotClass},
      TypeDefinition::TemplateParameter{"SIZE", TypeDefinition::TemplateParameterQualifier::Size}
    }, true);
    auto ref = AddNative("ref", "::DataModel::NativeTypes::Ref", vector{
      TypeDefinition::TemplateParameter{"POINTEE", TypeDefinition::TemplateParameterQualifier::Class}
    }, true);
    auto own = AddNative("own", "::DataModel::NativeTypes::Own", vector{
      TypeDefinition::TemplateParameter{"POINTEE", TypeDefinition::TemplateParameterQualifier::Class}
    }, true);

    auto variant = AddNative("variant", "::DataModel::NativeTypes::Variant", vector{
      TypeDefinition::TemplateParameter{"TYPES", TypeDefinition::TemplateParameterQualifier::NotClass, true}
    }, true);
  }
};

using FilterFunc = std::function<bool(TypeDefinition const*)>;

FilterFunc fltAnyStruct = [](TypeDefinition const* def) { return def && def->IsStruct(); };
FilterFunc fltAnyClass = [](TypeDefinition const* def) { return def && def->IsClass(); };
FilterFunc fltAnyRecord = [](TypeDefinition const* def) { return def && def->IsRecord(); };
FilterFunc fltAnyEnum = [](TypeDefinition const* def) { return def && def->IsEnum(); };
FilterFunc fltAny = [](TypeDefinition const* def) { return def; };

FilterFunc fltNoCycles(Database& db, TypeDefinition const* self, FilterFunc parent = fltAny)
{
  return FilterFunc{ [parent = move(parent), self, &db](TypeDefinition const* def) { return parent(def) && !db.IsParentOrChild(def, self); } };
}

string ValidateTemplateArgument(TypeDefinition::TemplateParameterQualifier qualifier, TypeDefinition const* arg_type)
{
  switch (qualifier)
  {
  case TypeDefinition::TemplateParameterQualifier::AnyType: break;
  case TypeDefinition::TemplateParameterQualifier::Struct:
    if (arg_type->Type() != DefinitionType::Struct)
      return "must be a struct type";
    break;
  case TypeDefinition::TemplateParameterQualifier::NotClass:
    if (arg_type->Type() == DefinitionType::Class)
      return "must be a non-class type";
    break;
  case TypeDefinition::TemplateParameterQualifier::Enum:
    if (arg_type->Type() != DefinitionType::Enum)
      return "must be an enum type";
    break;
  case TypeDefinition::TemplateParameterQualifier::Integral:
  case TypeDefinition::TemplateParameterQualifier::Floating:
  case TypeDefinition::TemplateParameterQualifier::Simple:
  case TypeDefinition::TemplateParameterQualifier::Pointer:
    if (arg_type->Type() != DefinitionType::BuiltIn)
      return "must be an integral type";
    else
    {
      auto builtin_type = dynamic_cast<BuiltinDefinition const*>(arg_type);
      if (builtin_type->ApplicableQualifiers.is_set(qualifier) == false)
        return format("must be a {} type", string_ops::ascii::tolower(magic_enum::enum_name(qualifier)));
    }
    break;
  case TypeDefinition::TemplateParameterQualifier::Class:
    if (arg_type->Type() != DefinitionType::Class)
      return "must be a class type";
    break;
  default:
    throw format("internal error: unimplemented template parameter qualifier `{}`", magic_enum::enum_name(qualifier));
  }

  return {};
}

FilterFunc fltTemplateArgumentFilter(TypeDefinition::TemplateParameterQualifier qualifier)
{
  return [qualifier](TypeDefinition const* def) { 
    auto str = ValidateTemplateArgument(qualifier, def);
    return str.empty();
  };
}

template <bool CAN_BE_NONE = false>
void TypeChooser(Database& db, TypeReference& ref, function<bool(TypeDefinition const*)> filter = {}, const char* name = "")
{
  using namespace ImGui;
  PushID(&ref);
  auto current = ref.ToString();
  if (BeginCombo(name, current.c_str()))
  {
    for (auto& type : db.Definitions)
    {
      if constexpr (CAN_BE_NONE)
      {
        if (Selectable("[none]"))
          ref = TypeReference{};
      }
      if (!filter || filter(type.get()))
      {
        if (Selectable(type->Name.c_str(), type == ref.Type))
        {
          ref = TypeReference{ type };
          ref.TemplateArguments.resize(type->TemplateParameters.size());
        }
      }
    }
    EndCombo();
  }
  if (ref.Type)
  {
    Indent(8.0f);
    size_t i = 0;
    for (auto& param : ref.Type->TemplateParameters)
    {
      PushID(&param);
      //param.QualifierRequiresCompletedType
      if (param.Qualifier == TypeDefinition::TemplateParameterQualifier::Size)
      {
        if (!holds_alternative<uint64_t>(ref.TemplateArguments[i]))
          ref.TemplateArguments[i] = uint64_t{};
        InputScalar(param.Name.c_str(), ImGuiDataType_U64, &get<uint64_t>(ref.TemplateArguments[i]));
      }
      else
        TypeChooser(db, ref.TemplateArguments[i], fltTemplateArgumentFilter(param.Qualifier), param.Name.c_str());
      PopID();
      ++i;
    }
    Unindent();
  }
  PopID();
}

void EditRecord(Database& db, RecordDefinition* def, bool is_struct)
{
  using namespace ImGui;
  SetNextItemWidth(GetContentRegionAvail().x / 3);
  InputText("Name", &def->Name);
  SameLine();
  SetNextItemWidth(GetContentRegionAvail().x / 3);
  TypeChooser<true>(db, def->BaseType, fltNoCycles(db, def, is_struct ? fltAnyStruct : fltAnyClass));

  if (Button("Add Field"))
  {
    def->Fields.push_back(FieldDefinition{ "field1" });
  }
  if (BeginTable("Fields", 5))
  {
    TableSetupColumn("Name");
    TableSetupColumn("Type");
    TableSetupColumn("Initial Value");
    TableSetupColumn("Properties");
    TableSetupColumn("Actions");
    TableSetupScrollFreeze(0, 1);
    TableHeadersRow();

    TableNextRow();
    for (auto& field : def->Fields)
    {
      PushID(&field);
      TableNextColumn();
      SetNextItemWidth(GetContentRegionAvail().x);
      InputText("##name", &field.Name);
      TableNextColumn();
      SetNextItemWidth(GetContentRegionAvail().x);
      TypeChooser(db, field.FieldType, fltNoCycles(db, def));
      TableNextColumn();
      Text("Initial Value");
      TableNextColumn();
      Text("Properties");
      TableNextColumn();
      SmallButton("Up"); SameLine();
      SmallButton("Down"); SameLine();
      SmallButton("Duplicate"); SameLine();
      SmallButton("Copy"); SameLine();
      SmallButton("Delete"); SameLine();
      PopID();
    }

    EndTable();
  }
}

void EditEnum(Database& db, EnumDefinition* def)
{

}

Database mDatabase;
Database* mCurrentDatabase = &mDatabase;

void TypesTab()
{
  using namespace ImGui;
  if (Button("Add Struct"))
  {
    mCurrentDatabase->AddNewStruct();
  }
  SameLine();
  Button("Add Class"); SameLine();
  Button("Add Enum"); SameLine();
  Text("|");

  for (auto& def : mCurrentDatabase->Definitions)
  {
    PushID(def.get());
    if (auto strukt = dynamic_cast<StructDefinition*>(def.get()))
    {
      if (CollapsingHeader(def->Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
      {
        EditRecord(*mCurrentDatabase, strukt, true);
      }
    }
    else if (auto klass = dynamic_cast<ClassDefinition*>(def.get()))
    {
      if (CollapsingHeader(def->Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
      {
        EditRecord(*mCurrentDatabase, strukt, false);
      }
    }
    else if (auto eenoom = dynamic_cast<EnumDefinition*>(def.get()))
    {
      if (CollapsingHeader(def->Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
      {
        EditEnum(*mCurrentDatabase, eenoom);
      }
    }
    PopID();
  }
}
void DataTab()
{

}
void InterfacesTab()
{

}
void DisplaysTab()
{

}
void ScriptingTab()
{

}

int main(int, char**)
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
  {
    printf("Error: %s\n", SDL_GetError());
    return -1;
  }

  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);

  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
  if (renderer == NULL)
  {
    SDL_Log("Error creating SDL_Renderer!");
    return false;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;

  ImGui::StyleColorsDark();

  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer_Init(renderer);

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // Main loop
  bool done = false;
  while (!done)
  {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
        done = true;
      if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
        done = true;
    }

    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos({}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoDecoration);
    
    if (ImGui::BeginTabBar("Main Tabs"))
    {
      if (ImGui::BeginTabItem("Types"))
      {
        TypesTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Data"))
      {
        DataTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Interfaces"))
      {
        InterfacesTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Displays"))
      {
        DisplaysTab();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Scripting"))
      {
        ScriptingTab();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
    
    ImGui::End();

    // Rendering
    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(renderer);
  }

  // Cleanup
  ImGui_ImplSDLRenderer_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
