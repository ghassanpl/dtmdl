#include "pch.h"
#include "CSharpFormats.h"
#include "Schema.h"
#include "Database.h"
#include "Validation.h"

namespace dtmdl
{
	string CSharpFormatPlugin::FormatTypeReference(Database const& db, TypeReference const& ref)
	{
		if (!ref.Type)
			return {};
		if (ref.Type->Name() == "ref")
			return FormatTypeReference(db, get<TypeReference>(ref.TemplateArguments[0]));
		else if (ref.Type->Name() == "array")
			return FormatTypeReference(db, get<TypeReference>(ref.TemplateArguments[0])) + "[]";

		if (ref.TemplateArguments.size())
			return format("{}<{}>", FormatTypeName(db, ref.Type), string_ops::join(ref.TemplateArguments, ", ", bind_front(&CSharpDeclarationFormat::FormatTemplateArgument, std::ref(db))));
		return FormatTypeName(db, ref.Type);
	}

	string CSharpFormatPlugin::FormatTypeName(Database const& db, TypeDefinition const* type)
	{
		if (!type)
			return {};

		if (!type->IsBuiltIn())
			return format("{}.{}", FormatNamespace(db), type->Name());

		static map<string, string, less<>> csharp_builtin_type_names = {
			{"void", "dtmdl.Void"},
			{"f32", "float"},
			{"f64", "double"},
			{"i8", "sbyte"},
			{"i16", "short"},
			{"i32", "int"},
			{"i64", "long"},
			{"u8", "byte"},
			{"u16", "ushort"},
			{"u32", "uint"},
			{"u64", "ulong"},
			{"bool", "bool"},
			{"string", "string"},
			{"bytes", "System.Collections.Generic.List<byte>"},
			{"flags", "dtmdl.NativeTypes.Flags"},
			{"list", "System.Collections.Generic.List"},
			{"own", "dtmdl.NativeTypes.Own"},
			//{"variant", "::dtmdl::NativeTypes::Variant"},
			{"map", "System.Collections.Generic.Dictionary"},
			{"json", "dtmdl.NativeTypes.JSON"},

			{"vec2", "System.Numerics.Vector2"},
			{"vec3", "System.Numerics.Vector3"},
			{"vec4", "System.Numerics.Vector4"},
			{"dvec2", "dtmdl.NativeTypes.DVector2"},
			{"dvec3", "dtmdl.NativeTypes.DVector3"},
			{"dvec4", "dtmdl.NativeTypes.DVector4"},
			{"ivec2", "dtmdl.NativeTypes.IVector2"},
			{"ivec3", "dtmdl.NativeTypes.IVector3"},
			{"ivec4", "dtmdl.NativeTypes.IVector4"},
			{"uvec2", "dtmdl.NativeTypes.UVector2"},
			{"uvec3", "dtmdl.NativeTypes.UVector3"},
			{"uvec4", "dtmdl.NativeTypes.UVector4"},
			{"bvec2", "dtmdl.NativeTypes.BVector2"},
			{"bvec3", "dtmdl.NativeTypes.BVector3"},
			{"bvec4", "dtmdl.NativeTypes.BVector4"},
		};

		auto it = csharp_builtin_type_names.find(type->Name());
		if (it == csharp_builtin_type_names.end())
			throw std::runtime_error(format("unrecognized builtin type: {}", type->Name()));

		return it->second;
	}

	string CSharpFormatPlugin::FormatTemplateArgument(Database const& db, TemplateArgument const& arg)
	{
		if (auto it = get_if<uint64_t>(&arg))
			return to_string(*it);
		return FormatTypeReference(db, get<TypeReference>(arg));
	}
	
	string CSharpFormatPlugin::FormatNamespace(Database const& db)
	{
		return db.Schema().Namespace; /// TODO: This
	}

	SimpleOutputter CSharpFormatPlugin::StartOutput(Database const& db, vector<string_view> includes)
	{
		mOutString.clear();
		SimpleOutputter out{ mOutString };
		out.WriteLine("/// source_database: \"{}\"", string_ops::escaped(filesystem::absolute(db.Directory()).string(), "\"\\"));
		out.WriteLine("/// generated_time: \"{}\"", chrono::zoned_time{ chrono::current_zone(), chrono::system_clock::now() });

		out.WriteStart("namespace {} {{", db.Schema().Namespace);

		return out;
	}

	string CSharpFormatPlugin::FinishOutput(Database const& db)
	{
		SimpleOutputter out{ mOutString };

		out.WriteLine("}}");

		return move(mOutString).str();
	}


	string CSharpDeclarationFormat::Export(Database const& db)
	{
		auto out = StartOutput(db);

		for (auto def : db.UserDefinitions())
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


	void CSharpDeclarationFormat::WriteClass(SimpleOutputter& out, Database const& db, ClassDefinition const* klass)
	{
		if (klass->BaseType().Type)
			out.WriteStart("public class {}{} : {} {{", klass->Name(), (klass->Flags.contain(ClassFlags::Final) ? " final" : ""), FormatTypeReference(db, klass->BaseType()));
		else
			out.WriteStart("public class {}{} {{", klass->Name(), (klass->Flags.contain(ClassFlags::Final) ? " final" : ""));

		for (auto& field : klass->Fields())
		{
			out.WriteLine("{} {} {{}};", FormatTypeReference(db, field->FieldType), field->Name);
		}

		out.WriteEnd("}}");
	}
	void CSharpDeclarationFormat::WriteEnum(SimpleOutputter& out, Database const& db, EnumDefinition const* enoom)
	{
		out.WriteStart("public enum {} {{", enoom->Name());
		for (auto e : enoom->Enumerators())
		{
			out.WriteLine("{} = {},", e->Name, e->ActualValue());
		}
		out.WriteEnd("}}");
	}
	void CSharpDeclarationFormat::WriteStruct(SimpleOutputter& out, Database const& db, StructDefinition const* klass)
	{
		out.WriteStart("public record struct {} {{", klass->Name());

		/*
		if (klass->BaseType().Type)
		{

		}
		*/

		out.WriteEnd("}}");
	}
}