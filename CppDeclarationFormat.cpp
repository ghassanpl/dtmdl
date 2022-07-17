#include "pch.h"
#include "CppDeclarationFormat.h"
#include "Schema.h"
#include "Database.h"
#include "Validation.h"

namespace dtmdl
{

	string CppDeclarationFormat::FormatName()
	{
		return "C++ Type Header";
	}

	string CppDeclarationFormat::ExportFileName()
	{
		return "types.hpp";
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

}