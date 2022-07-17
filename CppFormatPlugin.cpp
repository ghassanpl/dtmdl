#include "pch.h"
#include "CppFormats.h"
#include "Schema.h"
#include "Database.h"
#include "Validation.h"

namespace dtmdl
{

	string CppFormatPlugin::FormatTypeName(Database const& db, TypeDefinition const* type)
	{
		if (!type)
			return {};

		if (!type->IsBuiltIn())
			return format("{}::{}", FormatNamespace(db), type->Name());

		static map<string, string, less<>> cpp_builtin_type_names = {
			{"void", "::dtmdl::NativeTypes::Void"},
			{"f32", "float"},
			{"f64", "double"},
			{"i8", "int8_t"},
			{"i16", "int16_t"},
			{"i32", "int32_t"},
			{"i64", "int64_t"},
			{"u8", "uint8_t"},
			{"u16", "uint16_t"},
			{"u32", "uint32_t"},
			{"u64", "uint64_t"},
			{"bool", "bool"},
			{"string", "::dtmdl::NativeTypes::String"},
			{"bytes", "::dtmdl::NativeTypes::Bytes"},
			{"flags", "::dtmdl::NativeTypes::Flags"},
			{"list", "::dtmdl::NativeTypes::List"},
			{"array", "::dtmdl::NativeTypes::Array"},
			{"ref", "::dtmdl::NativeTypes::Ref"},
			{"own", "::dtmdl::NativeTypes::Own"},
			{"variant", "::dtmdl::NativeTypes::Variant"},
			{"map", "::dtmdl::NativeTypes::Map"},
			{"json", "::dtmdl::NativeTypes::JSON"},

			{"vec2", "::dtmdl::NativeTypes::vec<2, float>"},
			{"vec3", "::dtmdl::NativeTypes::vec<3, float>"},
			{"vec4", "::dtmdl::NativeTypes::vec<4, float>"},

			{"dvec2", "::dtmdl::NativeTypes::vec<2, double>"},
			{"dvec3", "::dtmdl::NativeTypes::vec<3, double>"},
			{"dvec4", "::dtmdl::NativeTypes::vec<4, double>"},

			{"ivec2", "::dtmdl::NativeTypes::vec<2, int>"},
			{"ivec3", "::dtmdl::NativeTypes::vec<3, int>"},
			{"ivec4", "::dtmdl::NativeTypes::vec<4, int>"},

			{"uvec2", "::dtmdl::NativeTypes::vec<2, unsigned>"},
			{"uvec3", "::dtmdl::NativeTypes::vec<3, unsigned>"},
			{"uvec4", "::dtmdl::NativeTypes::vec<4, unsigned>"},

			{"bvec2", "::dtmdl::NativeTypes::vec<2, bool>"},
			{"bvec3", "::dtmdl::NativeTypes::vec<3, bool>"},
			{"bvec4", "::dtmdl::NativeTypes::vec<4, bool>"},
		};

		auto it = cpp_builtin_type_names.find(type->Name());
		if (it == cpp_builtin_type_names.end())
			throw std::runtime_error(format("unrecognized builtin type: {}", type->Name()));

		return it->second;
	}

	string CppFormatPlugin::FormatTypeReference(Database const& db, TypeReference const& ref)
	{
		if (ref.TemplateArguments.size())
			return format("{}<{}>", FormatTypeName(db, ref.Type), string_ops::join(ref.TemplateArguments, ", ", bind_front(&CppDeclarationFormat::FormatTemplateArgument, std::ref(db))));
		return FormatTypeName(db, ref.Type);
	}

	string CppFormatPlugin::FormatTemplateArgument(Database const& db, TemplateArgument const& arg)
	{
		if (auto it = get_if<uint64_t>(&arg))
			return to_string(*it);
		return FormatTypeReference(db, get<TypeReference>(arg));
	}

	string CppFormatPlugin::FormatNamespace(Database const& db)
	{
		return db.Schema().Namespace; /// TODO: This
	}

	SimpleOutputter CppFormatPlugin::StartOutput(Database const& db, vector<string_view> includes)
	{
		mOutString.clear();
		SimpleOutputter out{ mOutString };
		out.WriteLine("/// source_database: \"{}\"", string_ops::escaped(filesystem::absolute(db.Directory()).string(), "\"\\"));
		out.WriteLine("/// generated_time: \"{}\"", chrono::zoned_time{ chrono::current_zone(), chrono::system_clock::now() });

		out.WriteLine("#pragma once");

		for (auto include : includes)
			out.WriteLine("#include \"{}\"", include);

		out.WriteStart("namespace {} {{", db.Schema().Namespace);

		return out;
	}

	string CppFormatPlugin::FinishOutput(Database const& db)
	{
		SimpleOutputter out{ mOutString };

		out.WriteLine("}}");

		return move(mOutString).str();
	}

}