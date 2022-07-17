#pragma once

#include "CppFormatPlugin.h"

namespace dtmdl
{

	struct CppDeclarationFormat : CppFormatPlugin
	{
		// Inherited via FormatPlugin
		virtual string FormatName() override;
		virtual string ExportFileName() override;
		virtual string Export(Database const&) override;

	private:

		set<TypeDefinition const*> mWrittenTypes;

		void WriteClass(SimpleOutputter& out, Database const& db, ClassDefinition const* klass);
		void WriteEnum(SimpleOutputter& out, Database const& db, EnumDefinition const* enoom);
		void WriteStruct(SimpleOutputter& out, Database const& db, StructDefinition const* strukt);

		void AdditionalMembers(SimpleOutputter& out, Database const& db, TypeDefinition const* type);
	};

}