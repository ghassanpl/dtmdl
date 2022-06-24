#pragma once

#include <set>
#include <variant>
#include <array>
#include <filesystem>
#include <functional>
#include <format>

#include <outcome.hpp>
#include <nlohmann/json.hpp>
#include <magic_enum.hpp>
#include <ghassanpl/enum_flags.h>
#include <ghassanpl/string_ops.h>
#include <ghassanpl/json_helpers.h>
#include <ghassanpl/assuming.h>

using namespace outcome_v2_35644f5c;

using namespace std;
using namespace ghassanpl;
using nlohmann::json;

template <typename FUNC>
string FreshName(string_view base, FUNC&& func)
{
	string candidate = string{ base };
	size_t num = 1;
	while (func(candidate))
		candidate = format("{}{}", base, num++);
	return candidate;
}

void CheckError(result<void, string> val, string else_string = {});