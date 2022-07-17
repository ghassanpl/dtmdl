#pragma once

#include "FormatPlugin.h"

namespace dtmdl
{

	struct CSharpFormatPlugin : FormatPlugin
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

	struct CSharpDeclarationFormat : CSharpFormatPlugin
	{
		virtual string FormatName() override { return "C# Declaration File"; }
		virtual string ExportFileName() override { return "Types.cs"; }
		virtual string Export(Database const&) override;

	private:

		void WriteClass(SimpleOutputter& out, Database const& db, ClassDefinition const* klass);
		void WriteEnum(SimpleOutputter& out, Database const& db, EnumDefinition const* enoom);
		void WriteStruct(SimpleOutputter& out, Database const& db, StructDefinition const* strukt);
	};

}