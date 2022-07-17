#pragma once

#include <vector>
#include <variant>
#include <cstdint>
#include <string>

namespace dtmdl
{
	enum class DefinitionType
	{
		BuiltIn,
		Enum,
		Struct,
		Class,
		Union,
		Alias,

		Attribute,
	};

	struct TypeDefinition;
	struct Schema;

	struct TypeReference
	{
		TypeDefinition const* Type = nullptr;
		::std::vector<::std::variant<::std::uint64_t, TypeReference>> TemplateArguments;

		TypeReference() noexcept = default;
		TypeReference(TypeReference const&) noexcept = default;
		TypeReference(TypeReference&&) noexcept = default;
		TypeReference& operator=(TypeReference const&) noexcept = default;
		TypeReference& operator=(TypeReference&&) noexcept = default;

		//TypeReference(Schema const& schema, json const& val);

		bool operator==(TypeReference const& other) const noexcept = default;

		explicit TypeReference(TypeDefinition const* value) noexcept;
		explicit TypeReference(TypeDefinition const* value, ::std::vector<::std::variant<::std::uint64_t, TypeReference>> args);
		
		TypeDefinition const* operator->() const { return Type; }

		::std::string ToString() const;

		explicit operator bool() const noexcept { return Type != nullptr; }

		auto OnlyTypeReferenceArguments() const
		{
			return TemplateArguments 
				| views::filter([](auto& arg) { return arg.index() == 1; })
				| views::transform([](auto const& arg) -> TypeReference const& { return ::std::get<TypeReference>(arg); });
		}

		/*
		json ToJSON() const;
		void FromJSON(Schema const& schema, json const& value);
		void CalculateDependencies(set<TypeDefinition const*>& dependencies) const;
		*/

		/// TODO: When C++23's ranges/zip comes along:
		/// auto TemplatePairs() { return views::zip(Type->TemplateParameters(), TemplateArguments); }
	};

	using TemplateArgument = ::std::variant<::std::uint64_t, TypeReference>;


	enum class StructFlags
	{
		CreateTableType,
	};

	enum class ClassFlags
	{
		Abstract,
		Final,
		CreateIsAs,
	};

	enum class FieldFlags
	{
		Private,
		Transient,
		Getter,
		Setter,
		Unique,
		Indexed,

		NoEdit,
		NoView,
		NoDebug,
		NoClone,
		NoSerialize,
		NoDeserialize,
	};

}