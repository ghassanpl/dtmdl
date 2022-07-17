#include "pch.h"
#include "CppReflectionFormat.h"
#include "Schema.h"
#include "Database.h"
#include "Validation.h"

namespace dtmdl
{

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
}