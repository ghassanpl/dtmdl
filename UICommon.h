#pragma once

#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"
#include <imgui_stdlib.h>
#include "ImGuiHelpers.h"

#include "Database.h"
#include "Validation.h"
#include "Values.h"

void Label(string_view s);

template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using ValidateFunc = function<result<void, string>(EDITING_OBJECT, OBJECT_PROPERTY const&)>;

template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using EditorFunc = function<void(EDITING_OBJECT, OBJECT_PROPERTY&)>;

template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using ApplyFunc = function<result<void, string>(EDITING_OBJECT, OBJECT_PROPERTY const&)>;

template <typename EDITING_OBJECT, typename OBJECT_PROPERTY>
using GetterFunc = function<OBJECT_PROPERTY(EDITING_OBJECT)>;

template <typename EDITING_OBJECT>
using DisplayFunc = function<void(EDITING_OBJECT)>;

inline void Display(string const& val) { Label(val); }
inline void Display(TypeReference const& val) { Label(val.ToString()); }

template <typename E, typename P>
bool GenericEditor(const char* id, Database& db, E def,
	/*ValidateFunc<E, P>*/ auto&& validate,
	/*EditorFunc<E, P>*/ auto&& editor,
	/*ApplyFunc<E, P>*/ auto&& apply,
	/*GetterFunc<E, P>*/ auto&& getter,
	DisplayFunc<E> display = {}) /// TODO: reset ?
{
	using namespace ImGui;

	bool changed = false;
	using namespace ImGui;
	static map<string, map<E, P>, less<>> is_editing_map;
	static E open_next{};
	auto& is_editing = is_editing_map[id];
	PushID(id);
	PushID(def);

	/*
	if (auto it = is_editing.find(def); it == is_editing.end())
	{
		decltype(auto) val = getter(def);
		if (validate(def, val).has_error())
			is_editing[def] = val;
	}
	*/

	if (auto it = is_editing.find(def); it != is_editing.end())
	{
		bool do_apply = false;
		if (open_next == def)
		{
			SetKeyboardFocusHere();
			open_next = {};
		}
		if constexpr (convertible_to<invoke_result_t<remove_cvref_t<decltype(editor)>, decltype(def), decltype(it->second)&>, bool>)
		{
			do_apply = editor(def, it->second);
		}
		else
		{
			editor(def, it->second);
		}

		if (auto result = validate(def, it->second); result.has_error())
		{
			TextColored({ 1,0,0,1 }, ICON_VS_ERROR "%s", result.error().c_str());
			BeginDisabled();
			SmallButton(ICON_VS_CHECK "Apply");
			EndDisabled();
		}
		else
		{
			if (SmallButton(ICON_VS_CHECK "Apply") || do_apply)
			{
				auto result = apply(def, it->second);
				if (result.has_error())
				{
					CheckError(result);
				}
				else
				{
					is_editing.erase(def);
					changed = true;
				}
			}
		}
		SameLine();
		if (SmallButton(ICON_VS_CLOSE "Cancel"))
			is_editing.erase(def);
	}
	else
	{
		if (display)
			display(def);
		else
		{
			decltype(auto) val = getter(def);
			Display(val);
		}
		SameLine();
		if (SmallButton(ICON_VS_EDIT "Edit"))
		{
			is_editing[def] = getter(def);
			open_next = def;
		}
	}

	PopID();
	PopID();
	return changed;
}

template <typename FUNC>
void DoConfirmUI(string confirm_text, FUNC&& func)
{
	using namespace ImGui;
	if (BeginPopupContextItem(confirm_text.c_str(), 0))
	{
		Label(confirm_text);
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

extern vector<function<void()>> LateExec;

using FilterFunc = std::function<bool(TypeDefinition const*)>;

extern TypeDefinition const* mOpenType;

void TypeChooser(Database& db, TypeReference& ref, FilterFunc filter = {}, const char* label = nullptr);