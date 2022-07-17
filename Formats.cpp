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
		void WriteLine(string_view fmt, ARGS&&... args) { Write(fmt, forward<ARGS>(args)...); Nl(); }
		template <typename... ARGS>
		void WriteStart(string_view fmt, ARGS&&... args) { WriteLine(fmt, forward<ARGS>(args)...); Indent(); }
		template <typename... ARGS>
		void WriteEnd(string_view fmt, ARGS&&... args) { Unindent(); WriteLine(fmt, forward<ARGS>(args)...); }

		void Nl() { OutStream << "\n"; NewLine = true; }

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

	void CppDeclarationFormat::AdditionalMembers(SimpleOutputter& out, Database const& db, TypeDefinition const* type)
	{
		string additionals_name;
		additionals_name = format("dtmdl_{}_{}_additional_fields", db.Schema().Namespace, type->Name());
		out.WriteLine("#ifdef {}", additionals_name);
		out.WriteLine("{}", additionals_name);
		out.WriteLine("#endif");
	}

	void CppDeclarationFormat::WriteClass(SimpleOutputter& out, Database const& db, ClassDefinition const* klass)
	{
		if (klass->BaseType().Type)
			out.WriteStart("class {}{} : public {} {{", klass->Name(), (klass->Flags.contain(ClassFlags::Final) ? " final" : ""), FormatTypeReference(db, klass->BaseType()));
		else
			out.WriteStart("class {}{} : public ::dtmdl::BaseClass {{", klass->Name(), (klass->Flags.contain(ClassFlags::Final) ? " final" : ""));

		out.Unindent();
		out.WriteLine("public:");
		out.Indent();

		out.WriteLine("virtual ::dtmdl::TypeInfo const* dtmdl_Type() const noexcept override;");
		out.WriteLine("virtual void dtmdl_Mark() noexcept override;");
		out.Nl();

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

		out.Nl();

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

			out.Nl();
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

			out.Nl();
			out.Unindent();
			out.WriteLine("public:");
			out.Indent();
		}

		if (klass->Flags.contain(ClassFlags::CreateIsAs))
		{
			for (auto derived_class : db.Classes() | views::filter([klass](auto potential_child) { return potential_child->IsChildOf(klass); }))
			{
				out.WriteLine("bool Is{0}() const noexcept;", derived_class->Name());
				out.WriteLine("auto As{0}() const noexcept -> ::dtmdl::NativeTypes::Ref<{0} const>;", derived_class->Name());
				out.WriteLine("auto As{0}() noexcept -> ::dtmdl::NativeTypes::Ref<{0}>;", derived_class->Name());
			}
			out.Nl();
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

		/// TODO: Constructors/Operators:
		/// - copy
		/// - move
		/// - == and <=>
		/// - hashing

		for (auto& field : klass->Fields())
		{
			out.WriteLine("{} {} {{}};", FormatTypeReference(db, field->FieldType), field->Name);
		}

		out.Nl();

		for (auto& field : klass->Fields())
		{
			/// noexcept(noexcept({2}{{}} < {2}{{}})) 
			if (MatchesQualifier(field->FieldType, TemplateParameterQualifier::Scalar))
			{
				out.WriteLine("static auto cmpBy{0}({1} const& a, {1} const& b) {{ return a.{0} <=> b.{0}; }}", field->Name, FormatTypeName(db, klass), FormatTypeReference(db, field->FieldType));
				out.WriteLine("static bool ltBy{0}({1} const& a, {1} const& b) {{ return a.{0} < b.{0}; }}", field->Name, FormatTypeName(db, klass), FormatTypeReference(db, field->FieldType));
			}
		}

		out.Nl();

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

	string ToCppTypeReference(TypeReference const& ref);

	string ToCppTemplateArgument(TemplateArgument const& ref)
	{
		if (ref.index() == 1)
			return format("result.TemplateArguments.push_back(::dtmdl::TemplateArgument{{ {} }});", ToCppTypeReference(get<TypeReference>(ref)));
		return format("result.TemplateArguments.push_back(::dtmdl::TemplateArgument{{ {} }});", get<uint64_t>(ref));
	}

	string ToCppTypeReference(TypeReference const& ref)
	{
		/// NOTE: This is a nasty hack because using constructors directly triggers a internal compiler error on msvc
		/*
		if (ref.TemplateArguments.size())
			return format("::dtmdl::TypeReference{{ &dtmdl_{}_type_info, ::std::vector<::dtmdl::TemplateArgument>{{ {} }} }}", ref.Type->Name(), string_ops::join(ref.TemplateArguments, ", ", ToCppTemplateArgument));
		return format("::dtmdl::TypeReference{{ &dtmdl_{}_type_info }}", ref.Type->Name());
		*/
		if (!ref.Type)
			return "::dtmdl::TypeReference{}";
		return format("[]() {{ ::dtmdl::TypeReference result; result.Type = &dtmdl_{}_type_info; {} return result; }} ()", ref.Type->Name(), 
			string_ops::join(ref.TemplateArguments, " ", ToCppTemplateArgument)
		);
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

		out.Nl();

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

		for (auto def : ordered_types)
		{
			if (!def->IsBuiltIn())
				out.Nl();

			switch (def->Type())
			{
			case DefinitionType::Class: WriteClass(out, db, dynamic_cast<ClassDefinition const*>(def)); break;
			case DefinitionType::Enum: WriteEnum(out, db, dynamic_cast<EnumDefinition const*>(def)); break;
			case DefinitionType::Struct: WriteStruct(out, db, dynamic_cast<StructDefinition const*>(def)); break;
			}
		}

		out.Nl();
		
		return FinishOutput(db);
	}

	string CppReflectionFormat::Export(Database const& db)
	{
		auto out = StartOutput(db, { "types.hpp" });

		out.WriteStart("enum mirror_tags {{");
		for (auto def : db.UserDefinitions())
		{
			out.WriteLine("dtmdl_{}_Mirror_Tag,", def->Name());
		}
		out.WriteEnd("}};");

		out.Nl();

		out.WriteStart("struct dtmdl_reflection {{");
		out.WriteLine("using vt = ::dtmdl::VisitType;");

		out.WriteLine("template <typename TYPE, typename VISITOR>");
		out.WriteStart("static void PreDeserialize(VISITOR& visitor, TYPE& record) {{");
		out.WriteLine("if constexpr (::dtmdl::HasPreDeserialize<TYPE, VISITOR>) record.PreDeserialize(visitor);");
		out.WriteEnd("}}");

		out.WriteLine("template <typename TYPE, typename VISITOR>");
		out.WriteStart("static void PostDeserialize(VISITOR& visitor, TYPE& record) {{");
		out.WriteLine("if constexpr (::dtmdl::HasPostDeserialize<TYPE, VISITOR>) record.PostDeserialize(visitor);");
		out.WriteEnd("}}");

		out.WriteLine("template <typename TYPE, typename VISITOR>");
		out.WriteStart("static void PreSerialize(VISITOR& visitor, TYPE const& record) {{");
		out.WriteLine("if constexpr (::dtmdl::HasPreSerialize<TYPE, VISITOR>) record.PreSerialize(visitor);");
		out.WriteEnd("}}");

		out.WriteLine("template <typename TYPE, typename VISITOR>");
		out.WriteStart("static void PostSerialize(VISITOR& visitor, TYPE const& record) {{");
		out.WriteLine("if constexpr (::dtmdl::HasPostSerialize<TYPE, VISITOR>) record.PostSerialize(visitor);");
		out.WriteEnd("}}");

		for (auto def : db.UserDefinitions())
		{
			out.Nl();
			out.WriteLine("/// {}", FormatTypeName(db, def));
			size_t i = 0;
			switch (def->Type())
			{
			case DefinitionType::Class:
			case DefinitionType::Struct:

				out.WriteLine("template <vt VISIT_TYPE, typename VISITOR>");
				out.WriteStart("constexpr static void VisitFields(VISITOR& visitor, ::dtmdl::RefAnyConst<{}> auto record) {{", FormatTypeName(db, def));
				out.WriteLine("if constexpr (VISIT_TYPE == vt::Deserialize) PreDeserialize(visitor, record);", FormatTypeName(db, def));
				out.WriteLine("if constexpr (VISIT_TYPE == vt::Serialize) PreSerialize(visitor, record);", FormatTypeName(db, def));
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
				out.WriteLine("if constexpr (VISIT_TYPE == vt::Deserialize) PostDeserialize(visitor, record);");
				out.WriteLine("if constexpr (VISIT_TYPE == vt::Serialize) PostSerialize(visitor, record);");
				out.WriteEnd("}}");
				break;

			case DefinitionType::Enum:
				out.WriteLine("consteval static ::std::string_view EnumTypeName(::std::type_identity<{0}>) {{ return \"{0}\"; }}", def->Name());
				out.WriteLine("consteval static ::std::size_t EnumCount(::std::type_identity<{0}>) {{ return {1}; }}", FormatTypeName(db, def), def->AsEnum()->EnumeratorCount());
				out.WriteStart("consteval static ::std::array<::dtmdl::EnumeratorProperties<{0}>, {1}> EnumEntries(::std::type_identity<{0}> ti) {{", FormatTypeName(db, def), def->AsEnum()->EnumeratorCount());
				out.WriteStart("return {{");
				for (auto e : def->AsEnum()->Enumerators())
				{
					out.WriteLine("::dtmdl::EnumeratorProperties<{0}>{{ ({0}){1}, \"{2}\", \"{3}\" }},", FormatTypeName(db, def), e->ActualValue(), e->Name, e->DescriptiveName);
				}
				out.WriteEnd("}};");
				out.WriteEnd("}}");
				out.WriteLine("constexpr static {0} EnumValue(::std::type_identity<{0}> ti, ::std::size_t i) {{ return EnumEntries(ti)[i].Value; }}", FormatTypeName(db, def));
				out.WriteLine("template <::std::size_t I>");
				out.WriteLine("requires (I < {})", def->AsEnum()->EnumeratorCount());
				out.WriteLine("consteval static {0} EnumValue(::std::type_identity<{0}> ti) {{ return EnumEntries(ti)[I].Value; }}", FormatTypeName(db, def));
				out.WriteLine("constexpr static ::std::string_view EnumName(::std::type_identity<{0}> ti, ::std::size_t i) {{ return EnumEntries(ti)[i].Name; }}", FormatTypeName(db, def));
				out.WriteLine("template <::std::size_t I>");
				out.WriteLine("requires (I < {})", def->AsEnum()->EnumeratorCount());
				out.WriteLine("consteval static {0} EnumName(::std::type_identity<{0}> ti) {{ return EnumEntries(ti)[I].Name; }}", FormatTypeName(db, def));

				out.WriteStart("constexpr static bool EnumValid(::std::type_identity<{0}> ti, {0} val) {{ ", FormatTypeName(db, def));
				out.WriteStart("switch (val) {{");
				i = 0;
				for (auto e : def->AsEnum()->Enumerators())
				{
					out.WriteLine("case {}::{}: return true;", FormatTypeName(db, def), e->Name, i);
					++i;
				}
				out.WriteEnd("}}");
				out.WriteLine("return false;");
				out.WriteEnd("}}");

				out.WriteStart("constexpr static ::std::optional<::std::size_t> EnumIndex(::std::type_identity<{0}> ti, {0} val) {{ ", FormatTypeName(db, def));
				out.WriteStart("switch (val) {{");
				i = 0;
				for (auto e : def->AsEnum()->Enumerators())
				{
					out.WriteLine("case {}::{}: return {};", FormatTypeName(db, def), e->Name, i);
					++i;
				}
				out.WriteEnd("}}");
				out.WriteLine("return ::std::nullopt;");
				out.WriteEnd("}}");

				out.WriteStart("constexpr static ::std::optional<{0}> EnumCast(::std::type_identity<{0}> ti, ::std::string_view val) {{ ", FormatTypeName(db, def));
				out.WriteStart("for (auto&& [value, name, desc] : EnumEntries(ti)) {{");
				out.WriteLine("if (val == name) return value;");
				out.WriteEnd("}}");
				out.WriteLine("return ::std::nullopt;");
				out.WriteEnd("}}");

				/// TODO: This
				/*
				out.WriteStart("constexpr static ::dtmdl::Flags<{0}> EnumCastFlags(::std::type_identity<{0}> ti, ::std::string_view val) {{ ", FormatTypeName(db, def));
				out.WriteEnd("}}");
				*/

				out.WriteLine("template <{} V>", FormatTypeName(db, def));
				out.WriteStart("consteval static ::std::size_t EnumIndex() {{ ", FormatTypeName(db, def));
				out.WriteLine("return EnumIndex(::std::type_identity<{}>{{}}, V);", FormatTypeName(db, def));
				out.WriteEnd("}}");

				out.WriteLine("template <typename VISITOR>");
				out.WriteStart("constexpr static void VisitEnumerators(VISITOR& visitor, ::std::type_identity<{}>) {{", FormatTypeName(db, def));
				out.WriteEnd("}}");
				break;
			}
		}

		out.Nl();
		out.WriteLine("private:");
		out.WriteLine("friend struct dtmdl_database;");

		for (auto klass : db.Classes())
		{
			out.Nl();
			out.WriteLine("/// {}", FormatTypeName(db, klass));

			out.WriteLine("static [[nodiscard]] {0}* New{1}() {{ return new {0}(); }}", FormatTypeName(db, klass), klass->Name());
			out.WriteLine("static [[nodiscard]] {0}* New(::std::type_identity<{0}>) {{ return new {0}(); }}", FormatTypeName(db, klass), klass->Name());
			out.WriteLine("static {0}* Construct{1}(void* memory) {{ return new (memory){0}(); }}", FormatTypeName(db, klass), klass->Name());
			out.WriteLine("static void Destruct{1}(void* memory) {{ reinterpret_cast<{0}*>(memory)->~{1}(); }}", FormatTypeName(db, klass), klass->Name());
			out.WriteLine("static void Destruct({0}* memory) {{ memory->~{1}(); }}", FormatTypeName(db, klass), klass->Name());
			out.WriteLine("static void Delete{1}(void* memory) {{ delete reinterpret_cast<{0}*>(memory); }}", FormatTypeName(db, klass), klass->Name());
			out.WriteLine("static void Delete({0}* memory) {{ delete memory; }}", FormatTypeName(db, klass), klass->Name());
		}

		out.WriteEnd("}};");

		for (auto def : db.UserDefinitions())
		{
			out.WriteLine("::std::type_identity<dtmdl_reflection> ReflectionTypeFor(::std::type_identity<{}>) {{ return {{}}; }}", FormatTypeName(db, def));
		}

		out.Nl();

		size_t count = 0;
		for (auto def : db.UserDefinitions())
		{
			out.WriteLine("extern ::dtmdl::TypeInfo const dtmdl_{}_type_info;", def->Name());
			++count;
		}

		out.WriteStart("constexpr inline ::std::array<::dtmdl::TypeInfo const*, {}> dtmdl_types {{", count);
		for (auto def : db.UserDefinitions())
		{
			out.WriteLine("&dtmdl_{}_type_info,", def->Name());
		}
		out.WriteEnd("}};");
		out.Nl();

		for (auto def : db.Structs())
		{
			out.WriteLine("inline void dtmdl_MarkStruct({0}& obj);", FormatTypeName(db, def));
		}
		out.Nl();

		for (auto klass : db.Classes())
		{
			out.WriteStart("inline void {}::dtmdl_Mark() noexcept {{", klass->Name());
			out.WriteLine("using ::dtmdl::dtmdl_Mark;");
			for (auto& field : klass->Fields())
			{
				out.WriteLine("dtmdl_Mark(this->{});", MemberName(db, field.get()));
			}
			out.WriteEnd("}}");
			out.WriteLine("inline ::dtmdl::TypeInfo const* {0}::dtmdl_Type() const noexcept {{ return &dtmdl_{0}_type_info; }}", klass->Name());

			if (klass->Flags.contain(ClassFlags::CreateIsAs))
			{
				for (auto derived_class : db.Classes() | views::filter([klass](auto potential_child) { return potential_child->IsChildOf(klass); }))
				{
					out.WriteLine("inline bool {1}::Is{0}() const noexcept {{ return this->dtmdl_Type() == &dtmdl_{0}_type_info; }}", derived_class->Name(), klass->Name());
					out.WriteLine("inline auto {1}::As{0}() const noexcept -> ::dtmdl::NativeTypes::Ref<{0} const> {{ return Is{0}() ? reinterpret_cast<{0} const*>(this) : nullptr; }}", derived_class->Name(), klass->Name());
					out.WriteLine("inline auto {1}::As{0}() noexcept -> ::dtmdl::NativeTypes::Ref<{0}> {{ return Is{0}() ? reinterpret_cast<{0}*>(this) : nullptr; }}", derived_class->Name(), klass->Name());
				}
			}
		}
		out.Nl();

		for (auto def : db.Structs())
		{
			out.WriteStart("inline void dtmdl_MarkStruct({0}& obj) {{", FormatTypeName(db, def));
			out.WriteLine("using ::dtmdl::dtmdl_Mark;");
			for (auto& field : def->Fields())
			{
				out.WriteLine("dtmdl_Mark(obj.{});", field->Name);
			}
			out.WriteEnd("}}");
		}

		for (auto def : db.Definitions())
		{
			auto qname = FormatTypeName(db, def);

			out.WriteStart("static inline ::dtmdl::TypeInfo const dtmdl_{}_type_info = {{", def->Name());
			out.WriteLine(".Schema = {{}},");
			out.WriteLine(".Name = \"{}\",", def->Name());
			if (auto klass = def->AsClass()) out.WriteLine(".Flags = {},", klass->Flags.bits);
			if (auto strukt = def->AsStruct()) out.WriteLine(".Flags = {},", strukt->Flags.bits);
			out.WriteLine(".BaseType = {},", ToCppTypeReference(def->BaseType()));
			out.WriteLine(".TypeType = ::dtmdl::DefinitionType::{},", magic_enum::enum_name(def->Type()));

			if (!(def->IsBuiltIn() && def->TemplateParameters().size()))
			{
				out.WriteLine(".SizeOf = sizeof({}),", qname);
				out.WriteLine(".AlignOf = alignof({}),", qname);
				switch (def->Type())
				{
				case DefinitionType::Class:
					/// TODO: Hmmmmmmmmmmmmmmmmmmm
					//out.WriteLine(".Constructor = [](void* mem) {{ dtmdl_reflection::Construct{}(mem); }},", def->Name());
					//out.WriteLine(".Destructor = [](void* mem) {{ dtmdl_reflection::Destruct{}(mem);  }},", def->Name());
					break;
				default:
					out.WriteLine(".Constructor = [](void* mem) {{ new(mem) {}; }},", qname);
					out.WriteLine(".Destructor = [](void* mem) {{ ::std::destroy_at(reinterpret_cast<{}*>(mem)); }},", qname, qname);
					break;
				}
			}
			if (auto rec = def->AsRecord(); rec && rec->Fields().size())
			{
				out.WriteStart(".Members = {{");

				for (auto& fld : rec->Fields())
				{
					auto field_type = FormatTypeReference(db, fld->FieldType);
					auto field_name = MemberName(db, fld.get());

					out.WriteStart("{{");

					out.WriteLine(".ParentType = &dtmdl_{}_type_info,", def->Name());
					out.WriteLine(".Name = \"{}\",", fld->Name);
					out.WriteLine(".Flags = {},", fld->Flags.bits);
					out.WriteLine(".MemberType = 'fld',");
					out.WriteLine(".FieldType = {},", ToCppTypeReference(fld->FieldType));
					out.WriteLine(".TypeIndex = typeid({}),", field_type);
					

					if (fld->Flags.contain(FieldFlags::Private) && fld->Flags.contain(FieldFlags::Getter))
						out.WriteLine(".GetValue = [](void const* obj) -> void const* {{ return &reinterpret_cast<{} const*>(obj)->{}(); }},", FormatTypeName(db, def), fld->Name);
					else if (!fld->Flags.contain(FieldFlags::Private))
						out.WriteLine(".GetValue = [](void const* obj) -> void const* {{ return &reinterpret_cast<{} const*>(obj)->{}; }},", FormatTypeName(db, def), field_name);
					if (fld->Flags.contain(FieldFlags::Private) && fld->Flags.contain(FieldFlags::Setter))
					{
						if (IsCopyable(fld->FieldType)) out.WriteLine(".SetValueCopy = [](void* obj, void const* from) {{ reinterpret_cast<{}*>(obj)->Set{}(*reinterpret_cast<{} const*>(from)); }},", FormatTypeName(db, def), fld->Name, field_type);
						out.WriteLine(".SetValueMove = [](void* obj, void* from) {{ reinterpret_cast<{}*>(obj)->Set{}(::std::move(*reinterpret_cast<{}*>(from))); }},", FormatTypeName(db, def), fld->Name, field_type);
					}
					else if (!fld->Flags.contain(FieldFlags::Private))
					{
						if (IsCopyable(fld->FieldType)) out.WriteLine(".SetValueCopy = [](void* obj, void const* from) {{ reinterpret_cast<{}*>(obj)->{} = *reinterpret_cast<{} const*>(from); }},", FormatTypeName(db, def), field_name, field_type);
						out.WriteLine(".SetValueMove = [](void* obj, void* from) {{ reinterpret_cast<{}*>(obj)->{} = ::std::move(*reinterpret_cast<{}*>(from)); }},", FormatTypeName(db, def), field_name, field_type);
					}
				
					out.WriteEnd("}},");
				}

				out.WriteEnd("}},");
			}
			else if (auto enoom = def->AsEnum())
			{
				out.WriteStart(".Members = {{");

				for (auto e : enoom->Enumerators())
				{
					out.WriteStart("{{");
					out.WriteLine(".ParentType = &dtmdl_{}_type_info,", def->Name());
					out.WriteLine(".Name = \"{}\",", e->Name);
					if (!e->DescriptiveName.empty())
						out.WriteLine(".DescriptiveName = \"{}\",", e->DescriptiveName);
					out.WriteLine(".MemberType = 'enum',");
					out.WriteLine(".EnumValue = {},", e->ActualValue());
					out.WriteLine(".TypeIndex = typeid(void),");
					out.WriteEnd("}},");
				}

				out.WriteEnd("}},");
			}

			if (!(def->IsBuiltIn() && def->TemplateParameters().size()))
				out.WriteLine(".TypeIndex = typeid({})", qname);
			else
				out.WriteLine(".TypeIndex = typeid(void)", qname);
			out.WriteEnd("}};");
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