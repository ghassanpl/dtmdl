#pragma once

#include "Schema.h"

result<void, string> ValidateIdentifierName(string_view new_name);
result<void, string> ValidateTemplateArgument(TemplateArgument const& arg, TemplateParameter const& param);