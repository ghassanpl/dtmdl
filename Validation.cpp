#include "pch.h"
#include "Validation.h"
#include "Database.h"

static string_view cpp_keywords[] = {
	"alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char8_t",
	"char16_t", "char32_t", "class", "compl", "concept", "const", "consteval", "constexpr", "constinit", "const_cast", "continue",
	"co_await", "co_return", "co_yield", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit",
	"export", "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long", "mutable", "namespace", "new",
	"noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public", "reflexpr", "register",
	"reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
	"switch", "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned",
	"using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
};

result<void, string> ValidateIdentifierName(string_view new_name)
{
	if (new_name.empty())
		return failure("name cannot be empty");
	if (!string_ops::ascii::isalpha(new_name[0]) && new_name[0] != '_')
		return failure("name must start with a letter or underscore");
	if (!ranges::all_of(new_name, string_ops::ascii::isident))
		return failure("name must contain only letters, numbers, or underscores");
	if (ranges::find(cpp_keywords, new_name) != ranges::end(cpp_keywords))
		return failure("name cannot be a C++ keyword");
	if (new_name.find("__") != string::npos)
		return failure("name cannot contain two consecutive underscores (__)");
	if (new_name.size() >= 2 && new_name[0] == '_' && string_ops::ascii::isupper(new_name[1]))
		return failure("name cannot begin with an underscore followed by a capital letter (_X)");
	return success();
}

result<void, string> ValidateTemplateArgument(TemplateArgument const& arg, TemplateParameter const& param)
{
	if (param.Qualifier == TemplateParameterQualifier::Size)
	{
		if (holds_alternative<uint64_t>(arg))
			return success();
		return failure("template argument must be a size");
	}

	if (!holds_alternative<TypeReference>(arg))
		return failure("template argument must be a type");

	auto& arg_type = get<TypeReference>(arg);

	if (!arg_type.Type)
		return failure("type must be set");

	switch (param.Qualifier)
	{
	case TemplateParameterQualifier::AnyType: break;
	case TemplateParameterQualifier::Struct:
		if (arg_type->Type() != DefinitionType::Struct)
			return "must be a struct type";
		break;
	case TemplateParameterQualifier::NotClass:
		if (arg_type->Type() == DefinitionType::Class)
			return "must be a non-class type";
		break;
	case TemplateParameterQualifier::Enum:
		if (arg_type->Type() != DefinitionType::Enum)
			return "must be an enum type";
		break;
	case TemplateParameterQualifier::Integral:
	case TemplateParameterQualifier::Floating:
	case TemplateParameterQualifier::Pointer:
		if (arg_type->Type() != DefinitionType::BuiltIn)
			return "must be an integral type";
		else
		{
			auto builtin_type = dynamic_cast<BuiltinDefinition const*>(arg_type.Type);
			if (builtin_type && builtin_type->ApplicableQualifiers().is_set(param.Qualifier) == false)
				return format("must be a {} type", string_ops::ascii::tolower(magic_enum::enum_name(param.Qualifier)));
		}
		break;
	case TemplateParameterQualifier::Class:
		if (arg_type->Type() != DefinitionType::Class)
			return "must be a class type";
		break;
	default:
		throw format("internal error: unimplemented template parameter qualifier `{}`", magic_enum::enum_name(param.Qualifier));
	}

	return success();
}

result<void, string> Database::ValidateTypeName(Def def, string const& new_name)
{
	if (auto result = ValidateIdentifierName(new_name); result.has_error())
		return result;

	auto type = mSchema.ResolveType(new_name);
	if (!type)
		return success();
	if (type == def)
		return success();
	return failure("a type with that name already exists");
}

result<void, string> Database::ValidateFieldName(Fld def, string const& new_name)
{
	if (auto result = ValidateIdentifierName(new_name); result.has_error())
		return result;

	auto rec = def->ParentRecord;
	for (auto& field : rec->mFields)
	{
		if (field.get() == def)
			continue;
		if (field->Name == new_name)
			return failure("a field with that name already exists");
	}
	return success();
}

result<void, string> Database::ValidateRecordBaseType(Rec def, TypeReference const& type)
{
	if (IsParent(def, type.Type))
		return failure("cycle in base types");

	if (type.Type)
	{
		auto record_field_names = def->OwnFieldNames();
		auto base_field_names = type.Type->AsRecord()->AllFieldNames();

		vector<string> shadowed_names;
		ranges::set_intersection(record_field_names, base_field_names, back_inserter(shadowed_names));
		if (!shadowed_names.empty())
		{
			vector<string> shadows;
			for (auto& name : shadowed_names)
				shadows.push_back(format("- {0}.{1} would hide {2}.{1}", def->Name(), name, type.Type->AsRecord()->OwnOrBaseField(name)->ParentRecord->Name()));
			return failure(format("the following fields would hide base fields:\n{}", string_ops::join(shadows, "\n")));
		}
	}

	return success();
}


result<void, string> Database::ValidateFieldType(Fld def, TypeReference const& type)
{
	if (!type.Type)
		return failure("field type is invalid");

	//if (type.Type->Name() == "void") return failure("field type cannot be void");

	set<TypeDefinition const*> open_types;
	open_types.insert(def->ParentRecord);
	auto is_open = [&](TypeDefinition const* type) -> TypeDefinition const* {
		/// If the type itself is open
		if (open_types.contains(type)) return type;

		/// If any base type is open
		auto parent_type = type->BaseType().Type;
		while (parent_type && parent_type->IsRecord())
		{
			if (open_types.contains(parent_type))
			{
				/// For performance, insert all types up to the open base type into the open set
				for (auto t = type; t != parent_type; t = t->BaseType().Type)
					open_types.insert(t);
				return parent_type;
			}
			parent_type = type->BaseType().Type;
		}
		return nullptr;
	};

	auto check_type = [](this auto& check_type, TypeReference const& type, auto& is_open) -> result<void, string> {
		if (auto incomplete = is_open(type.Type))
			return failure(format("{}: type is dependent on an incomplete type: {}", type.ToString(), incomplete->Name()));

		if (type.TemplateArguments.size() < type.Type->TemplateParameters().size())
			return failure(format("{}: invalid number of template arguments given", type.ToString()));

		for (size_t i = 0; i < type.TemplateArguments.size(); ++i)
		{
			auto& arg = type.TemplateArguments[i];
			auto& param = type.Type->TemplateParameters().at(i);
			auto result = ValidateTemplateArgument(arg, param);
			if (result.has_failure())
				return failure(format("{}: in template argument {}:\n{}", type.ToString(), i, move(result).error()));
			if (param.MustBeComplete())
			{
				if (auto ref = get_if<TypeReference>(&arg))
				{
					if (result = check_type(*ref, is_open); result.has_failure())
						return failure(format("{}: in template argument {}:\n{}", type.ToString(), i, move(result).error()));
				}
			}
		}
		return success();
	};

	return check_type(type, is_open);
}

