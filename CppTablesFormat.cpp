#include "pch.h"
#include "CppTablesFormat.h"
#include "Schema.h"
#include "Database.h"
#include "Validation.h"

namespace dtmdl
{

	string CppTablesFormat::Export(Database const& db)
	{
		auto out = StartOutput(db, { "types.hpp" });

		for (auto def : db.Structs())
		{
			if (!def->Flags.contain(StructFlags::CreateTableType))
				continue;

			out.WriteStart("struct dtmdl_{0}Table : ::dtmdl::TableBase<dtmdl_{0}Table> {{", def->Name());
			out.WriteLine("using RowType = {};", FormatTypeName(db, def));
			out.WriteLine("template <::dtmdl::FixedString COLUMN>");
			out.WriteStart("static constexpr auto GetField() {{");
			for (auto& field : def->AllFieldsOrdered())
			{
				out.WriteLine("if constexpr (COLUMN.eq(\"{}\")) {{ return &{}::{}; }} else", field->Name, FormatTypeName(db, def), MemberName(db, field));
			}
			out.WriteLine("static_assert(::std::is_same_v<decltype(COLUMN), void>, \"column name not an (accessible) field in {}\");", FormatTypeName(db, def));
			out.WriteEnd("}}");

			out.Unindent();
			out.WriteLine("protected:");
			out.Indent();
			out.WriteLine("friend struct ::dtmdl::TableBase<dtmdl_{}Table>;", def->Name());
			out.WriteLine("::std::int64_t mLastRowID = 0;");
			out.WriteLine("::std::map<::std::int64_t, {}> mRows;", FormatTypeName(db, def));
			for (auto& field : def->AllFieldsOrdered())
			{
				if (field->Flags.contain(FieldFlags::Indexed))
				{
					if (field->Flags.contain(FieldFlags::Unique))
						out.WriteLine("::std::map<{}, ::std::int64_t, ::std::less<>> mOrderedBy{};", FormatTypeReference(db, field->FieldType), field->Name);
					else
						out.WriteLine("::std::multimap<{}, ::std::int64_t, ::std::less<>> mOrderedBy{};", FormatTypeReference(db, field->FieldType), field->Name);
				}
			}
			out.WriteEnd("}};");
		}

		return FinishOutput(db);
	}

}