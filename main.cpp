#include "pch.h"
#include "pch.h"

#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include <SDL2/SDL.h>
#undef main
#include <imgui_stdlib.h>
#include <any>

#include "Database.h"
#include "Validation.h"
#include "Values.h"

#include "X:\Code\Native\ghassanpl\windows_message_box\windows_message_box.h"

using FilterFunc = std::function<bool(TypeDefinition const*)>;

struct IModal
{
	virtual ~IModal() noexcept = default;

	virtual void Do() = 0;
	virtual string WindowName() const = 0;

	bool Close = false;
};

/// TODO: Icons for Error and Success modals
struct ErrorModal : IModal
{
	variant<string, function<void()>> Message;
	ErrorModal(variant<string, function<void()>> msg) : Message(move(msg)) {  }
	virtual void Do() override
	{
		if (auto str = get_if<string>(&Message))
			ImGui::Text("%s", str->c_str());
		else
			get<1>(Message)();
		ImGui::Spacing();
		if (ImGui::Button("OK"))
			Close = true;
	}
	virtual string WindowName() const override { return "Error"; }
};

struct SuccessModal : IModal
{
	variant<string, function<void()>> Message;
	SuccessModal(variant<string, function<void()>> msg) : Message(move(msg)) {  }
	virtual void Do() override
	{
		if (auto str = get_if<string>(&Message))
			ImGui::Text("%s", str->c_str());
		else
			get<1>(Message)();
		ImGui::Spacing();
		if (ImGui::Button("OK"))
			Close = true;
	}
	virtual string WindowName() const override { return "Success"; }
};

vector<unique_ptr<IModal>> Modals;

template <typename T, typename... ARGS>
void OpenModal(ARGS&&... args)
{
	Modals.push_back(make_unique<T>(forward<ARGS>(args)...));
}

template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using ValidateFunc = function<result<void, string>(EDITING_OBJECT, OBJECT_PROPERTY const&)>;
template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using EditorFunc = function<void(EDITING_OBJECT, OBJECT_PROPERTY&)>;
template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using ApplyFunc = function<result<void, string>(EDITING_OBJECT, OBJECT_PROPERTY const&)>;
template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using GetterFunc = function<OBJECT_PROPERTY(EDITING_OBJECT)>;

void Display(string const& val) { ImGui::Text("%s", val.c_str()); }
void Display(TypeReference const& val) { auto name = val.ToString(); ImGui::Text("%s", name.c_str()); }

template <typename E, typename P>
bool GenericEditor(const char* id, Database& db, E def, ValidateFunc<E, P> validate, EditorFunc<E, P> editor, ApplyFunc<E, P> apply, GetterFunc<E, P> getter)
{
	bool changed = false;
	using namespace ImGui;
	static map<E, P> is_editing;
	PushID(id);
	PushID(def);

	if (auto it = is_editing.find(def); it == is_editing.end())
	{
		decltype(auto) val = getter(def);
		if (validate(def, val).has_error())
			is_editing[def] = val;
	}

	if (auto it = is_editing.find(def); it != is_editing.end())
	{
		editor(def, it->second);
		if (auto result = validate(def, it->second); result.has_error())
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
				auto result = apply(def, it->second);
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
		decltype(auto) val = getter(def);
		Display(val); SameLine();
		if (SmallButton("Edit"))
			is_editing[def] = val;
	}

	PopID();
	PopID();
	return changed;
}

bool TypeNameEditor(Database& db, TypeDefinition const* def)
{
	return GenericEditor<TypeDefinition const*, string>("Type Name", db, def,
		bind_front(&Database::ValidateTypeName, &db),
		[](TypeDefinition const* def, string& name) {
			using namespace ImGui;
			SetNextItemWidth(GetContentRegionAvail().x);
			InputText("###typename", &name);
		},
		bind_front(&Database::SetTypeName, &db),
		[](TypeDefinition const* def) -> auto const& { return def->Name(); }
	);
}

bool RecordBaseTypeEditor(Database& db, RecordDefinition const* def)
{
	return GenericEditor<RecordDefinition const*, TypeReference>("Base Type", db, def,
		bind_front(&Database::ValidateRecordBaseType, &db),
		[&db](RecordDefinition const* def, TypeReference& current) {
			using namespace ImGui;

			SetNextItemWidth(GetContentRegionAvail().x);
			if (BeginCombo("###basetype", current.ToString().c_str()))
			{
				if (Selectable("[none]", current.Type == nullptr))
					current = TypeReference{};
				for (auto type : db.Definitions())
				{
					if (def->Type() == type->Type() && !db.Schema().IsParent(def, type))
					{
						if (Selectable(type->Name().c_str(), type == current.Type))
							current = TypeReference{ type };
					}
				}
				EndCombo();
			}
		},
		bind_front(&Database::SetRecordBaseType, &db),
		[](RecordDefinition const* def) -> auto const& { return def->BaseType(); }
	);
}

bool FieldNameEditor(Database& db, FieldDefinition const* def)
{
	return GenericEditor<FieldDefinition const*, string>("Field Name", db, def,
		bind_front(&Database::ValidateFieldName, &db),
		[](FieldDefinition const* def, string& name) {
			using namespace ImGui;
			SetNextItemWidth(GetContentRegionAvail().x);
			InputText("###fieldname", &name);
		},
		bind_front(&Database::SetFieldName, &db),
		[](FieldDefinition const* def) -> auto const& { return def->Name; }
	);
}

void TypeChooser(Database& db, TypeReference& ref, FilterFunc filter = {}, const char* label = nullptr)
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
		for (auto type : db.Definitions())
		{
			if (!filter || filter(type))
			{
				if (Selectable(type->Name().c_str(), type == ref.Type))
				{
					ref = TypeReference{ type };
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
				TypeChooser(db, get<TypeReference>(ref.TemplateArguments[i]), {}, param.Name.c_str());
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
	return GenericEditor<FieldDefinition const*, TypeReference>("Field Type", db, def,
		&ValidateFieldType,
		[&db](FieldDefinition const* field, TypeReference& current) {
			using namespace ImGui;

			TypeChooser(db, current, [&db, field](TypeDefinition const* def) { return !def->IsClass() && !db.Schema().IsParent(field->ParentRecord, def); });
		},
		bind_front(&Database::SetFieldType, &db),
		[](FieldDefinition const* def) -> auto const& { return def->FieldType; }
	);
}

void CheckError(result<void, string> val, string else_string)
{
	if (val.has_error())
		OpenModal<ErrorModal>(val.error());
	else if (!else_string.empty())
		OpenModal<SuccessModal>(move(else_string));
}

vector<function<void()>> LateExec;

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

TypeReference* GetTypeRef(TypeReference& ref, span<size_t const> s)
{
	if (s.empty())
		return &ref;
	if (s[0] >= ref.TemplateArguments.size())
		return nullptr;
	return GetTypeRef(get<TypeReference>(ref.TemplateArguments[s[0]]), s.subspan(1));
}

TypeReference Change(Database const& db, TypeUsedInFieldType const& usage)
{
	TypeReference old_type = usage.Field->FieldType;
	for (auto& ref : usage.References)
	{
		if (auto subref = GetTypeRef(old_type, ref))
			subref->Type = db.VoidType();
	}
	return old_type;
}

void ShowUsageHandleUI(Database const& db, TypeDefinition const* def_to_delete, pair<int, std::any>& settings, TypeUsedInFieldType const& usage)
{
	using namespace ImGui;

	/// TODO: Add option to serialize values from this field/subfield into the 'json' type
	/// TODO: Check if changing template argument type to void is at all possible
	/// e.g. ValidateFieldType(field, changed_type);
	ShowOptions(settings.first,
		"Remove Field",
		format("Change All Occurances of '{}' to 'void'", def_to_delete->Name()));
	switch (settings.first)
	{
	case 0:
		Text("NOTE: Removing this field will also remove all data stored in this field!");
		break;
	case 1:
	{
		auto changed = Change(db, usage);
		auto str = format("Change: var {0} : {1}; -> var {0} : {2};", usage.Field->Name, usage.Field->FieldType.ToString(), changed.ToString());
		Text("%s", str.c_str());
		Spacing();
	}
	if (usage.References.size() == 1 && usage.References[0].empty())
		Text("NOTE: Changing the field type to 'void' will also remove all data stored in this field!");
	else
		Text("NOTE: Changing the field type will trigger a data update which may destroy or corrupt data held in this field!");
	break;
	}
}

result<void, string> ApplyChangeOption(Database& db, TypeUsedInFieldType const& usage, pair<int, std::any>& settings)
{
	switch (settings.first)
	{
	case 0:
		return db.SetFieldType(usage.Field, Change(db, usage));
	case 1:
		return db.DeleteField(usage.Field);
	}
	throw runtime_error("invalid change option");
}

void ShowUsageHandleUI(Database const& db, TypeDefinition const* def_to_delete, pair<int, std::any>& settings, TypeIsBaseTypeOf const& usage)
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

result<void, string> ApplyChangeOption(Database& db, TypeIsBaseTypeOf const& usage, pair<int, std::any>& settings)
{
	switch (settings.first)
	{
	case 0:
		if (any_cast<bool>(settings.second))
			return db.CopyFieldsAndMoveUpBaseTypeHierarchy(usage.ChildType);
		else
			return db.SetRecordBaseType(usage.ChildType, {});
	case 1:
		if (any_cast<bool>(settings.second))
			return db.CopyFieldsAndMoveUpBaseTypeHierarchy(usage.ChildType);
		else
			return db.SetRecordBaseType(usage.ChildType, TypeReference{ usage.ChildType });
	}
	throw runtime_error("invalid change option");
}

void ShowUsageHandleUI(Database const& db, TypeDefinition const* def_to_delete, pair<int, std::any>& settings, TypeHasDataInDataStore const& usage)
{
	settings.first = 0;
	ImGui::Text("NOTE: All data of this type will be deleted");
	/// TODO: Option to perform a data export of just this type
}

result<void, string> ApplyChangeOption(Database& db, TypeHasDataInDataStore const& usage, pair<int, std::any>& settings)
{
	/// This will be taken care of by the db deleting the type

	/// TODO: perform data export if chosen

	return success();
}


struct DeleteFieldModal : IModal
{
	Database& mDB;
	FieldDefinition const* mField;
	vector<string> mStores;
	vector<char> mSettings;
	bool mMakeBackup = true;

	DeleteFieldModal(Database& db, FieldDefinition const* def, vector<string> stores_with_data)
		: mDB(db)
		, mField(def)
		, mStores(move(stores_with_data))
		, mSettings(mStores.size(), false)
	{

	}

	virtual void Do() override
	{
		using namespace ImGui;

		auto text = format("You are trying to delete the field '{}.{}' which is in use in {} data stores. Please decide how to handle each store:", mField->ParentRecord->Name(), mField->Name, mStores.size());

		Text("%s", text.c_str());

		size_t i = 0;
		for (auto& store : mStores)
		{
			BulletText("Storage '': ");
			SameLine();
			Checkbox("Make data backup", (bool*)&mSettings[i]);
			i++;
		}

		if (Button("Proceed with Changes"))
		{
			Close = true;
		}
		SameLine();
		if (Button("Cancel Changes"))
			Close = true;
	}

	virtual string WindowName() const override
	{
		return "Deleting Field";
	}
};

struct DeleteTypeModal : IModal
{
	Database& mDB;
	TypeDefinition const* mType;
	vector<TypeUsage> mUsages;
	vector<pair<int, std::any>> mSettings;
	bool mMakeBackup = true;

	DeleteTypeModal(Database& db, TypeDefinition const* def, vector<TypeUsage> usages)
		: mDB(db)
		, mType(def)
		, mUsages(move(usages))
		, mSettings(mUsages.size(), { -1, {} })
	{

	}

	virtual void Do() override
	{
		using namespace ImGui;

		auto text = format("You are trying to delete the type '{}' which is in use in {} places. Please decide how to handle each use:", mType->Name(), mUsages.size());

		Text("%s", text.c_str());

		int i = 0;
		for (auto& usage : mUsages)
		{
			visit([&](auto& usage) {
				PushID(&usage);
				auto usage_text = Describe(usage);
				BulletText("%s", usage_text.c_str());
				Indent();
				ShowUsageHandleUI(mDB, mType, mSettings[i], usage);
				Unindent();
				PopID();
				}, usage);
			++i;
		}

		Spacing();
		Checkbox("Make Database Backup", &mMakeBackup);

		Spacing();

		auto chosen_all_options = ranges::all_of(mSettings, [](auto const& opt) { return opt.first != -1; });

		BeginDisabled(!chosen_all_options);
		if (Button("Proceed with Changes"))
		{
			if (mMakeBackup)
			{
				if (auto result = mDB.CreateBackup(); result.has_error())
				{
					auto err = format("Backup could not be created: {}", result.error());
					::ghassanpl::windows_message_box({ ::ghassanpl::msg::title{"Backup creation failed"}, ::ghassanpl::msg::description{err},
						::ghassanpl::msg::ok_button });
					Close = true;
				}
			}

			if (!Close)
			{
				vector<string> issues;
				for (size_t i = 0; i < mSettings.size(); ++i)
				{
					auto& usage = mUsages[i];
					auto& option = mSettings[i];

					visit([&](auto& usage) { auto result = ApplyChangeOption(mDB, usage, option); if (result.has_error()) issues.push_back(result.error()); }, usage);
				}
				if (issues.empty())
				{
					auto result = mDB.DeleteType(mType);
					if (result.has_error())
						OpenModal<ErrorModal>(format("Type not deleted:\n\n{}", result.error()));
				}
				else
				{
					OpenModal<ErrorModal>(format("Type not deleted - Could not apply the chosen changes due to the following issues:\n\n{}", string_ops::join(issues, "\n")));
				}
			}

			Close = true;
		}
		EndDisabled();
		SameLine();
		if (Button("Cancel Changes"))
			Close = true;
	}

	virtual string WindowName() const override
	{
		return "Deleting Type";
	}
};

template <typename FUNC>
void DoConfirmUI(string confirm_text, FUNC&& func)
{
	using namespace ImGui;
	if (BeginPopupContextItem(confirm_text.c_str(), 0))
	{
		Text("%s", confirm_text.c_str());
		if (Button("Yes"))
		{
			func();
			CloseCurrentPopup();
		}
		SameLine();
		if (Button("No"))
			CloseCurrentPopup();
		EndPopup();
	}
}

void DoDeleteTypeUI(Database& db, TypeDefinition const* def)
{
	ImGui::Button("Delete Type");
	DoConfirmUI("Are you sure you want to delete this type?", [&db, def]() {
		auto usages = db.LocateTypeUsages(def);
		if (usages.empty())
			LateExec.push_back([&db, def] { CheckError(db.DeleteType(def)); });
		else
			OpenModal<DeleteTypeModal>(db, def, move(usages));
		}
	);
}

void DoDeleteValueUI(DataStore& store, string_view name)
{
	ImGui::SmallButton("Delete Value");
	DoConfirmUI("Are you sure you want to delete this value?", [&store, name]() {
		LateExec.push_back([&store, name] { store.DeleteValue(name); });
	});
}

void EditRecord(Database& db, RecordDefinition const* def, bool is_struct)
{
	using namespace ImGui;
	Text("Name: "); SameLine(); TypeNameEditor(db, def);
	Text("Base Type: "); SameLine(); RecordBaseTypeEditor(db, def);

	if (Button("Add Field"))
	{
		ignore = db.AddNewField(def);
	}
	SameLine();

	DoDeleteTypeUI(db, def);

	if (BeginTable("Fields", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
	{
		TableSetupColumn("Name");
		TableSetupColumn("Type");
		TableSetupColumn("Attributes");
		TableSetupColumn("Actions");
		TableSetupScrollFreeze(0, 1);
		TableHeadersRow();

		TableNextRow();
		int index = 0;
		for (auto& field : def->AllFieldsOrdered())
		{
			PushID(index);

			if (field->ParentRecord != def)
			{
				TableNextColumn();
				auto name = format("{}.{}", field->ParentRecord->Name(), field->Name);
				TextDisabled("%s", name.c_str());
				TableNextColumn();
				auto type = format("{}", field->FieldType.ToString());
				TextDisabled("%s", type.c_str());
				TableNextColumn();
				Text("Attributes");
				TableNextColumn();
				SmallButton("Move to Child");
			}
			else
			{
				TableNextColumn();
				SetNextItemWidth(GetContentRegionAvail().x);
				FieldNameEditor(db, field);
				TableNextColumn();
				SetNextItemWidth(GetContentRegionAvail().x);
				FieldTypeEditor(db, field);
				TableNextColumn();
				Text("Attributes");
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
				DoConfirmUI("Are you sure you want to delete this field?", [&db, field]() {
					auto usages = db.StoresWithFieldData(field);
					if (usages.empty())
						LateExec.push_back([&db, field] { CheckError(db.DeleteField(field)); });
					else
						OpenModal<DeleteFieldModal>(db, field, move(usages));
				});
			}

			PopID();
			index++;
		}

		EndTable();
	}
}

void EditEnum(Database& db, EnumDefinition const* enoom)
{
	/*
	using namespace ImGui;
	Text("Name: "); SameLine(); TypeNameEditor(db, enoom);

	if (Button("Add Enumerator"))
	{
		ignore = db.AddNewEnumerator(enoom);
	}
	SameLine();

	Button("Sort By Value");
	SameLine();

	DoDeleteTypeUI(db, enoom);

	/// TODO: PropertyEditor<bool>(enoom, "backcomp", "Try To Ensure Backwards Compatibility");

	if (BeginTable("Enumerators", 5, ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable))
	{
		TableSetupColumn("Name");
		TableSetupColumn("Value");
		TableSetupColumn("Descriptive Name");
		TableSetupColumn("Attributes");
		TableSetupColumn("Actions");
		TableSetupScrollFreeze(0, 1);
		TableHeadersRow();

		TableNextRow();
		size_t index = 0;
		for (auto& enumerator : enoom->Enumerators())
		{
			PushID(index);

			TableNextColumn();
			SetNextItemWidth(GetContentRegionAvail().x);
			FieldNameEditor(db, field);
			TableNextColumn();
			SetNextItemWidth(GetContentRegionAvail().x);
			FieldTypeEditor(db, field);
			TableNextColumn();
			Text("Initial Value");
			TableNextColumn();
			Text("Attributes");
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
					auto usages = db.StoresWithFieldData(field);
					if (usages.empty())
						LateExec.push_back([&db, field] { CheckError(db.DeleteField(field)); });
					else
						OpenModal<DeleteFieldModal>(db, field, move(usages));

					CloseCurrentPopup();
				}
				SameLine();
				if (Button("No"))
					CloseCurrentPopup();
				EndPopup();
			}

			PopID();
			index++;
		}

		EndTable();
	}
	*/
}

Database mDatabase{ "test/db1/" };
Database* mCurrentDatabase = &mDatabase;

void TypesTab()
{
	using namespace ImGui;
	Spacing();
	if (Button("Add Struct"))
		ignore = mCurrentDatabase->AddNewStruct();
	SameLine();
	Button("Add Class"); SameLine();
	if (Button("Add Enum"))
		ignore = mCurrentDatabase->AddNewEnum();
	SameLine();
	Button("Add Union"); SameLine();
	Button("Add Alias");

	Spacing();
	Separator();
	Spacing();

	for (auto def : mCurrentDatabase->Definitions())
	{
		PushID(def);
		if (auto strukt = dynamic_cast<StructDefinition const*>(def))
		{
			if (CollapsingHeader(def->Name().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				Indent();
				EditRecord(*mCurrentDatabase, strukt, true);
				Unindent();
			}
		}
		else if (auto klass = dynamic_cast<ClassDefinition const*>(def))
		{
			if (CollapsingHeader(def->Name().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				Indent();
				EditRecord(*mCurrentDatabase, strukt, false);
				Unindent();
			}
		}
		else if (auto eenoom = dynamic_cast<EnumDefinition const*>(def))
		{
			if (CollapsingHeader(def->Name().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
			{
				Indent();
				EditEnum(*mCurrentDatabase, eenoom);
				Unindent();
			}
		}
		PopID();
	}
}

void DataTab()
{
	using namespace ImGui;
	if (BeginTabBar("Data Stores"))
	{
		for (auto& [name, store] : mCurrentDatabase->DataStores())
		{
			if (BeginTabItem(name.c_str()))
			{
				if (Button("Add Value"))
				{
					auto name = FreshName("Value", [&](string_view name) { return store.HasValue(name); });
					//store.AddValue(json::object({ { "name", name }, { "type", TypeReference{ mCurrentDatabase->VoidType() }.ToJSON()}, {"value", json{}}}));
					store.AddValue(name, TypeReference{ mCurrentDatabase->VoidType() });
				}
				SameLine();
				if (Button("Save Data"))
				{

				}
				SameLine();
				if (Button("Revert Data"))
				{

				}
				SameLine();
				if (Button("Import Value from JSON"))
				{

				}
				SameLine();
				if (Button("Merge Another Data Store"))
				{

				}
				SameLine();
				if (Button("Delete Data Store"))
				{

				}
				SameLine();

				static bool show_json = false;
				Checkbox("Show JSON", &show_json);

				Spacing();
				Separator();
				Spacing();

				if (show_json)
				{
					string j = store.Storage().dump(2);
					PushTextWrapPos(0.0f);
					TextUnformatted(j.data(), j.data() + j.size());
					PopTextWrapPos();
				}
				else
				{
					if (BeginTable("Data Values", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
					{
						TableSetupColumn("Name");
						TableSetupColumn("Type");
						TableSetupColumn("Value");
						TableSetupColumn("Actions");
						TableSetupScrollFreeze(0, 1);
						TableHeadersRow();

						TableNextRow();
						int index = 0;

						for (auto& [name, value] : store.Roots().items())
						{
							PushID(index);

							TableNextColumn();
							/// FieldNameEditor(db, field);
							Text("%s", name.c_str()); SameLine(); SmallButton("Edit");
							TableNextColumn();
							SetNextItemWidth(GetContentRegionAvail().x);
							/// FieldTypeEditor(db, field);
							TypeReference old_type{ mCurrentDatabase->Schema(), value.at("type") };
							GenericEditor<json*, TypeReference>("Type", *mCurrentDatabase, &value,
								/// validator
								[&](json* value, TypeReference const& new_type) -> result<void, string> {
									if (ResultOfConversion(old_type, new_type, value->at("value")) == ConversionResult::ConversionImpossible)
										return failure("conversion to this type is impossible");
									return ValidateType(new_type);
								},
								/// editor
								[&](json* value, TypeReference& current) {
									TypeChooser(mDatabase, current);
									auto result = ResultOfConversion(old_type, current, value->at("value"));
									switch (result)
									{
									case ConversionResult::DataCorrupted:
										TextColored({ 1,1,0,1 }, "WARNING: Data might be corrupted if you attempt this type change!");
										break;
									case ConversionResult::DataLost:
										TextColored({ 1,1,0,1 }, "WARNING: Data WILL BE LOST if you attempt this type change!");
										break;
									}
								},
								/// setter
								[&](json* value, TypeReference const& new_type) -> result<void, string> {
									TypeReference old_type{ mDatabase.Schema(), value->at("type") };
									value->at("type") = new_type.ToJSON();
									return Convert(old_type, new_type, value->at("value"));
								},
								/// getter
								[&](json* value) { return TypeReference{ mDatabase.Schema(), value->at("type") }; }
							);
							TableNextColumn();
							json::json_pointer ptr{ "/" + name };
							SetNextItemWidth(GetContentRegionAvail().x);
							EditValue(TypeReference{ mCurrentDatabase->Schema(), value.at("type") }, value.at("value"), {}, ptr, & store);
							TableNextColumn();

							DoDeleteValueUI(store, name);
							SameLine();
							SmallButton("Export Value to JSON");

							index++;
							PopID();
						}

						EndTable();
					}
				}

				EndTabItem();
			}
		}
		if (TabItemButton("+"))
		{
		}
		EndTabBar();
	}
}

void AttributesTab()
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

	Separator();
	if (Button("Create Backup"))
		CheckError(mCurrentDatabase->CreateBackup(), "Backup successfully created");
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
			if (ImGui::BeginTabItem("Data Stores"))
			{
				DataTab();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Attributes"))
			{
				AttributesTab();
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
		for (auto& modal : Modals)
		{
			ImGui::OpenPopup(modal->WindowName().c_str());
			if (ImGui::BeginPopupModal(modal->WindowName().c_str()))
			{
				modal->Do();
				ImGui::End();
			}
		}
		erase_if(Modals, [](auto const& modal) { return modal->Close; });

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
