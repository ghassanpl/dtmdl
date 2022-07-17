#include "pch.h"

#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include "UICommon.h"
#include <SDL2/SDL.h>
#undef main
#include <any>

#include "Database.h"
#include "Validation.h"
#include "Values.h"

#include "X:\Code\Native\ghassanpl\windows_message_box\windows_message_box.h"
#include "X:\Code\Native\ghassanpl\windows_message_box\windows_folder_browser.h"

using namespace dtmdl;

struct IModal
{
	virtual ~IModal() noexcept = default;

	virtual void Do() = 0;
	virtual string WindowName() const = 0;

	bool Close = false;
};

namespace ghassanpl
{
	void ReportAssumptionFailure(std::string_view expectation, std::initializer_list<std::pair<std::string_view, std::string>> values, std::string data, std::source_location loc)
	{
		msg::assumption_failure(expectation, values, data, loc);
	}
}

struct ErrorModal : IModal
{
	variant<string, function<void()>> Message;
	ErrorModal(variant<string, function<void()>> msg) : Message(move(msg)) {  }
	virtual void Do() override
	{
		if (auto str = get_if<string>(&Message))
			TextU(*str);
		else
			get<1>(Message)();
		ImGui::Spacing();
		if (ImGui::Button("OK"))
			Close = true;
	}
	virtual string WindowName() const override { return ICON_VS_ERROR "Error"; }
};

struct SuccessModal : IModal
{
	variant<string, function<void()>> Message;
	SuccessModal(variant<string, function<void()>> msg) : Message(move(msg)) {  }
	virtual void Do() override
	{
		if (auto str = get_if<string>(&Message))
			TextU(*str);
		else
			get<1>(Message)();
		ImGui::Spacing();
		if (ImGui::Button("OK"))
			Close = true;
	}
	virtual string WindowName() const override { return ICON_VS_PASS "Success"; }
};

vector<unique_ptr<IModal>> Modals;

template <typename T, typename... ARGS>
void OpenModal(ARGS&&... args)
{
	Modals.push_back(make_unique<T>(forward<ARGS>(args)...));
}

/// TODO: Move this away from depending on Database& db
bool TypeNameEditor(Database& db, TypeDefinition const* def)
{
	return GenericEditor<TypeDefinition const*, string>("Type Name", def,
		bind_front(&Database::ValidateTypeName, &db),
		[](TypeDefinition const* def, string& name) {
			using namespace ImGui;
			SetNextItemWidth(GetContentRegionAvail().x);
			return InputText("###typename", &name, ImGuiInputTextFlags_EnterReturnsTrue);
		},
		[&db, def](TypeDefinition const* def, string new_name) { return db.SetTypeName(def, move(new_name)); },
		[](TypeDefinition const* def) -> auto const& { return def->Name(); }
	);
}

bool EnumeratorNameEditor(Database& db, EnumeratorDefinition const* def)
{
	return GenericEditor<EnumeratorDefinition const*, string>("Enumerator Name", def,
		bind_front(&Database::ValidateEnumeratorName, &db),
		[](EnumeratorDefinition const* def, string& name) {
			using namespace ImGui;
			SetNextItemWidth(GetContentRegionAvail().x);
			return InputText("###enumeratorname", &name, ImGuiInputTextFlags_EnterReturnsTrue);
		},
		bind_front(&Database::SetEnumeratorName, &db),
		[](EnumeratorDefinition const* def) -> auto const& { return def->Name; }
	);
}

bool ToolButton(const char* icon, const char* name)
{
	auto result = ImGui::SmallButton(icon);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", name);
	return result;
}

bool EnumeratorValueEditor(Database& db, EnumeratorDefinition const* def)
{
	bool changed = false;
	using namespace ImGui;

	PushID("Enumerator Value");
	PushID(def);

	static map<EnumeratorDefinition const*, int64_t> is_editing;

	if (auto it = is_editing.find(def); it != is_editing.end())
	{
		using namespace ImGui;
		SetNextItemWidth(GetContentRegionAvail().x);
		InputScalar("###enumeratorvalue", ImGuiDataType_S64, &it->second);

		if (SmallButton(ICON_VS_EDIT "Apply"))
		{
			CheckError(db.SetEnumeratorValue(def, it->second));
			is_editing.erase(def);
			changed = true;
		}
		SameLine();
		if (SmallButton(/*ICON_VS_REFRESH*/ ICON_VS_DISCARD "Reset"))
		{
			CheckError(db.SetEnumeratorValue(def, nullopt));
			is_editing.erase(def);
			changed = true;
		}
		SameLine();
		if (SmallButton("Cancel"))
			is_editing.erase(def);
	}
	else
	{
		auto actual = def->ActualValue();
		if (!def->Value.has_value())
			ImGui::TextDisabled("%lli", actual);
		else
			ImGui::Text("%lli", actual);
		SameLine();
		if (SmallButton(ICON_VS_EDIT "Edit"))
			is_editing[def] = actual;
		if (def->Value.has_value())
		{
			SameLine();
			if (SmallButton(/*ICON_VS_REFRESH*/ ICON_VS_DISCARD "Reset"))
			{
				CheckError(db.SetEnumeratorValue(def, nullopt));
				changed = true;
			}
		}
	}

	PopID();
	PopID();
	return changed;
}

bool EnumeratorDescriptiveNameEditor(Database& db, EnumeratorDefinition const* def)
{
	return GenericEditor<EnumeratorDefinition const*, string>("Enumerator Descriptive Name", def,
		[](EnumeratorDefinition const* def, string const& value) -> result<void, string> { return success(); },
		[](EnumeratorDefinition const* def, string& name) {
			using namespace ImGui;
			SetNextItemWidth(GetContentRegionAvail().x);
			return InputTextWithHint("###enumeratordescname", def->Name.c_str(), &name, ImGuiInputTextFlags_EnterReturnsTrue);
		},
		bind_front(&Database::SetEnumeratorDescriptiveName, &db),
		[](EnumeratorDefinition const* def) -> auto const& { return def->DescriptiveName; },
		[](EnumeratorDefinition const* def) { 
			if (def->DescriptiveName.empty())
				TextUD(def->Name);
			else
				TextU(def->DescriptiveName);
		}
	);
}

bool RecordBaseTypeEditor(Database& db, RecordDefinition const* def)
{
	return GenericEditor<RecordDefinition const*, TypeReference>("Base Type", def,
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
						if (Selectable(type->IconName().c_str(), type == current.Type))
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
	return GenericEditor<FieldDefinition const*, string>("Field Name", def,
		bind_front(&Database::ValidateFieldName, &db),
		[](FieldDefinition const* def, string& name) {
			using namespace ImGui;
			SetNextItemWidth(GetContentRegionAvail().x);
			return InputText("###fieldname", &name, ImGuiInputTextFlags_EnterReturnsTrue);
		},
		bind_front(&Database::SetFieldName, &db),
		[](FieldDefinition const* def) -> auto const& { return def->Name; }
	);
}

bool FieldTypeEditor(Database& db, FieldDefinition const* def)
{
	return GenericEditor<FieldDefinition const*, TypeReference>("Field Type", def,
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
		TextU("NOTE: Removing this field will also remove all data stored in this field!");
		break;
	case 1:
	{
		auto changed = Change(db, usage);
		TextF("Change: var {0} : {1}; -> var {0} : {2};", usage.Field->Name, usage.Field->FieldType.ToString(), changed.ToString());
		Spacing();
	}
	if (usage.References.size() == 1 && usage.References[0].empty())
		TextU("NOTE: Changing the field type to 'void' will also remove all data stored in this field!");
	else
		TextU("NOTE: Changing the field type will trigger a data update which may destroy or corrupt data held in this field!");
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
	TextU("NOTE: All data of this type will be deleted");
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

		TextF("You are trying to delete the field '{}.{}' which is in use in {} data stores. Please decide how to handle each store:", mField->ParentRecord->Name(), mField->Name, mStores.size());

		size_t i = 0;
		for (auto& store : mStores)
		{
			BulletText("Storage '{}': ", store.c_str());
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

struct DeleteEnumeratorModal : IModal
{
	Database& mDB;
	EnumeratorDefinition const* mEnumerator;
	vector<string> mStores;
	vector<char> mSettings;
	bool mMakeBackup = true;

	DeleteEnumeratorModal(Database& db, EnumeratorDefinition const* def, vector<string> stores_with_data)
		: mDB(db)
		, mEnumerator(def)
		, mStores(move(stores_with_data))
		, mSettings(mStores.size(), false)
	{

	}

	virtual void Do() override
	{
		using namespace ImGui;

		TextF("You are trying to delete the enumerator '{}.{}' which is in use in {} data stores. Please decide how to handle each store:", mEnumerator->ParentEnum->Name(), mEnumerator->Name, mStores.size());
		size_t i = 0;
		for (auto& store : mStores)
		{
			BulletText("Storage '{}': ", store.c_str());
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
		return "Deleting Enumerator";
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

		TextF("You are trying to delete the type '{}' which is in use in {} places. Please decide how to handle each use:", mType->Name(), mUsages.size());

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

void DoDeleteTypeUI(Database& db, TypeDefinition const* def)
{
	ImGui::Button(ICON_VS_TRASH "Delete Type");
	DoConfirmUI("Are you sure you want to delete this type?", [&db, def]() {
		auto usages = db.LocateTypeUsages(def);
		if (usages.empty())
			LateExec.push_back([&db, def] { CheckError(db.DeleteType(def)); });
		else
			OpenModal<DeleteTypeModal>(db, def, move(usages));
		}
	);
}

template <typename T, typename E>
void FlagEditor(T const* val, enum_flags<E>& flags)
{
	for (auto& [value, name] : magic_enum::enum_entries<E>())
	{
		if (!IsFlagAvailable(val, value))
			continue;

		string fuck = string{ name };
		bool val = flags.is_set(value);
		if (ImGui::Checkbox(fuck.c_str(), &val))
			flags.set_to(val, value);
	}
}

bool FieldFlagsEditor(Database& db, FieldDefinition const* field)
{
	return GenericEditor<FieldDefinition const*, enum_flags<FieldFlags>>("Field Flags", field,
		bind_front(&Database::ValidateFieldFlags, &db),
		[](FieldDefinition const* val, enum_flags<FieldFlags>& flags) { FlagEditor(val, flags); },
		bind_front(&Database::SetFieldFlags, &db),
		[](FieldDefinition const* def) -> auto const& { return def->Flags; }
	);
}

bool ClassFlagsEditor(Database& db, ClassDefinition const* klass)
{
	return GenericEditor<ClassDefinition const*, enum_flags<ClassFlags>>("Class Flags", klass,
		bind_front(&Database::ValidateClassFlags, &db),
		[](ClassDefinition const* val, enum_flags<ClassFlags>& flags) { FlagEditor(val, flags); },
		bind_front(&Database::SetClassFlags, &db),
		[](ClassDefinition const* def) -> auto const& { return def->Flags; }
	);
}

bool StructFlagsEditor(Database& db, StructDefinition const* klass)
{
	return GenericEditor<StructDefinition const*, enum_flags<StructFlags>>("Struct Flags", klass,
		bind_front(&Database::ValidateStructFlags, &db),
		[](StructDefinition const* val, enum_flags<StructFlags>& flags) { FlagEditor(val, flags); },
		bind_front(&Database::SetStructFlags, &db),
		[](StructDefinition const* def) -> auto const& { return def->Flags; }
	);
}

void EditRecord(Database& db, RecordDefinition const* def, bool is_struct)
{
	using namespace ImGui;
	TextU("Name: "); SameLine(); TypeNameEditor(db, def);
	TextU("Base Type: "); SameLine(); RecordBaseTypeEditor(db, def);
	
	TextU("Flags: ");
	SameLine();
	if (def->IsClass())
		ClassFlagsEditor(db, (ClassDefinition const*)def);
	else if (def->IsStruct())
		StructFlagsEditor(db, (StructDefinition const*)def);

	if (Button(ICON_VS_SYMBOL_FIELD "Add Field"))
	{
		ignore = db.AddNewField(def);
	}
	SameLine();

	DoDeleteTypeUI(db, def);

	if (BeginTable("Fields", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
	{
		TableSetupColumn("Name");
		TableSetupColumn("Type");
		TableSetupColumn("Attributes");
		TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
		TableSetupScrollFreeze(0, 1);
		TableHeadersRow();

		TableNextRow();
		int own_field_index = 0;
		size_t index = 0;
		for (auto& field : def->AllFieldsOrdered())
		{
			PushID(index);

			if (field->ParentRecord != def)
			{
				TableNextColumn();
				TextDisabledF("{}.{}", field->ParentRecord->Name(), field->Name);
				TableNextColumn();
				TextDisabledF("{}", field->FieldType.ToString());
				TableNextColumn();
				TextF("{}", field->Flags);
				//Label("Attributes");
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
				FieldFlagsEditor(db, field);
				TableNextColumn();

				BeginDisabled(own_field_index == 0);
				if (ToolButton(ICON_VS_TRIANGLE_UP, "Up"))
					LateExec.push_back([&db, def, own_field_index] { CheckError(db.SwapFields(def, own_field_index, size_t(own_field_index - 1))); });
				EndDisabled();
				SameLine();

				BeginDisabled(own_field_index == def->Fields().size() - 1);
				if (ToolButton(ICON_VS_TRIANGLE_DOWN, "Down"))
					LateExec.push_back([&db, def, own_field_index] { CheckError(db.SwapFields(def, own_field_index, size_t(own_field_index + 1))); });
				EndDisabled();
				SameLine();

				ToolButton(ICON_VS_EXPAND_ALL, "Duplicate"); SameLine();
				ToolButton(ICON_VS_COPY, "Copy"); SameLine();

				ToolButton(ICON_VS_TRASH, "Delete");
				DoConfirmUI("Are you sure you want to delete this field?", [&db, field]() {
					auto usages = db.StoresWithFieldData(field);
					if (usages.empty())
						LateExec.push_back([&db, field] { CheckError(db.DeleteField(field)); });
					else
						OpenModal<DeleteFieldModal>(db, field, move(usages));
				});

				own_field_index++; /// only count own fields
			}

			index++;
			PopID();
		}

		EndTable();
	}
}

void EditEnum(Database& db, EnumDefinition const* enoom)
{
	using namespace ImGui;
	TextU("Name: "); SameLine(); TypeNameEditor(db, enoom);

	/// TODO: Edit base type: [none] or integer types

	if (Button(ICON_VS_SYMBOL_ENUM_MEMBER "Add Enumerator"))
	{
		ignore = db.AddNewEnumerator(enoom);
	}
	SameLine();

	Button("Sort By Value");
	SameLine();

	DoDeleteTypeUI(db, enoom);

	/// TODO: PropertyEditor<bool>(enoom, "backcomp", "Try To Ensure Backwards Compatibility");

	if (BeginTable("Enumerators", 5, ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable| ImGuiTableFlags_SizingStretchProp))
	{
		TableSetupColumn("Name");
		TableSetupColumn("Value");
		TableSetupColumn("Descriptive Name");
		TableSetupColumn("Attributes");
		TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
		TableSetupScrollFreeze(0, 1);
		TableHeadersRow();

		TableNextRow();
		size_t index = 0;
		for (auto enumerator : enoom->Enumerators())
		{
			PushID(index);

			TableNextColumn();
			SetNextItemWidth(GetContentRegionAvail().x);
			EnumeratorNameEditor(db, enumerator);
			TableNextColumn();
			SetNextItemWidth(GetContentRegionAvail().x);
			EnumeratorValueEditor(db, enumerator);
			TableNextColumn();
			SetNextItemWidth(GetContentRegionAvail().x);
			EnumeratorDescriptiveNameEditor(db, enumerator);
			TableNextColumn();
			TextU("Attributes");
			TableNextColumn();

			BeginDisabled(index == 0);
			if (ToolButton(ICON_VS_TRIANGLE_UP, "Up"))
				LateExec.push_back([&db, enoom, index] { CheckError(db.SwapEnumerators(enoom, index, index - 1)); });
			EndDisabled();
			SameLine();

			BeginDisabled(index == enoom->Enumerators().size() - 1);
			if (ToolButton(ICON_VS_TRIANGLE_DOWN, "Down"))
				LateExec.push_back([&db, enoom, index] { CheckError(db.SwapEnumerators(enoom, index, index + 1)); });
			EndDisabled();
			SameLine();

			ToolButton(ICON_VS_EXPAND_ALL, "Duplicate"); SameLine();
			ToolButton(ICON_VS_COPY, "Copy"); SameLine();
			ToolButton(ICON_VS_TRASH, "Delete");
			DoConfirmUI("Are you sure you want to delete this enumerator?", [&db, enumerator]() {
				auto usages = db.StoresWithEnumeratorData(enumerator);
				if (usages.empty())
					LateExec.push_back([&db, enumerator] { CheckError(db.DeleteEnumerator(enumerator)); });
				else
					OpenModal<DeleteEnumeratorModal>(db, enumerator, move(usages));
			});

			PopID();
			index++;
		}

		EndTable();
	}
}

unique_ptr<Database> mCurrentDatabase = nullptr;

string Multiples(string_view objs)
{
	if (objs.ends_with("s"))
		return format("{}es", objs);
	return format("{}s", objs);
}

void TypesTab()
{
	using namespace ImGui;
	Spacing();
	if (Button(ICON_VS_SYMBOL_STRUCTURE "Add Struct"))
		mSelectedType = mCurrentDatabase->AddNewStruct().value();
	SameLine();
	if (Button(ICON_VS_SYMBOL_CLASS "Add Class"))
		mSelectedType = mCurrentDatabase->AddNewClass().value();
	SameLine();
	if (Button(ICON_VS_SYMBOL_ENUM "Add Enum"))
		mSelectedType = mCurrentDatabase->AddNewEnum().value();
	SameLine();
	Button(ICON_VS_SYMBOL_MISC "Add Union"); SameLine();
	Button(ICON_VS_ARROW_SMALL_RIGHT "Add Alias"); SameLine();

	SeparatorEx(ImGuiSeparatorFlags_Vertical); SameLine();
	
	TextU("Sort by: "); SameLine();

	static int sort = 0;
	RadioButton("Name", &sort, 0); SameLine();
	RadioButton("Type", &sort, 1); SameLine();

	SeparatorEx(ImGuiSeparatorFlags_Vertical); SameLine();

	TextU("Show: "); SameLine();

	static unsigned filter_types = -1;
	using enum DefinitionType;
	for (auto type : { Enum, Struct, Class, Union, Alias })
	{
		//CheckboxFlags(IconsForDefinitionType[(int)type], &filter_types, (1u << unsigned(type))); SameLine();
		auto label = format("{}{}", IconsForDefinitionType[(int)type], Multiples(magic_enum::enum_name(type)));
		ToggleButtonFlags(label.c_str(), filter_types, (1u << unsigned(type))); SameLine();
	}
	NewLine();

	Spacing();
	Separator();
	Spacing();


	/*
	float h = GetContentRegionAvail().y;
	static float sz1 = 0.2f;
	static float sz2 = 0.8f;
	if (sz1 == -1)
	{
		sz1 = GetWindowWidth() * 0.3f;
		sz2 = GetWindowWidth() * 0.7f;
	}
	Splitter(true, 8.0f, &sz1, &sz2, 0.2f, 0.2f, h);
	*/

	bool selected_in_list = false;
	for (auto def : mCurrentDatabase->UserDefinitions())
	{
		if (!(filter_types & (1 << int(def->Type()))))
			continue;
		if (def == mSelectedType)
			selected_in_list = true;
	}
	if (mSelectedType && !selected_in_list)
		mSelectedType = nullptr;

	if (BeginChild("Types", { 300.0f, -1.0f }))
	{
		if (BeginListBox("##", { -1.0f, -1.0f }))
		{
			for (auto def : mCurrentDatabase->UserDefinitions())
			{
				AssumingNotNull(def);
				if (!(filter_types & (1 << int(def->Type()))))
					continue;

				bool selected = def == mSelectedType;
				if (Selectable(def->IconNameWithParent().c_str(), selected))
					mSelectedType = def;
			}

			EndListBox();
		}
	}
	EndChild();

	SameLine();

	if (BeginChild("Type Properties", {-1.0f, -1.0f}))
	{
		if (mSelectedType)
		{
			if (auto strukt = dynamic_cast<StructDefinition const*>(mSelectedType))
				EditRecord(*mCurrentDatabase, strukt, true);
			else if (auto klass = dynamic_cast<ClassDefinition const*>(mSelectedType))
				EditRecord(*mCurrentDatabase, klass, false);
			else if (auto eenoom = dynamic_cast<EnumDefinition const*>(mSelectedType))
				EditEnum(*mCurrentDatabase, eenoom);
		}
		else
		{
			TextU("Select a type on the left");
		}
	}
	EndChild();

}

namespace dtmdl
{

	void DataTab();

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

	/// TODO: validation - identifier, cannot be "std" or "dtmdl"
	//InputText("Namespace", &mCurrentDatabase->Schema().Namespace);
}

int main(int argc, char** argv)
{
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
	{
		printf("Error: %s\n", SDL_GetError());
		return -1;
	}

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window* window = SDL_CreateWindow("Dear ImGui SDL2+SDL_Renderer example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, window_flags);

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


	ImFont* font = io.Fonts->AddFontFromFileTTF("Roboto-Regular.ttf", 22.0f);

	static const ImWchar icons_ranges[] = { ICON_MIN_VS, ICON_MAX_VS, 0 };
	ImFontConfig config;
	config.MergeMode = true;
	config.GlyphOffset.x = -2.0f;
	config.GlyphOffset.y = 5.0f;
	config.GlyphMinAdvanceX = 16.0f;
	io.Fonts->AddFontFromFileTTF(FONT_ICON_FILE_NAME_VS, 22.0f, &config, icons_ranges);
	io.Fonts->Build();

	if (argc > 1)
	{
		try
		{
			mCurrentDatabase = make_unique<Database>(argv[1]);
		}
		catch (...)
		{

		}
	}

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

		if (ImGui::Button(ICON_VS_FOLDER_OPENED "Open Database"))
		{
			try
			{
				auto path = ghassanpl::windows_browse_for_folder({ .title = "Choose Database Directory" });
				if (!path.empty())
				{
					mCurrentDatabase = make_unique<Database>(path);
				}
			}
			catch (...)
			{
				ghassanpl::windows_message_box("Error", "Could not open database", ghassanpl::msg::ok_button, ghassanpl::windows_message_box_icon::Error);
			}
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(mCurrentDatabase == nullptr);
		if (ImGui::Button(ICON_VS_CLOSE_ALL "Close Database"))
		{
			mCurrentDatabase->SaveAll();
			mCurrentDatabase = nullptr;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button(ICON_VS_SAVE_ALL "Save All"))
		{
			mCurrentDatabase->SaveAll();
		}
		ImGui::SameLine();
		if (ImGui::Button(ICON_VS_FILE_ZIP "Create Backup"))
			CheckError(mCurrentDatabase->CreateBackup(), "Backup successfully created");
		ImGui::SameLine();

		ImGui::NewLine();
		ImGui::Separator();

		if (mCurrentDatabase)
		{
			if (ImGui::BeginTabBar("Main Tabs"))
			{
				if (ImGui::BeginTabItem(ICON_VS_TYPE_HIERARCHY_SUB "Types"))
				{
					TypesTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(ICON_VS_DATABASE "Data Stores"))
				{
					DataTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(ICON_VS_SYMBOL_PARAMETER "Attributes"))
				{
					AttributesTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(ICON_VS_SYMBOL_INTERFACE "Interfaces"))
				{
					InterfacesTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(/*ICON_VS_LAYOUT*/ICON_VS_PREVIEW "Displays"))
				{
					DisplaysTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(ICON_VS_CODE "Scripting"))
				{
					ScriptingTab();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(ICON_VS_SETTINGS "Properties")) /// ICON_VS_SETTINGS_GEAR / ICON_VS_TOOLS
				{
					PropertiesTab();
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
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

	if (mCurrentDatabase)
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
