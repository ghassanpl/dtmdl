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

#include "codicons_font.h"

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

namespace ghassanpl
{
	template<typename T> void to_json(json& j, enum_flags<T> const& v) { j = json::array(); v.for_each([&j](auto v) { j.push_back(magic_enum::enum_name(v)); }); }
	template<typename T> void from_json(json const& j, enum_flags<T>& v) { for (auto& f : j) v.set(magic_enum::enum_cast<T>((string_view)f).value()); }
}