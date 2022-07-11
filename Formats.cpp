#include "pch.h"
#include "Formats.h"
#include "Schema.h"
#include "Database.h"
//#include <sstream>
//#include <chrono>

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
	void Unindent() { if (Indentation == 0) throw "invalid unindent"; Indentation--; }
};

string MemberName(Database const& db, FieldDefinition const* def)
{
	if (def->Flags.contain(FieldFlags::Private))
		return format("{}{}", db.PrivateFieldPrefix, def->Name);
	return def->Name;
}

string CppDeclarationFormat::FormatName()
{
	return "C++ Type Header";
}

string CppDeclarationFormat::ExportFileName()
{
	return "types.hpp";
}

string CppFormatPlugin::FormatTypeName(Database const& db, TypeDefinition const* type)
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
		{"map", "::DataModel::NativeTypes::Map"},
		{"json", "::DataModel::NativeTypes::JSON"},

		{"vec2", "::DataModel::NativeTypes::vec<2, float>"},
		{"vec3", "::DataModel::NativeTypes::vec<3, float>"},
		{"vec4", "::DataModel::NativeTypes::vec<4, float>"},

		{"dvec2", "::DataModel::NativeTypes::vec<2, double>"},
		{"dvec3", "::DataModel::NativeTypes::vec<3, double>"},
		{"dvec4", "::DataModel::NativeTypes::vec<4, double>"},

		{"ivec2", "::DataModel::NativeTypes::vec<2, int>"},
		{"ivec3", "::DataModel::NativeTypes::vec<3, int>"},
		{"ivec4", "::DataModel::NativeTypes::vec<4, int>"},

		{"uvec2", "::DataModel::NativeTypes::vec<2, unsigned>"},
		{"uvec3", "::DataModel::NativeTypes::vec<3, unsigned>"},
		{"uvec4", "::DataModel::NativeTypes::vec<4, unsigned>"},

		{"bvec2", "::DataModel::NativeTypes::vec<2, bool>"},
		{"bvec3", "::DataModel::NativeTypes::vec<3, bool>"},
		{"bvec4", "::DataModel::NativeTypes::vec<4, bool>"},
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
	return db.Namespace; /// TODO: This
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

	if (!db.Namespace.empty())
		out.WriteStart("namespace {} {{", db.Namespace);

	return out;
}

string CppFormatPlugin::FinishOutput(Database const& db)
{
	SimpleOutputter out{ mOutString };

	if (!db.Namespace.empty())
		out.WriteLine("}}");

	return move(mOutString).str();
}

void CppDeclarationFormat::AdditionalMembers(SimpleOutputter& out, Database const& db, TypeDefinition const* type)
{
	string additionals_name;
	if (db.Namespace.empty())
		additionals_name = format("dtmdl_additional_fields_for_{}", type->Name());
	else
		additionals_name = format("dtmdl_additional_fields_for_{}_{}", db.Namespace, type->Name());
	out.WriteLine("#ifdef {}", additionals_name);
	out.WriteLine("{}", additionals_name);
	out.WriteLine("#endif");
}

void CppDeclarationFormat::WriteClass(SimpleOutputter& out, Database const& db, ClassDefinition const* klass)
{
	if (klass->BaseType().Type)
		out.WriteStart("class {}{} : public {} {{", klass->Name(), (klass->Flags.contain(ClassFlags::Final) ? " final" : ""), FormatTypeReference(db, klass->BaseType()));
	else
		out.WriteStart("class {}{} : public ::DataModel::BaseClass {{", klass->Name(), (klass->Flags.contain(ClassFlags::Final) ? " final" : ""));

	out.Unindent();
	out.WriteLine("public:");
	out.Indent();

	out.WriteLine("virtual ::DataModel::dtmdl_TypeInfo const* dtmdl_Type() const noexcept override {{ return &dtmdl_{0}_type_info; }}", klass->Name());
	out.WriteLine("virtual void dtmdl_Mark() noexcept override;");
	out.WriteLine("");

	bool any_accessors = false;
	bool any_privates = false;

	for (auto& field : klass->Fields())
	{
		if (!field->Flags.contain(FieldFlags::Private))
			out.WriteLine("{} {} {{}};", FormatTypeReference(db, field->FieldType), field->Name);
		else
			any_privates = true;
		any_accessors |= (field->Flags.contain(FieldFlags::Getter) || field->Flags.contain(FieldFlags::Setter));
	}

	out.WriteLine("");

	if (any_accessors)
	{
		for (auto& field : klass->Fields())
		{
			if (field->Flags.contain(FieldFlags::Getter))
				out.WriteLine("auto const& {}() const noexcept {{ return {}; }}", field->Name, MemberName(db, field.get()));
			if (field->Flags.contain(FieldFlags::Setter))
			{
				out.WriteLine("void Set{1}({0}&& value) const noexcept {{ {2} = ::std::move<{0}>(value); }}", FormatTypeReference(db, field->FieldType), field->Name, MemberName(db, field.get()));
				out.WriteLine("void Set{1}({0} const& value) const noexcept {{ {2} = value; }}", FormatTypeReference(db, field->FieldType), field->Name, MemberName(db, field.get()));
			}
		}

		out.WriteLine("");
	}
	
	if (any_privates)
	{
		out.Unindent();
		out.WriteLine("private:");
		out.Indent();

		for (auto& field : klass->Fields())
		{
			if (field->Flags.contain(FieldFlags::Private))
				out.WriteLine("{} {} {{}};", FormatTypeReference(db, field->FieldType), MemberName(db, field.get()));
		}

		out.WriteLine("");
		out.Unindent();
		out.WriteLine("public:");
		out.Indent();
	}

	if (klass->Flags.contain(ClassFlags::CreateIsAs))
	{
		for (auto derived_class : db.Classes() | views::filter([klass](auto potential_child) { return potential_child->IsChildOf(klass); }))
		{
			out.WriteLine("bool Is{0}() const noexcept {{ return this->dtmdl_Type() == &dtmdl_{0}_type_info; }}", derived_class->Name());
			out.WriteLine("auto As{0}() const noexcept -> ::DataModel::NativeTypes::Ref<{0} const> {{ return Is{0}() ? reinterpret_cast<{0} const*>(this) : nullptr; }}", derived_class->Name());
			out.WriteLine("auto As{0}() noexcept -> ::DataModel::NativeTypes::Ref<{0}> {{ return Is{0}() ? reinterpret_cast<{0}*>(this) : nullptr; }}", derived_class->Name());
		}
		out.WriteLine("");
	}

	AdditionalMembers(out, db, klass);

	out.Unindent();
	out.WriteLine("protected:");
	out.Indent();

	out.WriteLine("{}() noexcept = default;", klass->Name());
	out.WriteLine("{0}({0}&&) noexcept = delete;", klass->Name());
	out.WriteLine("{0}({0} const&) noexcept = delete;", klass->Name());
	out.WriteLine("{0} operator=({0}&&) noexcept = delete;", klass->Name());
	out.WriteLine("{0} operator=({0} const&) noexcept = delete;", klass->Name());

	out.WriteLine("friend struct dtmdl_reflection;");

	out.WriteEnd("}};");
}

void CppDeclarationFormat::WriteStruct(SimpleOutputter& out, Database const& db, StructDefinition const* klass)
{
	if (klass->BaseType().Type)
		out.WriteStart("struct {} : {} {{", klass->Name(), FormatTypeReference(db, klass->BaseType()));
	else
		out.WriteStart("struct {} {{", klass->Name());
	for (auto& field : klass->Fields())
	{
		out.WriteLine("{} {} {{}};", FormatTypeReference(db, field->FieldType), field->Name);
	}

	out.WriteLine("friend struct dtmdl_reflection;");

	AdditionalMembers(out, db, klass);
	out.WriteEnd("}};");
}

void CppDeclarationFormat::WriteEnum(SimpleOutputter& out, Database const& db, EnumDefinition const* enoom)
{
	out.WriteStart("enum class {} {{", enoom->Name());
	for (auto enumerator : enoom->Enumerators())
	{
		out.WriteLine("{} = {},", enumerator->Name, enumerator->ActualValue());
	}
	out.WriteEnd("}};");
}

string CppDeclarationFormat::Export(Database const& db)
{
	auto out = StartOutput(db);

	for (auto def : db.UserDefinitions())
	{
		switch (def->Type())
		{
		case DefinitionType::Class: out.WriteLine("class {};", def->Name()); break;
		case DefinitionType::Enum: out.WriteLine("enum class {};", def->Name()); break;
		case DefinitionType::Struct: out.WriteLine("struct {};", def->Name()); break;
		}
	}

	out.WriteLine("");

	out.WriteStart("enum mirror_tags {{");
	for (auto def : db.UserDefinitions())
	{
		out.WriteLine("dtmdl_{}_Mirror_Tag,", def->Name());
	}
	out.WriteEnd("}};");

	out.WriteLine("");

	size_t count = 0;
	for (auto def : db.UserDefinitions())
	{
		out.WriteLine("extern ::DataModel::dtmdl_TypeInfo const dtmdl_{}_type_info;", def->Name());
		++count;
	}

	out.WriteStart("constexpr inline ::std::array<::DataModel::dtmdl_TypeInfo const*, {}> dtmdl_types {{", count);
	for (auto def : db.UserDefinitions())
	{
		out.WriteLine("&dtmdl_{}_type_info,", def->Name());
	}
	out.WriteEnd("}};");

	out.WriteLine("");

	set<TypeDefinition const*> closed_types;
	vector<TypeDefinition const*> ordered_types;

	auto calc_deps = [&](this auto& self, TypeDefinition const* def) {
		if (closed_types.contains(def))
			return;

		set<TypeDefinition const*> dependencies;
		def->CalculateDependencies(dependencies);

		for (auto dep : dependencies)
			self(dep);

		closed_types.insert(def);
		ordered_types.push_back(def);
	};

	for (auto def : db.UserDefinitions())
	{
		calc_deps(def);
	}

	//mWrittenTypes.clear();
	for (auto def : ordered_types)
	{
		switch (def->Type())
		{
		case DefinitionType::Class: WriteClass(out, db, dynamic_cast<ClassDefinition const*>(def)); break;
		case DefinitionType::Enum: WriteEnum(out, db, dynamic_cast<EnumDefinition const*>(def)); break;
		case DefinitionType::Struct: WriteStruct(out, db, dynamic_cast<StructDefinition const*>(def)); break;
		}
	}

	out.WriteLine("");

	return FinishOutput(db);
}

string CppReflectionFormat::Export(Database const& db)
{
	auto out = StartOutput(db, { "types.hpp" });

	out.WriteStart("struct dtmdl_reflection {{");
	out.WriteLine("using vt = ::DataModel::VisitType;");

	out.WriteLine("template <typename TYPE, typename VISITOR>");
	out.WriteStart("static void PreDeserialize(VISITOR& visitor, TYPE& record) {{");
	out.WriteLine("if constexpr (::DataModel::HasPreDeserialize<TYPE, VISITOR>) record.PreDeserialize(visitor);");
	out.WriteEnd("}}");

	out.WriteLine("template <typename TYPE, typename VISITOR>");
	out.WriteStart("static void PostDeserialize(VISITOR& visitor, TYPE& record) {{");
	out.WriteLine("if constexpr (::DataModel::HasPostDeserialize<TYPE, VISITOR>) record.PostDeserialize(visitor);");
	out.WriteEnd("}}");

	out.WriteLine("template <typename TYPE, typename VISITOR>");
	out.WriteStart("static void PreSerialize(VISITOR& visitor, TYPE const& record) {{");
	out.WriteLine("if constexpr (::DataModel::HasPreSerialize<TYPE, VISITOR>) record.PreSerialize(visitor);");
	out.WriteEnd("}}");

	out.WriteLine("template <typename TYPE, typename VISITOR>");
	out.WriteStart("static void PostSerialize(VISITOR& visitor, TYPE const& record) {{");
	out.WriteLine("if constexpr (::DataModel::HasPostSerialize<TYPE, VISITOR>) record.PostSerialize(visitor);");
	out.WriteEnd("}}");

	for (auto def : db.UserDefinitions())
	{
		out.WriteLine("");
		out.WriteLine("/// {}", def->Name());
		size_t i = 0;
		switch (def->Type())
		{
		case DefinitionType::Class:
		case DefinitionType::Struct:

			out.WriteLine("template <vt VISIT_TYPE, typename VISITOR>");
			out.WriteStart("constexpr static void VisitFields(VISITOR& visitor, ::DataModel::RefAnyConst<{}> auto record) {{", def->Name());
			out.WriteLine("if constexpr (VISIT_TYPE == vt::Deserialize) PreDeserialize(visitor, record);", def->Name());
			out.WriteLine("if constexpr (VISIT_TYPE == vt::Serialize) PreSerialize(visitor, record);", def->Name());
			for (auto& fld : def->AsRecord()->AllFieldsOrdered())
			{
				set<string> unwanted_visitors;
				if (fld->Flags.contain(FieldFlags::Transient))
				{
					unwanted_visitors.insert("Deserialize");
					unwanted_visitors.insert("Serialize");
				}
				if (fld->Flags.contain(FieldFlags::Private))
				{
					if (!fld->Flags.contain(FieldFlags::Setter))
						unwanted_visitors.insert("Edit");
					if (!fld->Flags.contain(FieldFlags::Getter))
						unwanted_visitors.insert("View");
				}
				if (fld->Flags.contain(FieldFlags::NoEdit)) unwanted_visitors.insert("Edit");
				if (fld->Flags.contain(FieldFlags::NoView)) unwanted_visitors.insert("View");
				if (fld->Flags.contain(FieldFlags::NoDebug)) unwanted_visitors.insert("Debug");
				if (fld->Flags.contain(FieldFlags::NoClone)) unwanted_visitors.insert("Clone");
				if (fld->Flags.contain(FieldFlags::NoSerialize)) unwanted_visitors.insert("Serialize");
				if (fld->Flags.contain(FieldFlags::NoDeserialize)) unwanted_visitors.insert("Deserialize");

				if (unwanted_visitors.size())
				{
					out.WriteLine("if constexpr (!({}))", string_ops::join(unwanted_visitors | views::transform([](string const& visitor) { return "VISIT_TYPE == vt::" + visitor; }), " || "));
					out.Indent();
				}
				out.WriteLine("visitor(record.{}, \"{}\", dtmdl_{}_Mirror_Tag);", MemberName(db, fld), fld->Name, fld->ParentRecord->Name());
				if (unwanted_visitors.size())
					out.Unindent();
			}
			out.WriteLine("if constexpr (VISIT_TYPE == vt::Deserialize) PostDeserialize(visitor, record);", def->Name());
			out.WriteLine("if constexpr (VISIT_TYPE == vt::Serialize) PostSerialize(visitor, record);", def->Name());
			out.WriteEnd("}}");
			break;

			/*
			out.WriteLine("template <typename VISITOR>");
			out.WriteStart("constexpr static void Deserialize(VISITOR& visitor, {}& record) {{", def->Name());
			out.WriteLine("PreDeserialize(visitor, record);", def->Name());
			for (auto& fld : def->AsRecord()->AllFieldsOrdered())
			{
				if (!fld->Flags.contain(FieldFlags::Transient))
					out.WriteLine("visitor(record.{0}, \"{0}\", dtmdl_{1}_Mirror_Tag);", fld->Name, fld->ParentRecord->Name());
			}
			out.WriteEnd("}}");

			out.WriteLine("template <typename VISITOR>");
			out.WriteStart("constexpr static void Serialize(VISITOR& visitor, {} const& record) {{", def->Name());
			out.WriteLine("PreSerialize(visitor, record);", def->Name());
			for (auto& fld : def->AsRecord()->AllFieldsOrdered())
			{
				if (!fld->Flags.contain(FieldFlags::Transient))
					out.WriteLine("visitor(record.{0}, \"{0}\", dtmdl_{1}_Mirror_Tag);", fld->Name, fld->ParentRecord->Name());
			}
			out.WriteEnd("}}");

			out.WriteLine("template <typename VISITOR>");
			out.WriteStart("constexpr static void VisitFields(VISITOR& visitor, ::DataModel::RefAnyConst<{}> auto record) {{", def->Name());
			for (auto& fld : def->AsRecord()->AllFieldsOrdered())
				out.WriteLine("visitor(record.{0}, \"{0}\", dtmdl_{1}_Mirror_Tag);", fld->Name, fld->ParentRecord->Name());
			out.WriteEnd("}}");
			break;
			*/
		case DefinitionType::Enum:
			out.WriteLine("consteval static ::std::string_view EnumTypeName(::std::type_identity<{0}>) {{ return \"{0}\"; }}", def->Name());
			out.WriteLine("consteval static ::std::size_t EnumCount(::std::type_identity<{0}>) {{ return {1}; }}", def->Name(), def->AsEnum()->EnumeratorCount());
			out.WriteStart("consteval static ::std::array<::DataModel::EnumeratorProperties<{0}>, {1}> EnumEntries(::std::type_identity<{0}> ti) {{", def->Name(), def->AsEnum()->EnumeratorCount());
			out.WriteStart("return {{");
			for (auto e : def->AsEnum()->Enumerators())
			{
				out.WriteLine("::DataModel::EnumeratorProperties<{0}>{{ ({0}){1}, \"{2}\", \"{3}\" }},", def->Name(), e->ActualValue(), e->Name, e->DescriptiveName);
			}
			out.WriteEnd("}};");
			out.WriteEnd("}}");
			out.WriteLine("constexpr static {0} EnumValue(::std::type_identity<{0}> ti, ::std::size_t i) {{ return EnumEntries(ti)[i].Value; }}", def->Name());
			out.WriteLine("template <::std::size_t I>");
			out.WriteLine("requires (I < {})", def->AsEnum()->EnumeratorCount());
			out.WriteLine("consteval static {0} EnumValue(::std::type_identity<{0}> ti) {{ return EnumEntries(ti)[I].Value; }}", def->Name());
			out.WriteLine("constexpr static ::std::string_view EnumName(::std::type_identity<{0}> ti, ::std::size_t i) {{ return EnumEntries(ti)[i].Name; }}", def->Name());
			out.WriteLine("template <::std::size_t I>");
			out.WriteLine("requires (I < {})", def->AsEnum()->EnumeratorCount());
			out.WriteLine("consteval static {0} EnumName(::std::type_identity<{0}> ti) {{ return EnumEntries(ti)[I].Name; }}", def->Name());

			out.WriteStart("constexpr static bool EnumValid(::std::type_identity<{0}> ti, {0} val) {{ ", def->Name());
			out.WriteStart("switch (val) {{");
			i = 0;
			for (auto e : def->AsEnum()->Enumerators())
			{
				out.WriteLine("case {}::{}: return true;", def->Name(), e->Name, i);
				++i;
			}
			out.WriteEnd("}}");
			out.WriteLine("return false;");
			out.WriteEnd("}}");

			out.WriteStart("constexpr static ::std::optional<::std::size_t> EnumIndex(::std::type_identity<{0}> ti, {0} val) {{ ", def->Name());
			out.WriteStart("switch (val) {{");
			i = 0;
			for (auto e : def->AsEnum()->Enumerators())
			{
				out.WriteLine("case {}::{}: return {};", def->Name(), e->Name, i);
				++i;
			}
			out.WriteEnd("}}");
			out.WriteLine("return ::std::nullopt;");
			out.WriteEnd("}}");

			out.WriteStart("constexpr static ::std::optional<{0}> EnumCast(::std::type_identity<{0}> ti, ::std::string_view val) {{ ", def->Name());
			out.WriteStart("for (auto&& [value, name, desc] : EnumEntries(ti)) {{");
			out.WriteLine("if (val == name) return value;");
			out.WriteEnd("}}");
			out.WriteLine("return ::std::nullopt;");
			out.WriteEnd("}}");

			/// TODO: This
			/*
			out.WriteStart("constexpr static ::DataModel::Flags<{0}> EnumCastFlags(::std::type_identity<{0}> ti, ::std::string_view val) {{ ", def->Name());
			out.WriteEnd("}}");
			*/

			out.WriteLine("template <{} V>", def->Name());
			out.WriteStart("consteval static ::std::size_t EnumIndex() {{ ", def->Name());
			out.WriteLine("return EnumIndex(::std::type_identity<{}>{{}}, V);", def->Name());
			out.WriteEnd("}}");

			out.WriteLine("template <typename VISITOR>");
			out.WriteStart("constexpr static void VisitEnumerators(VISITOR& visitor, ::std::type_identity<{}>) {{", def->Name());
			out.WriteEnd("}}");
			break;
		}
	}
	out.WriteEnd("}};");

	for (auto def : db.UserDefinitions())
	{
		out.WriteLine("::std::type_identity<dtmdl_reflection> ReflectionTypeFor(::std::type_identity<{}>) {{ return {{}}; }}", def->Name());
	}

	return FinishOutput(db);
}

string CppTablesFormat::Export(Database const& db)
{
	auto out = StartOutput(db, { "types.hpp" });

	for (auto def : db.Structs())
	{
		if (!def->Flags.contain(StructFlags::CreateTableType))
			continue;

		out.WriteStart("struct dtmdl_{0}Table : ::DataModel::TableBase<dtmdl_{0}Table> {{", def->Name());
		out.WriteLine("using RowType = {};", def->Name());
		out.WriteLine("template <::DataModel::FixedString COLUMN>");
		out.WriteStart("static constexpr auto GetField() {{");
		for (auto& field : def->AllFieldsOrdered())
		{
			out.WriteLine("if constexpr (COLUMN.eq(\"{}\")) {{ return &{}::{}; }} else", field->Name, def->Name(), MemberName(db, field));
		}
		out.WriteLine("static_assert(::std::is_same_v<decltype(COLUMN), void>, \"column name not an (accessible) field in {}\");", def->Name());
		out.WriteEnd("}}");

		out.Unindent();
		out.WriteLine("protected:");
		out.Indent();
		out.WriteLine("friend struct ::DataModel::TableBase<dtmdl_{}Table>;", def->Name());
		out.WriteLine("::std::int64_t mLastRowID = 0;");
		out.WriteLine("::std::map<::std::int64_t, {}> mRows;", def->Name());
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

string CppDatabaseFormat::Export(Database const& db)
{
	auto out = StartOutput(db, { "types.hpp" });

	out.WriteStart("struct dtmdl_database {{");
	out.WriteEnd("}};");

	return FinishOutput(db);
}
