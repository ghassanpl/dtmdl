#pragma once

#include "CppFormatPlugin.h"

namespace dtmdl
{

	struct CppReflectionFormat : CppFormatPlugin
	{
		virtual string FormatName() override { return "C++ Reflection Header"; }
		virtual string ExportFileName() override { return "reflection.hpp"; }
		virtual string Export(Database const&) override;
	};

}