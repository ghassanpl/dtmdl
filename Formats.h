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

struct CppDeclarationFormat : FormatPlugin
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

	static string FormatTypeName(Database const& db, TypeDefinition const* type);
	static string FormatTypeReference(Database const& db, TypeReference const& ref);
	static string FormatTemplateArgument(Database const& db, TemplateArgument const& arg);
	static string FormatNamespace(Database const& db);

	void AdditionalMembers(SimpleOutputter& out, Database const& db, TypeDefinition const* type);
};