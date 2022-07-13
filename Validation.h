#pragma once

#include "Schema.h"

struct TypeReference;

result<void, string> ValidateIdentifierName(string_view new_name);
//result<void, string> ValidateTemplateArgument(TemplateArgument const& arg, TemplateParameter const& param);
result<void, string> ValidateType(TypeReference const& type);
result<void, string> ValidateFieldType(FieldDefinition const* def, TypeReference const& type);

result<void, string> ValidateTypeDefinition(TypeDefinition const* type, TemplateParameterQualifier qualifier);
bool MatchesQualifier(TypeDefinition const* type, TemplateParameterQualifier qualifier);
bool MatchesQualifier(TypeReference const& type, TemplateParameterQualifier qualifier);