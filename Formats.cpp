#include "pch.h"
#include "Formats.h"
#include "Schema.h"
#include "Database.h"
#include <sstream>
#include <chrono>

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
	{
		auto& types = result["types"] = json::object();
		for (auto type : db.Definitions())
		{
			if (!type->IsBuiltIn())
				types[type->Name()] = magic_enum::enum_name(type->Type());
		}
	}

	{
		auto& types = result["typedesc"] = json::object();
		for (auto type : db.Definitions())
		{
			if (!type->IsBuiltIn())
				types[type->Name()] = type->ToJSON();
		}
	}

	return result.dump(2);
}

struct SimpleOutputter
{
	SimpleOutputter(ostream& out) : OutStream(out) {}

	ostream& OutStream;
	size_t Indentation = 0;
	bool NewLine = true;

	void WriteIndent() { if (NewLine) { for (size_t i = 0; i < Indentation; ++i) OutStream << "\t"; NewLine = false; } }
	template <typename... ARGS>
	void Write(string_view fmt, ARGS&&... args) { WriteIndent(); OutStream << vformat(fmt, make_format_args(forward<ARGS>(args)...)); }
	template <typename... ARGS>
	void WriteLine(string_view fmt, ARGS&&... args) { Write(fmt, forward<ARGS>(args)...); OutStream << "\n"; NewLine = true; }
	template <typename... ARGS>
	void WriteStart(string_view fmt, ARGS&&... args) { WriteLine(fmt, forward<ARGS>(args)...); Indent(); }
	template <typename... ARGS>
	void WriteEnd(string_view fmt, ARGS&&... args) { Unindent(); WriteLine(fmt, forward<ARGS>(args)...); }

	void Indent() { Indentation++; }
	void Unindent() { Indentation--; }
};

string CppDeclarationFormat::FormatName()
{
	return "C++ Headers";
}

string CppDeclarationFormat::ExportFileName()
{
	return "header.hpp";
}

void CppDeclarationFormat::WriteClass(SimpleOutputter& out, Database const& db, ClassDefinition const* klass)
{

}

void CppDeclarationFormat::WriteEnum(SimpleOutputter& out, Database const& db, EnumDefinition const* klass)
{

}

string CppDeclarationFormat::FormatTypeName(Database const& db, TypeDefinition const* type)
{
	if (!type)
		return {};

	if (!type->IsBuiltIn())
		return format("{}::{}", FormatNamespace(db), type->Name());
	
	static map<string, string, less<>> cpp_builtin_type_names = {
		{"void", "::DataModel::NativeTypes::Void"},
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
		{"string", "::DataModel::NativeTypes::String"},
		{"bytes", "::DataModel::NativeTypes::Bytes"},
		{"flags", "::DataModel::NativeTypes::Flags"},
		{"list", "::DataModel::NativeTypes::List"},
		{"array", "::DataModel::NativeTypes::Array"},
		{"ref", "::DataModel::NativeTypes::Ref"},
		{"own", "::DataModel::NativeTypes::Own"},
		{"variant", "::DataModel::NativeTypes::Variant"},
	};

	auto it = cpp_builtin_type_names.find(type->Name());
	if (it == cpp_builtin_type_names.end())
		throw std::runtime_error(format("unrecognized builtin type: {}", type->Name()));

	return it->second;
}

string CppDeclarationFormat::FormatTypeReference(Database const& db, TypeReference const& ref)
{
	if (ref.TemplateArguments.size())
		return format("{}<{}>", FormatTypeName(db, ref.Type), string_ops::join(ref.TemplateArguments, ", ", bind_front(&CppDeclarationFormat::FormatTemplateArgument, std::ref(db))));
	return FormatTypeName(db, ref.Type);
}

string CppDeclarationFormat::FormatTemplateArgument(Database const& db, TemplateArgument const& arg)
{
	if (auto it = get_if<uint64_t>(&arg))
		return to_string(*it);
	return FormatTypeReference(db, get<TypeReference>(arg));
}

string CppDeclarationFormat::FormatNamespace(Database const& db)
{
	return db.Namespace; /// TODO: This
}

void CppDeclarationFormat::WriteStruct(SimpleOutputter& out, Database const& db, StructDefinition const* klass)
{
	if (mWrittenTypes.contains(klass)) return;
	if (klass->BaseType().Type)
	{
		WriteStruct(out, db, dynamic_cast<StructDefinition const*>(klass->BaseType().Type));
		out.WriteLine("");
	}

	if (klass->BaseType().Type)
		out.WriteStart("struct {} : {} {{", klass->Name(), FormatTypeReference(db, klass->BaseType()));
	else
		out.WriteStart("struct {} {{", klass->Name());
	for (auto& field : klass->Fields())
	{
		out.WriteLine("{} {};", FormatTypeReference(db, field->FieldType), field->Name);
	}
	out.WriteEnd("}};");
}

string CppDeclarationFormat::Export(Database const& db)
{
	stringstream outstr;
	SimpleOutputter out{ outstr };
	out.WriteLine("/// source_database: \"{}\"", string_ops::escaped(filesystem::absolute(db.Directory()).string(), "\"\\"));
	out.WriteLine("/// generated_time: \"{}\"", chrono::zoned_time{chrono::current_zone(), chrono::system_clock::now()});

	if (!db.Namespace.empty())
		out.WriteStart("namespace {} {{", db.Namespace);

	for (auto def : db.Definitions())
	{
		switch (def->Type())
		{
		case DefinitionType::Class: out.WriteLine("class {};", def->Name()); break;
		case DefinitionType::BuiltIn: continue;
		case DefinitionType::Enum: out.WriteLine("enum {};", def->Name()); break;
		case DefinitionType::Struct: out.WriteLine("struct {};", def->Name()); break;
		}
	}

	out.WriteLine("");

	mWrittenTypes.clear();
	for (auto def : db.Definitions())
	{
		switch (def->Type())
		{
		case DefinitionType::Class: WriteClass(out, db, dynamic_cast<ClassDefinition const*>(def)); break;
		case DefinitionType::Enum: WriteEnum(out, db, dynamic_cast<EnumDefinition const*>(def)); break;
		case DefinitionType::Struct: WriteStruct(out, db, dynamic_cast<StructDefinition const*>(def)); break;
		}
	}

	if (!db.Namespace.empty())
		out.WriteEnd("}}");

	return move(outstr).str();
}
