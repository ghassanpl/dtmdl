#include "pch.h"

#include "UICommon.h"

#include "Database.h"
#include "Values.h"
#include "Validation.h"

namespace dtmdl
{

	void DoDeleteValueUI(DataStore& store, string_view name)
	{
		ImGui::SmallButton(ICON_VS_TRASH "Delete Value");
		DoConfirmUI("Are you sure you want to delete this value?", [&store, name]() {
			LateExec.push_back([&store, name] { store.DeleteValue(name); });
			});
	}

	void DataTab()
	{
		using namespace ImGui;

		Button(ICON_VS_DATABASE "Add Data Store");
		SameLine();
		Button(ICON_VS_SAVE_ALL "Save All Data");

		if (BeginTabBar("Data Stores"))
		{
			for (auto& [name, store] : mCurrentDatabase->DataStores())
			{
				if (BeginTabItem(name.c_str()))
				{
					/// TODO: Tables!
					if (Button(ICON_VS_ADD "Add Value"))
					{
						auto name = FreshName("Value", [&](string_view name) { return store.HasValue(name); });
						//store.AddValue(json::object({ { "name", name }, { "type", TypeReference{ mCurrentDatabase->VoidType() }.ToJSON()}, {"value", json{}}}));
						store.AddValue(name, TypeReference{ mCurrentDatabase->VoidType() });
					}
					SameLine();
					if (Button(ICON_VS_SAVE_ALL "Save Data"))
					{

					}
					SameLine();
					if (Button(ICON_VS_DEBUG_RESTART "Revert Data"))
					{

					}
					SameLine();
					if (Button(ICON_VS_JSON "Import Value from JSON"))
					{

					}
					SameLine();
					if (Button(ICON_VS_MERGE "Merge Another Data Store"))
					{

					}
					SameLine();
					if (Button(ICON_VS_TRASH "Delete Data Store"))
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
						if (BeginTable("Data Values", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
						{
							TableSetupColumn("Name");
							TableSetupColumn("Type");
							TableSetupColumn("Value", {}, 100.0f);
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
								TextU(name); SameLine(); SmallButton(ICON_VS_EDIT "Edit");
								TableNextColumn();
								SetNextItemWidth(GetContentRegionAvail().x);
								/// FieldTypeEditor(db, field);
								TypeReference old_type = TypeFromJSON(mCurrentDatabase->Schema(), value.at("type"));
								GenericEditor<json*, TypeReference>("Type", &value,
									/// validator
									[&](json* value, TypeReference const& new_type) -> result<void, string> {
										if (ResultOfConversion(old_type, new_type, value->at("value")) == ConversionResult::ConversionImpossible)
											return failure("conversion to this type is impossible");
										return ValidateType(new_type);
									},
									/// editor
										[&](json* value, TypeReference& current) {
										TypeChooser(*mCurrentDatabase, current);
										auto result = ResultOfConversion(old_type, current, value->at("value"));
										switch (result)
										{
										case ConversionResult::DataCorrupted:
											TextColored({ 1,1,0,1 }, ICON_VS_WARNING "WARNING: Data might be corrupted if you attempt this type change!");
											break;
										case ConversionResult::DataLost:
											TextColored({ 1,1,0,1 }, ICON_VS_WARNING "WARNING: Data WILL BE LOST if you attempt this type change!");
											break;
										}
									},
										/// setter
										[&](json* value, TypeReference const& new_type) -> result<void, string> {
										TypeReference old_type = TypeFromJSON(mCurrentDatabase->Schema(), value->at("type"));
										value->at("type") = ToJSON(new_type);
										return Convert(old_type, new_type, value->at("value"));
									},
										/// getter
										[&](json* value) { return TypeFromJSON(mCurrentDatabase->Schema(), value->at("type")); }
									);
								TableNextColumn();
								json::json_pointer ptr{ "/" + name };
								SetNextItemWidth(GetContentRegionAvail().x);
								EditValue(TypeFromJSON(mCurrentDatabase->Schema(), value.at("type")), value.at("value"), {}, ptr, &store);
								TableNextColumn();

								DoDeleteValueUI(store, name);
								SameLine();
								SmallButton(ICON_VS_JSON "Export Value to JSON");

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

}