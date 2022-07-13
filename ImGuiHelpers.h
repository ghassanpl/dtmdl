#pragma once

namespace ImGui
{
	bool ComboWithFilter(const char* label, int* current_item, const std::vector<std::string>& items);
	bool ToggleButton(const char* label, bool& option, const ImVec2& size = ImVec2(0, 0), ImGuiButtonFlags flags = ImGuiButtonFlags_None);
	bool ToggleButtonFlags(const char* label, unsigned& flags, unsigned flag_value, const ImVec2& size = ImVec2(0, 0), ImGuiButtonFlags button_flags = ImGuiButtonFlags_None);
	bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f);
}