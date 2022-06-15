#include "pch.h"

#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include <SDL2/SDL.h>
#undef main
#include <imgui_stdlib.h>
#include <any>

#include "Database.h"

#include "X:\Code\Native\ghassanpl\windows_message_box\windows_message_box.h"

using FilterFunc = std::function<bool(TypeDefinition const*)>;

FilterFunc fltAnyStruct = [](TypeDefinition const* def) { return def && def->IsStruct(); };
FilterFunc fltAnyClass = [](TypeDefinition const* def) { return def && def->IsClass(); };
FilterFunc fltAnyRecord = [](TypeDefinition const* def) { return def && def->IsRecord(); };
FilterFunc fltAnyEnum = [](TypeDefinition const* def) { return def && def->IsEnum(); };
FilterFunc fltAny = [](TypeDefinition const* def) { return def; };

/*
FilterFunc fltNoCycles(Database& db, TypeDefinition const* self, FilterFunc parent = fltAny)
{
	return FilterFunc{ [parent = move(parent), self, &db](TypeDefinition const* def) { return parent(def) && !db.IsParentOrChild(def, self); } };
}
*/

template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using ValidateFunc = function<result<void, string>(Database&, EDITING_OBJECT const*, OBJECT_PROPERTY const&)>;
template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using EditorFunc = function<void(Database&, EDITING_OBJECT const*, OBJECT_PROPERTY&)>;
template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using ApplyFunc = function<result<void, string>(Database&, EDITING_OBJECT const*, OBJECT_PROPERTY const&)>;
template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using GetterFunc = function<OBJECT_PROPERTY const& (Database&, EDITING_OBJECT const*)>;

void Display(Database& db, string const& val) { ImGui::Text("%s", val.c_str()); }
void Display(Database& db, TypeReference const& val) { auto name = val.ToString(); ImGui::Text("%s", name.c_str()); }

template <typename E, typename P>
bool GenericEditor(const char* id, Database& db, E const* def, ValidateFunc<E, P> validate, EditorFunc<E, P> editor, ApplyFunc<E, P> apply, GetterFunc<E, P> getter)
{
	bool changed = false;
	using namespace ImGui;
	static map<E const*, P> is_editing;
	PushID(id);
	PushID(def);

	if (auto it = is_editing.find(def); it == is_editing.end())
	{
		auto const& val = getter(db, def);
		if (validate(db, def, val).has_error())
			is_editing[def] = val;
	}

	if (auto it = is_editing.find(def); it != is_editing.end())
	{
		editor(db, def, it->second);
		if (auto result = validate(db, def, it->second); result.has_error())
		{
			TextColored({ 1,0,0,1 }, "%s", result.error().c_str());
			BeginDisabled();
			SmallButton("Apply");
			EndDisabled();
		}
		else
		{
			if (SmallButton("Apply"))
			{
				auto result = apply(db, def, it->second);
				if (result.has_error())
				{
					/// TODO: Show error
				}
				else
				{
					is_editing.erase(def);
					changed = true;
				}
			}
		}
		SameLine();
		if (SmallButton("Cancel"))
			is_editing.erase(def);
	}
	else
	{
		auto const& val = getter(db, def);
		Display(db, val); SameLine();
		if (SmallButton("Edit"))
			is_editing[def] = val;
	}

	PopID();
	PopID();
	return changed;
}

bool TypeNameEditor(Database& db, TypeDefinition const* def)
{
	return GenericEditor<TypeDefinition, string>("Type Name", db, def,
		&Database::ValidateTypeName,
		[](Database& db, TypeDefinition const* def, string& name) {
			using namespace ImGui;
			SetNextItemWidth(GetContentRegionAvail().x);
			InputText("###typename", &name);
		},
		&Database::SetTypeName,
		[](Database& db, TypeDefinition const* def) -> auto const& { return def->Name(); }
		);
}

bool RecordBaseTypeEditor(Database& db, RecordDefinition const* def)
{
	return GenericEditor<RecordDefinition, TypeReference>("Base Type", db, def,
		&Database::ValidateRecordBaseType,
		[](Database& db, RecordDefinition const* def, TypeReference& current) {
			using namespace ImGui;

			SetNextItemWidth(GetContentRegionAvail().x);
			if (BeginCombo("###basetype", current.ToString().c_str()))
			{
				if (Selectable("[none]", current.Type == nullptr))
					current = TypeReference{};
				for (auto& type : db.Definitions())
				{
					if (def->Type() == type->Type() && !db.IsParent(def, type.get()))
					{
						if (Selectable(type->Name().c_str(), type.get() == current.Type))
							current = TypeReference{ type.get() };
					}
				}
				EndCombo();
			}
		},
		&Database::SetRecordBaseType,
			[](Database& db, RecordDefinition const* def) -> auto const& { return def->BaseType(); }
		);
}

bool FieldNameEditor(Database& db, FieldDefinition const* def)
{
	return GenericEditor<FieldDefinition, string>("Field Name", db, def,
		&Database::ValidateFieldName,
		[](Database& db, FieldDefinition const* def, string& name) {
			using namespace ImGui;
			SetNextItemWidth(GetContentRegionAvail().x);
			InputText("###fieldname", &name);
		},
		&Database::SetFieldName,
			[](Database& db, FieldDefinition const* def) -> auto const& { return def->Name; }
		);
}

FilterFunc fltTemplateArgumentFilter(TemplateParameter const& param)
{
	return [&](TypeDefinition const* def) {
		auto str = ValidateTemplateArgument(TypeReference{ def }, param);
		return !str.has_failure();
	};
}

void TypeChooser(Database& db, TypeReference& ref, FilterFunc filter, const char* label = nullptr)
{
	using namespace ImGui;

	PushID(&ref);
	auto current = ref.ToString();
	if (label == nullptr)
	{
		label = "##typechooser";
		SetNextItemWidth(GetContentRegionAvail().x);
	}
	if (BeginCombo(label, current.c_str()))
	{
		for (auto& type : db.Definitions())
		{
			if (!filter || filter(type.get()))
			{
				if (Selectable(type->Name().c_str(), type.get() == ref.Type))
				{
					ref = TypeReference{ type.get() };
				}
			}
		}
		EndCombo();
	}

	if (ref.Type)
	{
		Indent(8.0f);
		size_t i = 0;
		for (auto& param : ref.Type->TemplateParameters())
		{
			PushID(&param);
			if (param.Qualifier == TemplateParameterQualifier::Size)
			{
				if (!holds_alternative<uint64_t>(ref.TemplateArguments[i]))
					ref.TemplateArguments[i] = uint64_t{};
				InputScalar(param.Name.c_str(), ImGuiDataType_U64, &get<uint64_t>(ref.TemplateArguments[i]));
			}
			else
			{
				if (holds_alternative<uint64_t>(ref.TemplateArguments[i]))
					ref.TemplateArguments[i] = TypeReference{};
				TypeChooser(db, get<TypeReference>(ref.TemplateArguments[i]), fltTemplateArgumentFilter(param), param.Name.c_str());
			}
			PopID();
			++i;
		}
		Unindent(8.0f);
	}

	PopID();
}

bool FieldTypeEditor(Database& db, FieldDefinition const* def)
{
	return GenericEditor<FieldDefinition, TypeReference>("Field Type", db, def,
		&Database::ValidateFieldType,
		[](Database& db, FieldDefinition const* field, TypeReference& current) {
			using namespace ImGui;

			TypeChooser(db, current, [&db, field](TypeDefinition const* def) { return !def->IsClass() && !db.IsParent(field->ParentRecord, def); });
		},
		&Database::SetFieldType,
		[](Database& db, FieldDefinition const* def) -> auto const& { return def->FieldType; }
		);
}

void CheckError(result<void, string> val)
{

}

vector<function<void()>> LateExec;
vector<pair<string, function<bool()>>> Modals;

void ShowOptions(int& chosen, span<string> options)
{
	using namespace ImGui;
	if (BeginCombo("Action", chosen == -1 ? "Choose action..." : options[chosen].c_str()))
	{
		int i = 0;
		for (auto& option : options)
		{
			if (Selectable(option.c_str(), chosen == i))
				chosen = i;
			++i;
		}
		EndCombo();
	}
}

template <convertible_to<string_view>... ARGS>
void ShowOptions(int& chosen, ARGS&&... args)
{
	vector<string> options = { string{forward<ARGS>(args)}... };
	ShowOptions(chosen, span{ options });
}

void ShowUsageHandleUI(TypeDefinition const* def_to_delete, pair<int, std::any>& settings, Database::TypeUsedInFieldType const& usage)
{
	using namespace ImGui;

	/// TODO: Add option to serialize values from this field/subfield into the 'json' type

	if (&usage.Record->Field(usage.FieldIndex)->FieldType != usage.Reference)
	{
		/// TODO: Check if changing template argument type to void is at all possible
		/// e.g. ValidateFieldType(field, changed_type);
		ShowOptions(settings.first,
			"Remove Field",
			"Change Field Type to 'void'",
			"Change Template Argument to 'void'");
	}
	else
	{
		ShowOptions(settings.first,
			"Remove Field",
			"Change Field Type to 'void'");
	}
	switch (settings.first)
	{
	case 0:
		Text("NOTE: Removing this field will also remove all data stored in this field!");
		break;
	case 1:
		Text("NOTE: Changing the field type to 'void' will also remove all data stored in this field!");
		break;
	case 2:
		Text("NOTE: Changing the field type will trigger a data update which may destroy or corrupt data held in this field!");
		break;
	}
}

void ShowUsageHandleUI(TypeDefinition const* def_to_delete, pair<int, std::any>& settings, Database::TypeIsBaseTypeOf const& usage)
{
	using namespace ImGui;
	vector<string> options = { 
		format("Set Base Type of '{}' to none", usage.ChildType->Name()) 
	};
	if (def_to_delete->BaseType().Type)
		options.push_back(format("Set Base Type of '{}' to '{}' (parent of '{}')", usage.ChildType->Name(), def_to_delete->BaseType().ToString(), def_to_delete->Name()));
	
	ShowOptions(settings.first, options);

	if (!settings.second.has_value())
		settings.second = true;
	auto move_txt = format("Move '{}' Fields to '{}'", def_to_delete->Name(), usage.ChildType->Name());
	Checkbox(move_txt.c_str(), &std::any_cast<bool&>(settings.second));
}

void ShowUsageHandleUI(TypeDefinition const* def_to_delete, pair<int, std::any>& settings, Database::TypeHasDataInDataStore const& usage)
{
	settings.first = 0;
	ImGui::Text("NOTE: All data of this type will be deleted");
	/// TODO: Option to perform a data export of just this type
}

pair<string, function<bool()>> DeleteTypeModal(Database& db, RecordDefinition* def, vector<Database::TypeUsage> usages)
{
	using namespace ImGui;
	vector<pair<int, std::any>> options;
	options.resize(usages.size(), { -1, {} });
	return { "Deleting Type", [&db, def, usages = move(usages), make_backup = true, options = move(options)]() mutable {
		auto text = format("You are trying to delete the type '{}' which is in use in {} places. Please decide how to handle each use:", def->Name(), usages.size());

		Text("%s", text.c_str());

		int i = 0;
		for (auto& usage : usages)
		{
			visit([&](auto& usage) { 
				PushID(&usage);
				auto usage_text = db.Stringify(usage);
				BulletText("%s", usage_text.c_str());
				Indent();
				ShowUsageHandleUI(def, options[i], usage);
				Unindent();
				PopID();
			}, usage);
			++i;
		}

		Spacing();
		Checkbox("Make Database Backup", &make_backup);

		Spacing();

		auto chosen_all_options = ranges::all_of(options, [](auto const& opt) { return opt.first != -1; });
		
		bool close = false;
		BeginDisabled(!chosen_all_options);
		if (Button("Proceed with Changes"))
		{
			if (make_backup)
			{
				if (auto result = db.CreateBackup(); result.has_error())
				{
					auto err = format("Backup could not be created: {}", result.error());
					::ghassanpl::windows_message_box({ ::ghassanpl::msg::title{"Backup creation failed"}, ::ghassanpl::msg::description{err},
						::ghassanpl::msg::ok_button });
					close = true;
				}
			}

			if (!close)
			{
				set<string> removed_field;
				for (size_t i = 0; i < options.size(); ++i)
				{
					auto& usage = usages[i];
					auto& option = options[i];

					if (auto u = get_if<Database::TypeUsedInFieldType>(&usage))
					{
						if (removed_field.contains(u->))
						if (option.first == 0)
					}
				}
				///db.ApplyChanges(usages);
			}

			close = true;
		}
		EndDisabled();
		SameLine();
		if (Button("Cancel Changes"))
			close = true;
		return close;
	}};
}

void EditRecord(Database& db, RecordDefinition* def, bool is_struct)
{
	using namespace ImGui;
	Text("Name: "); SameLine(); TypeNameEditor(db, def);
	Text("Base Type: "); SameLine(); RecordBaseTypeEditor(db, def);

	if (Button("Add Field"))
	{
		ignore = db.AddNewField(def);
	}
	SameLine();

	Button("Delete Type");
	if (BeginPopupContextItem("Are you sure you want to delete this type?", 0))
	{
		Text("Are you sure you want to delete this type?");
		if (Button("Yes"))
		{
			auto usages = db.ValidateDeleteType(def);
			if (usages.empty())
				LateExec.push_back([&db, def] { CheckError(db.DeleteType(def)); });
			else
				Modals.push_back(DeleteTypeModal(db, def, move(usages)));
			CloseCurrentPopup();
		}
		SameLine();
		if (Button("No"))
			CloseCurrentPopup();
		EndPopup();
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
		size_t index = 0;
		for (auto& field : def->Fields())
		{
			PushID(&field);
			TableNextColumn();
			SetNextItemWidth(GetContentRegionAvail().x);
			FieldNameEditor(db, field.get());
			TableNextColumn();
			SetNextItemWidth(GetContentRegionAvail().x);
			FieldTypeEditor(db, field.get());
			TableNextColumn();
			Text("Initial Value");
			TableNextColumn();
			Text("Properties");
			TableNextColumn();

			BeginDisabled(index == 0);
			if (SmallButton("Up"))
				LateExec.push_back([&db, def, index] { CheckError(db.SwapFields(def, index, index - 1)); });
			EndDisabled();
			SameLine();
			
			BeginDisabled(index == def->Fields().size() - 1);
			if (SmallButton("Down"))
				LateExec.push_back([&db, def, index] { CheckError(db.SwapFields(def, index, index + 1)); });
			EndDisabled();
			SameLine();
			
			SmallButton("Duplicate"); SameLine();
			SmallButton("Copy"); SameLine();
			SmallButton("Delete");
			if (BeginPopupContextItem("Are you sure you want to delete this field?", 0))
			{
				Text("Are you sure you want to delete this field?");
				if (Button("Yes"))
				{
					LateExec.push_back([&db, field = field.get()] { CheckError(db.DeleteField(field)); });
					CloseCurrentPopup();
				}
				SameLine();
				if (Button("No"))
					CloseCurrentPopup();
				EndPopup();
			}
			SameLine();

			PopID();
			index++;
		}

		EndTable();
	}
}

void EditEnum(Database& db, EnumDefinition* def)
{

}

Database mDatabase{ "test/db1/" };
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

	for (auto& def : mCurrentDatabase->Definitions())
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

void PropertiesTab()
{
	using namespace ImGui;
	auto dir = mCurrentDatabase->Directory().string();
	LabelText("Directory", "%s", dir.c_str());

	InputText("Namespace", &mCurrentDatabase->Namespace);
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
			if (ImGui::BeginTabItem("Properties"))
			{
				PropertiesTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::End();

		for (auto& exec : LateExec)
			exec();
		LateExec.clear();

		bool close_modal = false;
		for (auto& [name, modal] : Modals)
		{
			ImGui::OpenPopup(name.c_str());
			if (ImGui::BeginPopupModal(name.c_str()))
			{
				close_modal = modal();
				ImGui::End();
			}
		}
		if (close_modal)
			Modals.pop_back();

		// Rendering
		ImGui::Render();
		SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(renderer);
	}

	mCurrentDatabase->SaveAll();

	// Cleanup
	ImGui_ImplSDLRenderer_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
