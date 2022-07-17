#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include "ImGuiHelpers.h"

namespace dtmdl
{
	struct TypeDefinition;
	struct TypeReference;
	struct Database;

	void TextU(string_view s);
	void TextUD(string_view s);

	template <typename... ARGS>
	void TextF(string_view fmt, ARGS&&... args)
	{
		TextU(vformat(fmt, make_format_args(forward<ARGS>(args)...)));
	}

	template <typename... ARGS>
	void TextDisabledF(string_view fmt, ARGS&&... args)
	{
		TextUD(vformat(fmt, make_format_args(forward<ARGS>(args)...)));
	}

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

	void Display(string const& val);
	void Display(TypeReference const& val);
	template <typename T>
	void Display(enum_flags<T> val)
	{
		Display(format("{}", val));
	}

	template <typename E, typename P>
	bool GenericEditor(const char* id, E def,
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

	template <typename T>
	bool FlagEditor(enum_flags<T>& flags, string_view name)
	{
		return GenericEditor<enum_flags<T>*, enum_flags<T>>(name, &flags,
			[](enum_flags<T>* def, enum_flags<T> const& value) -> result<void, string> { return success(); },
			[](enum_flags<T>*, enum_flags<T>& name) {

			},
			[](enum_flags<T>* def, enum_flags<T> const& value) -> result<void, string> { *def = value; },
				[](enum_flags<T>* def) -> auto const& { return *def; }
			);
	}

	template <typename FUNC>
	void DoConfirmUI(string confirm_text, FUNC&& func)
	{
		using namespace ImGui;
		if (BeginPopupContextItem(confirm_text.c_str(), 0))
		{
			TextU(confirm_text);
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

	extern TypeDefinition const* mSelectedType;

	void TypeChooser(Database& db, TypeReference& ref, FilterFunc filter = {}, const char* label = nullptr);
}