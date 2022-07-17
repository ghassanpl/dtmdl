#pragma once

#include "CppFormatPlugin.h"

namespace dtmdl
{

	struct CppTablesFormat : CppFormatPlugin
	{
		virtual string FormatName() override { return "C++ Tables Header"; }
		virtual string ExportFileName() override { return "tables.hpp"; }
		virtual string Export(Database const&) override;
	};

}