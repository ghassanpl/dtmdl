#pragma once

#include "CppFormatPlugin.h"

namespace dtmdl
{

	struct CppDatabaseFormat : CppFormatPlugin
	{
		virtual string FormatName() override { return "C++ Database Header"; }
		virtual string ExportFileName() override { return "database.hpp"; }
		virtual string Export(Database const&) override;
	};

}