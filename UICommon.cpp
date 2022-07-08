#include "pch.h"

#include "UICommon.h"

vector<function<void()>> LateExec;

void Label(string_view s)
{
	ImGui::TextUnformatted(s.data(), s.data() + s.size());
}

TypeDefinition const* mOpenType = nullptr;

void TypeChooser(Database& db, TypeReference& ref, FilterFunc filter, const char* label)
{
	using namespace ImGui;

	PushID(&ref);
	auto current = ref.ToString();
	if (label == nullptr)
	{
		label = "##typechooser";
		SetNextItemWidth(GetContentRegionAvail().x);
	}

	int selected = 0;
	int i = 0;
	vector<string> names;
	vector<TypeDefinition const*> types;
	for (auto type : db.Definitions())
	{
		if (!filter || filter(type))
		{
			names.push_back(type->IconName());
			types.push_back(type);
			if (type == ref.Type)
				selected = i;
			++i;
		}
	}

	if (ComboWithFilter(label, &selected, names))
	{
		ref = TypeReference{ types[selected] };
	}

	/*
if (BeginCombo(label, current.c_str(), ImGuiComboFlags_HeightLargest))
{
	for (auto type : db.Definitions())
	{
		if (!filter || filter(type))
		{
			if (Selectable(type->IconName().c_str(), type == ref.Type))
			{
				ref = TypeReference{ type };
			}
		}
	}
	EndCombo();
}
*/

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
				TypeChooser(db, get<TypeReference>(ref.TemplateArguments[i]), [&](TypeDefinition const* type) { return ValidateTypeDefinition(type, param.Qualifier).has_value(); }, param.Name.c_str());
			}
			PopID();
			++i;
		}
		Unindent(8.0f);
	}

	PopID();
}
