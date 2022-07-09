#pragma once

//#include "Schema.h"

struct SimpleOutputter;
struct Database;
struct ClassDefinition;
struct EnumDefinition;
struct StructDefinition;
struct TypeDefinition;
struct TypeReference;
using TemplateArgument = variant<uint64_t, TypeReference>;

struct FormatPlugin
{
	virtual ~FormatPlugin() noexcept = default;
	virtual string FormatName() = 0;
	virtual string ExportFileName() = 0;
	virtual string Export(Database const&) = 0;
};

struct JSONSchemaFormat : FormatPlugin
{
	// Inherited via FormatPlugin
	virtual string FormatName() override;
	virtual string ExportFileName() override;
	virtual string Export(Database const&) override;
};

struct CppFormatPlugin : FormatPlugin
{
protected:

	static string FormatTypeName(Database const& db, TypeDefinition const* type);
	static string FormatTypeReference(Database const& db, TypeReference const& ref);
	static string FormatTemplateArgument(Database const& db, TemplateArgument const& arg);
	static string FormatNamespace(Database const& db);

	stringstream mOutString;
	SimpleOutputter StartOutput(Database const& db, vector<string_view> includes = {});
	string FinishOutput(Database const& db);
};

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

struct CppReflectionFormat : CppFormatPlugin
{
	virtual string FormatName() override { return "C++ Reflection Header"; }
	virtual string ExportFileName() override { return "reflection.hpp"; }
	virtual string Export(Database const&) override;
};

struct CppTablesFormat : CppFormatPlugin
{
	virtual string FormatName() override { return "C++ Tables Header"; }
	virtual string ExportFileName() override { return "tables.hpp"; }
	virtual string Export(Database const&) override;
};

struct CppDatabaseFormat : CppFormatPlugin
{
	virtual string FormatName() override { return "C++ Database Header"; }
	virtual string ExportFileName() override { return "database.hpp"; }
	virtual string Export(Database const&) override;
};