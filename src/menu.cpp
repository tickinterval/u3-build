#include "menu.h"
#include "imgui.h"
#include <windows.h>
#include <string>

namespace menu {

namespace {

ImFont* g_font_main = nullptr;
ImFont* g_font_title = nullptr;

// Main tab
static bool aimbot = false;
static bool wallhack_realistic_main = false;
static bool tablet_aimbot = false;
static bool tablet_mode = false;
static bool always_play_against_tablets = false;
static bool always_highlight_armor = false;
static bool tundra = false;
static int tundra_key = 0x58; // X key
static bool binding_tundra = false;
static int spread_hack = 0; // 0=Off, 1=Partial, 2=Full
static bool remove_collision = false;

// Visual tab
static bool wallhack_realistic_visual = false;
static bool wallhack_outline = false;
static bool big_tracers = false;
static ImVec4 big_tracers_color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
static bool accurate_tracers = false;
static int accurate_tracers_value = 10;
static bool destroyed_objects = false;
static bool lasers = false;
static bool overlay = false;

// Misc tab
static int training_room_minutes = 60;
static int cluster_select = 0; // 0=C0, 1=C1, 2=C2, 3=C3, 4=C4
static bool maps_selected[16] = {false};
static bool maps_dropdown_open = false;

ImU32 ColorU32(float r, float g, float b, float a = 1.0f) {
    return ImGui::GetColorU32(ImVec4(r, g, b, a));
}

bool SidebarTab(const char* label, int id, int* active_tab) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float height = 34.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size(ImGui::GetContentRegionAvail().x, height);
    ImVec2 rect_max(pos.x + size.x, pos.y + size.y);

    ImGui::PushID(id);
    ImGui::InvisibleButton("tab", size);
    ImGui::PopID();

    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    const bool active = (*active_tab == id);

    ImU32 fill = active ? ColorU32(0.18f, 0.19f, 0.26f, 1.0f)
                        : (hovered ? ColorU32(0.14f, 0.15f, 0.20f, 1.0f) : ColorU32(0.10f, 0.11f, 0.14f, 1.0f));
    ImU32 border = ColorU32(0.20f, 0.22f, 0.30f, 1.0f);
    ImU32 text = ColorU32(0.92f, 0.94f, 0.98f, 1.0f);
    ImU32 icon = active ? ColorU32(0.26f, 0.75f, 0.65f, 1.0f) : ColorU32(0.55f, 0.60f, 0.70f, 1.0f);

    draw->AddRectFilled(pos, rect_max, fill, 6.0f);
    draw->AddRect(pos, rect_max, border, 6.0f);
    draw->AddRectFilled(ImVec2(pos.x + 10.0f, pos.y + 10.0f),
                        ImVec2(pos.x + 20.0f, pos.y + 20.0f),
                        icon, 2.0f);
    draw->AddText(ImVec2(pos.x + 28.0f, pos.y + 8.0f), text, label);

    if (clicked) {
        *active_tab = id;
    }
    return clicked;
}

const char* GetKeyName(int vk) {
    static char buf[32];
    if (vk == 0) return "None";
    if (vk >= 0x30 && vk <= 0x5A) {
        buf[0] = (char)vk;
        buf[1] = '\0';
        return buf;
    }
    switch (vk) {
        case VK_SHIFT: return "SHIFT";
        case VK_CONTROL: return "CTRL";
        case VK_MENU: return "ALT";
        case VK_SPACE: return "SPACE";
        case VK_TAB: return "TAB";
        default: 
            _snprintf_s(buf, sizeof(buf), "Key %d", vk);
            return buf;
    }
}

} // namespace

void InitStyle() {
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    ImFontConfig font_config = {};
    font_config.OversampleH = 3;
    font_config.OversampleV = 2;
    g_font_main = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdana.ttf", 15.0f, &font_config);
    g_font_title = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdana.ttf", 16.0f, &font_config);
    if (!g_font_main) {
        g_font_main = io.Fonts->AddFontDefault();
        g_font_title = g_font_main;
    }

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(10.0f, 6.0f);

    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.06f, 0.08f, 0.95f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.18f, 0.20f, 0.28f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.13f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16f, 0.18f, 0.24f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.20f, 0.28f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.75f, 0.65f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.26f, 0.75f, 0.65f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.32f, 0.82f, 0.70f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.18f, 0.20f, 0.28f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.26f, 0.34f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.30f, 0.38f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.14f, 0.16f, 0.22f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.24f, 0.32f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.24f, 0.28f, 0.36f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.92f, 0.94f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.60f, 0.70f, 1.0f);
}

void Render() {
    static int active_tab = 0;

    ImGui::SetNextWindowSize(ImVec2(560, 480), ImGuiCond_FirstUseEver);
    if (g_font_main) {
        ImGui::PushFont(g_font_main);
    }
    if (ImGui::Begin("u3ware", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                          ImGuiWindowFlags_NoCollapse)) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();
        draw->AddRect(win_pos, ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y),
                      ColorU32(0.22f, 0.24f, 0.32f, 1.0f), 8.0f);

        const float sidebar_width = 120.0f;
        ImGui::BeginChild("sidebar", ImVec2(sidebar_width, 0.0f), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetCursorPosY(8.0f);
        SidebarTab("Main", 0, &active_tab);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
        SidebarTab("Visual", 1, &active_tab);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
        SidebarTab("Info", 2, &active_tab);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
        SidebarTab("Misc", 3, &active_tab);
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("content", ImVec2(0.0f, 0.0f), false);
        if (g_font_title) {
            ImGui::PushFont(g_font_title);
        }
        if (active_tab == 0) {
            ImGui::TextUnformatted("Main");
        } else if (active_tab == 1) {
            ImGui::TextUnformatted("Visual");
        } else if (active_tab == 2) {
            ImGui::TextUnformatted("Info");
        } else {
            ImGui::TextUnformatted("Misc");
        }
        if (g_font_title) {
            ImGui::PopFont();
        }
        ImGui::Separator();
        ImGui::Spacing();

        // ========== MAIN TAB ==========
        if (active_tab == 0) {
            ImGui::Checkbox("Aimbot", &aimbot);
            ImGui::Checkbox("Wallhack (realistic mode)", &wallhack_realistic_main);
            ImGui::Checkbox("Tablet aimbot", &tablet_aimbot);
            ImGui::Checkbox("Tablet mode", &tablet_mode);
            
            // Always play against phones/tablets (only if tablet mode enabled)
            if (!tablet_mode) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            }
            ImGui::Checkbox("Always play against phones\\tablets", &always_play_against_tablets);
            if (!tablet_mode) {
                ImGui::PopStyleVar();
                if (!tablet_mode) {
                    always_play_against_tablets = false;
                }
            }
            
            ImGui::Checkbox("Always highlight armor", &always_highlight_armor);
            
            // Tundra with keybind
            ImGui::Checkbox("Tundra", &tundra);
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
            char keybind_text[64];
            _snprintf_s(keybind_text, sizeof(keybind_text), "[%s]", GetKeyName(tundra_key));
            if (ImGui::Button(keybind_text, ImVec2(80, 0))) {
                binding_tundra = true;
            }
            if (binding_tundra) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Press any key...");
                for (int vk = 0x08; vk <= 0x5A; ++vk) {
                    if (GetAsyncKeyState(vk) & 0x8000) {
                        tundra_key = vk;
                        binding_tundra = false;
                        break;
                    }
                }
            }
            
            // Spread hack combo
            const char* spread_modes[] = { "Off", "Partial", "Full" };
            ImGui::Combo("Spread hack", &spread_hack, spread_modes, IM_ARRAYSIZE(spread_modes));
            
            // Remove collision with tooltip
            ImGui::Checkbox("Remove collision", &remove_collision);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Aim thru objects");
            }
        }
        // ========== VISUAL TAB ==========
        else if (active_tab == 1) {
            ImGui::Checkbox("Wallhack (realistic mode)", &wallhack_realistic_visual);
            ImGui::Checkbox("Wallhack outline", &wallhack_outline);
            
            // Big tracers with color picker
            ImGui::Checkbox("Big tracers", &big_tracers);
            if (big_tracers) {
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
                ImGui::ColorEdit3("##big_tracers_color", reinterpret_cast<float*>(&big_tracers_color), 
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            }
            
            // Accurate tracers (only if big tracers enabled)
            if (!big_tracers) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
            }
            ImGui::Checkbox("Accurate tracers", &accurate_tracers);
            if (!big_tracers) {
                ImGui::PopStyleVar();
                accurate_tracers = false;
            }
            
            // Accurate tracers slider (only if accurate tracers enabled)
            if (accurate_tracers && big_tracers) {
                ImGui::Indent();
                ImGui::SliderInt("##accurate_tracers_value", &accurate_tracers_value, 3, 20, "Value: %d");
                ImGui::Unindent();
            }
            
            ImGui::Checkbox("Destroyed objects", &destroyed_objects);
            ImGui::Checkbox("Lasers", &lasers);
            ImGui::Checkbox("Overlay", &overlay);
        }
        // ========== INFO TAB ==========
        else if (active_tab == 2) {
            ImGui::TextUnformatted("u3ware overlay");
            ImGui::TextDisabled("Build 1.1.0");
            ImGui::Separator();
            ImGui::TextUnformatted("Status:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.26f, 0.75f, 0.65f, 1.0f), "Online");
            ImGui::Spacing();
            ImGui::TextDisabled("Internal build");
            ImGui::TextDisabled("@u3ware");
        }
        // ========== MISC TAB ==========
        else if (active_tab == 3) {
            // Training room slider
            ImGui::SliderInt("Training room", &training_room_minutes, 1, 467, "%d min");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Change number before creating room");
            }
            
            // Cluster select
            const char* clusters[] = { "C0", "C1", "C2", "C3", "C4" };
            ImGui::Combo("Cluster select", &cluster_select, clusters, IM_ARRAYSIZE(clusters));
            
            // Maps multi-select dropdown
            ImGui::TextUnformatted("Maps");
            ImGui::Indent();
            
            int selected_count = 0;
            for (int i = 0; i < 16; ++i) {
                if (maps_selected[i]) selected_count++;
            }
            
            char preview_text[128];
            if (selected_count == 0) {
                _snprintf_s(preview_text, sizeof(preview_text), "Select maps...");
            } else {
                _snprintf_s(preview_text, sizeof(preview_text), "%d maps selected", selected_count);
            }
            
            if (ImGui::Button(preview_text, ImVec2(200, 0))) {
                maps_dropdown_open = !maps_dropdown_open;
            }
            
            if (maps_dropdown_open) {
                ImGui::BeginChild("maps_list", ImVec2(200, 150), true);
                for (int i = 0; i < 16; ++i) {
                    char map_label[16];
                    _snprintf_s(map_label, sizeof(map_label), "A%d", i + 1);
                    ImGui::Checkbox(map_label, &maps_selected[i]);
                }
                ImGui::EndChild();
            }
            
            ImGui::Unindent();
        }

        ImGui::EndChild();
    }
    ImGui::End();
    if (g_font_main) {
        ImGui::PopFont();
    }
}

} // namespace menu
