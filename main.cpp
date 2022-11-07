#pragma once

#include <BML/BMLAll.h>
#include <thread>
#include "utils.hpp"


extern "C" {
  __declspec(dllexport) IMod* BMLEntry(IBML* bml);
}

class SysInfoDisplay : public IMod {
  std::atomic_bool init = false, info_visible = false;
  std::thread system_info_thread;
  BGui::Text *sprite = nullptr;
  bool display_battery = false, battery_same_line = false;


  const std::unordered_map<BYTE, std::string> ac_status = [] {
    // Stupid Windows. No emojis without Windows 10 && UTF-8
    bool display_emoji = (get_system_dpi != nullptr && GetACP() == CP_UTF8);
    return decltype(ac_status){
      {0, (display_emoji) ? ConvertWideToANSI(L"🔋 ") : "Not charging, "},
      {1, (display_emoji) ? ConvertWideToANSI(L"🔌 ") : "Charging, "},
      {255, "Unknown "},
    };
  }();
  IProperty *prop_enabled{}, *prop_battery{}, *prop_color{}, *prop_font_size{}, *prop_same_line{};
  CKDWORD text_color = 0xffffffff;
  int font_size{};

public:
  SysInfoDisplay(IBML* bml) : IMod(bml) {}

  virtual CKSTRING GetID() override { return "SysInfoDisplay"; }
  virtual CKSTRING GetVersion() override { return "0.0.1"; }
  virtual CKSTRING GetName() override { return "System Information Display"; }
  virtual CKSTRING GetAuthor() override { return "BallanceBug"; }
  virtual CKSTRING GetDescription() override { return "Display your system info ingame as an overlay."; }
  DECLARE_BML_VERSION;

  void OnExitGame() {
    hide_system_info();
    if (system_info_thread.joinable())
      system_info_thread.join();
  }

  void OnLoad() override {
    GetConfig()->SetCategoryComment("Main", "Main configurations.");
    prop_enabled = GetConfig()->GetProperty("Main", "Enabled");
    prop_enabled->SetDefaultBoolean(true);
    prop_enabled->SetComment("Whether or not to display your system info.");
    prop_battery = GetConfig()->GetProperty("Main", "DisplayBatteryStatus");
    prop_battery->SetDefaultBoolean(true);
    prop_battery->SetComment("Whether to display your battery status as part of the system info.");
    display_battery = prop_battery->GetBoolean();
    GetConfig()->SetCategoryComment("Text", "Text-related configurations.");
    prop_same_line = GetConfig()->GetProperty("Text", "BatterySameLine");
    prop_same_line->SetDefaultBoolean(false);
    prop_same_line->SetComment("Whether to display your battery status on the same line with the system time.");
    battery_same_line = prop_same_line->GetBoolean();
    prop_color = GetConfig()->GetProperty("Text", "TextColor");
    prop_color->SetDefaultString("FFFFFF");
    prop_color->SetComment("Color of the text for displaying the info.");
    parse_and_set_player_list_color(prop_color);
    prop_font_size = GetConfig()->GetProperty("Text", "FontSize");
    prop_font_size->SetDefaultInteger(11);
    prop_font_size->SetComment("Font size of the text.");
    font_size = get_display_font_size(m_bml->GetRenderContext()->GetHeight(), prop_font_size->GetInteger());
    // TODO: add position settings
  }

  void OnPostStartMenu() override {
    if (init)
      return;
    if (prop_enabled->GetBoolean())
      show_system_info();
    init = true;
  }

  void OnModifyConfig(CKSTRING category, CKSTRING key, IProperty* prop) {
    if (prop == prop_enabled) {
      if (prop_enabled->GetBoolean())
        show_system_info();
      else
        hide_system_info();
    }
    else if (prop == prop_battery) {
      display_battery = prop_battery->GetBoolean();
    }
    else if (prop == prop_same_line) {
      battery_same_line = prop_same_line->GetBoolean();
    }
    else if (prop == prop_color) {
      parse_and_set_player_list_color(prop);
    }
    else if (prop == prop_font_size) {
      font_size = get_display_font_size(m_bml->GetRenderContext()->GetHeight(), prop_font_size->GetInteger());
      if (sprite)
        sprite->SetFont("Segoe UI Symbol", font_size, 400, false, false);
    }
  }

private:
  void show_system_info() {
    if (info_visible)
      return;
    if (system_info_thread.joinable())
      system_info_thread.join();
    system_info_thread = std::thread([this] {
      my_ptr<std::remove_pointer_t<decltype(sprite)>> sprite_wrapper(&sprite, "SysInfo_Text");
      // sprite = sprite_wrapper.get();
      sprite->SetSize({0.4f, 0.2f});
      sprite->SetPosition({0.12f, 0.001f});
      sprite->SetAlignment(CKSPRITETEXT_LEFT);
      sprite->SetTextColor(text_color);
      sprite->SetZOrder(4096);
      sprite->SetFont("Segoe UI Symbol", font_size, 400, false, false);
      sprite->SetVisible(true);
      info_visible = true;
      int counter = 0;
      std::string power_text;
      while (info_visible) {
        auto current_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::string text(32, 0);
        text.resize(std::strftime(text.data(), text.size(), "%F %T", std::localtime(&current_time)));
        counter++;
        switch (counter) {
          case 5: {
            counter = 0;
            break;
          }
          case 1: {
            if (!display_battery) {
              power_text.clear();
              break;
            }
            SYSTEM_POWER_STATUS power_status;
            GetSystemPowerStatus(&power_status);
            power_text = (battery_same_line) ? " | " : "\n";
            power_text.reserve(64);
            power_text += ac_status.at(power_status.ACLineStatus);
            power_text += (power_status.BatteryLifePercent == 255) ? "unknown" : std::to_string(power_status.BatteryLifePercent) + "%";
            if (power_status.BatteryLifeTime != -1) {
              power_text += ", ";
              auto minutes = power_status.BatteryLifeTime / 60;
              auto hours = minutes / 60;
              minutes %= 60;
              if (hours != 0) {
                power_text += std::to_string(hours) + "h ";
              }
              power_text += std::to_string(minutes) + "min remaining";
            }
            break;
          }
          default: break;
        }
        sprite->SetText((text + power_text).c_str());
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
    });
  }

  void hide_system_info() { info_visible = false; }

  void parse_and_set_player_list_color(IProperty* prop) {
    CKDWORD temp_color = 0xFFFFFFFF;
    try {
      temp_color = (CKDWORD)std::stoul(prop->GetString(), nullptr, 16);
    }
    catch (const std::exception& e) {
      GetLogger()->Warn("Error parsing the color code: %s. Resetting to %06X.", e.what(), temp_color & 0x00FFFFFF);
      char color_text[8]{};
      snprintf(color_text, sizeof(color_text), "%06X", temp_color & 0x00FFFFFF);
      prop->SetString(color_text);
    }
    if (text_color == temp_color) return;
    text_color = temp_color | 0xFF000000;
    char color_text[8]{};
    snprintf(color_text, sizeof(color_text), "%06X", temp_color & 0x00FFFFFF);
    prop->SetString(color_text);
    if (sprite)
      sprite->SetTextColor(text_color);
  }
};

IMod* BMLEntry(IBML* bml) {
  return new SysInfoDisplay(bml);
}
