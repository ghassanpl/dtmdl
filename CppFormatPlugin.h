#pragma once

#include "FormatPlugin.h"

namespace dtmdl
{

	struct CppFormatPlugin : FormatPlugin
	{
	protected:

		static string FormatTypeName(Database const& db, TypeDefinition const* type);
		static string FormatTypeReference(Database const& db, TypeReference const& ref);
		static string FormatTemplateArgument(Database const& db, TemplateArgument const& arg);
		static string FormatNamespace(Database const& db);

		stringstream mOutString;
		SimpleOutputter StartOutput(Database const& db, vector<string_view> includes = {});
		string FinishOutput(Database const& db);
	};

	string ToCppTypeReference(TypeReference const& ref);
}