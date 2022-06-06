#include "pch.h"

#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include <SDL2/SDL.h>
#undef main
#include <imgui_stdlib.h>

#include "Database.h"

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

string ValidateTemplateArgument(TemplateParameterQualifier qualifier, TypeDefinition const* arg_type)
{
  switch (qualifier)
  {
  case TemplateParameterQualifier::AnyType: break;
  case TemplateParameterQualifier::Struct:
    if (arg_type->Type() != DefinitionType::Struct)
      return "must be a struct type";
    break;
  case TemplateParameterQualifier::NotClass:
    if (arg_type->Type() == DefinitionType::Class)
      return "must be a non-class type";
    break;
  case TemplateParameterQualifier::Enum:
    if (arg_type->Type() != DefinitionType::Enum)
      return "must be an enum type";
    break;
  case TemplateParameterQualifier::Integral:
  case TemplateParameterQualifier::Floating:
  case TemplateParameterQualifier::Simple:
  case TemplateParameterQualifier::Pointer:
    if (arg_type->Type() != DefinitionType::BuiltIn)
      return "must be an integral type";
    else
    {
      auto builtin_type = dynamic_cast<BuiltinDefinition const*>(arg_type);
      if (builtin_type->ApplicableQualifiers().is_set(qualifier) == false)
        return format("must be a {} type", string_ops::ascii::tolower(magic_enum::enum_name(qualifier)));
    }
    break;
  case TemplateParameterQualifier::Class:
    if (arg_type->Type() != DefinitionType::Class)
      return "must be a class type";
    break;
  default:
    throw format("internal error: unimplemented template parameter qualifier `{}`", magic_enum::enum_name(qualifier));
  }

  return {};
}

FilterFunc fltTemplateArgumentFilter(TemplateParameterQualifier qualifier)
{
  return [qualifier](TypeDefinition const* def) { 
    auto str = ValidateTemplateArgument(qualifier, def);
    return str.empty();
  };
}

/*
void TypeChooser(Database& db, TypeReference& ref, function<bool(TypeDefinition const*)> filter = {}, const char* name = "")
{
  using namespace ImGui;
  PushID(&ref);
  auto current = ref.ToString();
  if (BeginCombo(name, current.c_str()))
  {
    for (auto& type : db.Definitions)
    {
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
      if (param.Qualifier == TemplateParameterQualifier::Size)
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
*/

void TypeNameEditor(Database& db, TypeDefinition const* def)
{
  using namespace ImGui;
  static map<TypeDefinition const*, string> is_editing;

  PushID(def);

  if (is_editing.contains(def))
  {
    //SetNextItemWidth(GetContentRegionAvail().x / 3);
    InputText("Name", &is_editing[def]);
    SameLine();

    if (SmallButton("Apply"))
    {
      auto result = db.SetTypeName(def, is_editing[def]);
      if (result.has_error())
      {
        /// TODO: Show error
      }
      else
        is_editing.erase(def);
    }
    SameLine();
    if (SmallButton("Cancel"))
      is_editing.erase(def);
  }
  else
  {
    Text("Name: %s", def->Name().c_str()); SameLine(); 
    if (SmallButton("Edit"))
      is_editing[def] = def->Name();
  }

  PopID();
}

void RecordBaseTypeEditor(Database& db, RecordDefinition const* def)
{
  using namespace ImGui;
  static map<RecordDefinition const*, TypeReference> is_editing;

  PushID(def);

  if (is_editing.contains(def))
  {
    auto& current = is_editing[def];
    SetNextItemWidth(GetContentRegionAvail().x / 3);
    if (BeginCombo("Base Type", current.ToString().c_str()))
    {
      if (Selectable("[none]"))
        current = TypeReference{};
      for (auto& [name, type] : db.Definitions())
      {
        if (def->Type() == type->Type() && !db.IsParentOrChild(def, type.get()))
        {
          if (Selectable(type->Name().c_str(), type.get() == current.Type))
            current = TypeReference{ type.get()};
        }
      }
      EndCombo();
    }
    if (SmallButton("Apply"))
    {
      auto result = db.SetRecordBaseType(def, is_editing[def]);
      if (result.has_error())
      {
        /// TODO: Show error
      }
      else
        is_editing.erase(def);
    }
    SameLine();
    if (SmallButton("Cancel"))
      is_editing.erase(def);
  }
  else
  {
    string name = format("Base Type: {}", def->BaseType().ToString());
    Text("%s", name.c_str()); SameLine();
    if (SmallButton("Edit"))
      is_editing[def] = def->BaseType();
  }
  PopID();
}

void FieldNameEditor(Database& db, FieldDefinition const* def)
{
  using namespace ImGui;
  static map<FieldDefinition const*, string> is_editing;
  if (is_editing.contains(def))
  {
    //SetNextItemWidth(GetContentRegionAvail().x / 3);

    if (SmallButton("Apply"))
    {
      auto result = db.SetFieldName(def, is_editing[def]);
      if (result.has_error())
      {
        /// TODO: Show error
      }
      else
        is_editing.erase(def);
    }
    SameLine();
    if (SmallButton("Cancel"))
      is_editing.erase(def);
  }
  else
  {
    Text("%s", def->Name.c_str()); SameLine();
    if (SmallButton("Edit"))
      is_editing[def] = def->Name;
  }
}

void FieldTypeEditor(Database& db, FieldDefinition const* def)
{
  using namespace ImGui;
  static map<FieldDefinition const*, TypeReference> is_editing;

  if (is_editing.contains(def))
  {
    auto& current = is_editing[def];
    PushID(def);
    if (BeginCombo("", current.ToString().c_str()))
    {
      for (auto& [name, type] : db.Definitions())
      {
        if (!db.IsParentOrChild(def->ParentRecord, type.get()))
        {
          if (Selectable(type->Name().c_str(), type.get() == current.Type))
            current = TypeReference{ type.get() };
        }
      }
      EndCombo();
    }

    /*
    if (ref.Type)
    {
      Indent(8.0f);
      size_t i = 0;
      for (auto& param : ref.Type->TemplateParameters)
      {
        PushID(&param);
        //param.QualifierRequiresCompletedType
        if (param.Qualifier == TemplateParameterQualifier::Size)
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
    */

    if (SmallButton("Apply"))
    {
      auto result = db.SetFieldType(def, is_editing[def]);
      if (result.has_error())
      {
        /// TODO: Show error
      }
      else
        is_editing.erase(def);
    }
    SameLine();
    if (SmallButton("Cancel"))
      is_editing.erase(def);
    PopID();
  }
  else
  {
    string name = def->FieldType.ToString();
    Text("%s", name.c_str()); SameLine();
    if (SmallButton("Edit"))
      is_editing[def] = def->FieldType;
  }
  //TypeChooser<true>(db, def->BaseType(), fltNoCycles(db, def, def->IsStruct() ? fltAnyStruct : fltAnyClass));
}

void EditRecord(Database& db, RecordDefinition* def, bool is_struct)
{
  using namespace ImGui;
  TypeNameEditor(db, def);
  SameLine();
  RecordBaseTypeEditor(db, def);
  
  if (Button("Add Field"))
  {
    ignore = db.AddNewField(def);
    //def->Fields.push_back(FieldDefinition{ "field1" });
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
    for (auto& field : def->Fields())
    {
      PushID(&field);
      TableNextColumn();
      SetNextItemWidth(GetContentRegionAvail().x);
      FieldNameEditor(db, field.get());
      TableNextColumn();
      SetNextItemWidth(GetContentRegionAvail().x);
      FieldTypeEditor(db, field.get());
      //TypeChooser(db, field.FieldType, fltNoCycles(db, def));
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
    ignore = mCurrentDatabase->AddNewStruct();
  SameLine();
  Button("Add Class"); SameLine();
  Button("Add Enum"); SameLine();
  Text("|");

  for (auto& [name, def] : mCurrentDatabase->Definitions())
  {
    PushID(def.get());
    if (auto strukt = dynamic_cast<StructDefinition*>(def.get()))
    {
      if (CollapsingHeader(def->Name().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
      {
        EditRecord(*mCurrentDatabase, strukt, true);
      }
    }
    else if (auto klass = dynamic_cast<ClassDefinition*>(def.get()))
    {
      if (CollapsingHeader(def->Name().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
      {
        EditRecord(*mCurrentDatabase, strukt, false);
      }
    }
    else if (auto eenoom = dynamic_cast<EnumDefinition*>(def.get()))
    {
      if (CollapsingHeader(def->Name().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
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
