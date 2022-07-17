#include "pch.h"
#include "Formats.h"
#include "Schema.h"
#include "Database.h"
#include "Validation.h"
//#include <sstream>
//#include <chrono>

namespace dtmdl
{

	string JSONSchemaFormat::FormatName()
	{
		return "JSON Schema";
	}

	string JSONSchemaFormat::ExportFileName()
	{
		return "schema.json";
	}

	string JSONSchemaFormat::Export(Database const& db)
	{
		json result = json::object();
		result["version"] = 1;
		result["namespace"] = db.Schema().Namespace;
		{
			auto& types = result["types"] = json::object();
			for (auto type : db.UserDefinitions())
			{
				types[type->Name()] = magic_enum::enum_name(type->Type());
			}
		}

		{
			auto& types = result["typedesc"] = json::object();
			for (auto type : db.UserDefinitions())
			{
				types[type->Name()] = type->ToJSON();
			}
		}

		return result.dump(2);
	}

	string MemberName(Database const& db, FieldDefinition const* def)
	{
		if (def->Flags.contain(FieldFlags::Private))
			return format("{}{}", db.PrivateFieldPrefix, def->Name);
		return def->Name;
	}

}