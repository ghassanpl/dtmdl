#pragma once

#include "CppFormats.h"
#include "CSharpFormats.h"

namespace dtmdl
{

	struct JSONSchemaFormat : FormatPlugin
	{
		// Inherited via FormatPlugin
		virtual string FormatName() override;
		virtual string ExportFileName() override;
		virtual string Export(Database const&) override;
	};

	struct SqlSchemaFormat : FormatPlugin
	{
		/// - full schema
		/// - queries and stuff
		/// - maybe even add CppSQLProxyObjectsFormat with C++ proxy/bean handling code
	};
	
}