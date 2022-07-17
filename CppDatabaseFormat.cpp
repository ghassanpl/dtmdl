#include "pch.h"
#include "CppDatabaseFormat.h"
#include "Schema.h"
#include "Database.h"
#include "Validation.h"

namespace dtmdl
{

	string CppDatabaseFormat::Export(Database const& db)
	{
		auto out = StartOutput(db, { "types.hpp", "reflection.hpp" });

		out.WriteStart("struct dtmdl_database : public ::dtmdl::GCHeap<dtmdl_reflection> {{");
		out.WriteLine("template <::std::derived_from<::dtmdl::BaseClass> T>");
		out.WriteStart("::dtmdl::NativeTypes::Own<T> New() {{");
		out.WriteLine("return this->Add(::std::unique_ptr<T>(dtmdl_reflection::New(::std::type_identity<T>{{}})));");
		out.WriteEnd("}}");
		out.WriteEnd("}};");

		return FinishOutput(db);
	}

}