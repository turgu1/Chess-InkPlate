// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __OPTION_CONTROLLER__ 1
#include "controllers/option_controller.hpp"

#include "controllers/common_actions.hpp"
#include "controllers/app_controller.hpp"
#include "controllers/board_controller.hpp"
#include "viewers/menu_viewer.hpp"
#include "viewers/msg_viewer.hpp"
#include "viewers/form_viewer.hpp"
#include "models/config.hpp"
#include "models/fonts.hpp"

#if CHESS_INKPLATE_BUILD
  #include "esp_system.h"
#endif

// static int8_t boolean_value;

static Screen::Orientation     orientation;
static Screen::PixelResolution  resolution;
static int8_t show_battery;
static int8_t timeout;
static int8_t chess_font;
static int8_t show_heap;
static int8_t engine_time;
// static int8_t ok;

static Screen::Orientation     old_orientation;
static Screen::PixelResolution  old_resolution;
static int8_t old_chess_font;
static int8_t old_engine_time;

static constexpr int8_t MAIN_FORM_SIZE = 5;
static FormViewer::FormEntry main_params_form_entries[MAIN_FORM_SIZE] = {
  { "Minutes Before Sleeping :",  &timeout,                3, FormViewer::timeout_choices,     FormViewer::FormEntryType::HORIZONTAL_CHOICES },
  { "Buttons Position :",         (int8_t *) &orientation, 3, FormViewer::orientation_choices, FormViewer::FormEntryType::VERTICAL_CHOICES   },
  { "Pixel Resolution :",         (int8_t *) &resolution,  2, FormViewer::resolution_choices,  FormViewer::FormEntryType::HORIZONTAL_CHOICES },
  { "Show Battery Level :",       &show_battery,           4, FormViewer::battery_visual,      FormViewer::FormEntryType::VERTICAL_CHOICES   },
  { "Show Heap Size :",           &show_heap,              2, FormViewer::yes_no_choices,      FormViewer::FormEntryType::HORIZONTAL_CHOICES }
};

static constexpr int8_t FONT_FORM_SIZE = 2;
static FormViewer::FormEntry chess_params_form_entries[FONT_FORM_SIZE] = {
  { "Engine Work Duration :", &engine_time,        4, FormViewer::engine_time_choices, FormViewer::FormEntryType::HORIZONTAL_CHOICES },
  { "Chess Font :",           &chess_font,         7, FormViewer::font_choices,        FormViewer::FormEntryType::VERTICAL_CHOICES   }
};

extern bool start_web_server();
extern bool  stop_web_server();

static void
main_parameters()
{
  config.get(Config::Ident::ORIENTATION,      (int8_t *) &orientation);
  config.get(Config::Ident::PIXEL_RESOLUTION, (int8_t *) &resolution );
  config.get(Config::Ident::BATTERY,          &show_battery          );
  config.get(Config::Ident::SHOW_HEAP,        &show_heap             );
  config.get(Config::Ident::TIMEOUT,          &timeout               );

  old_orientation = orientation;
  old_resolution  = resolution;
  // ok              = 0;

  form_viewer.show(
    main_params_form_entries, 
    MAIN_FORM_SIZE, 
    "");

  option_controller.set_main_form_is_shown();
}

static void
chess_parameters()
{
  config.get(Config::Ident::ENGINE_TIME,  &engine_time);
  config.get(Config::Ident::DEFAULT_FONT, &chess_font );
  
  old_chess_font         = chess_font;
  old_engine_time        = engine_time;
  // ok                     = 0;

  form_viewer.show(
    chess_params_form_entries, 
    FONT_FORM_SIZE, 
    "");

  option_controller.set_chess_form_is_shown();
}

static void
wifi_mode()
{
  #if CHESS_INKPLATE_BUILD  
    fonts.clear();
    fonts.clear_glyph_caches();
    
    event_mgr.set_stay_on(true); // DO NOT sleep

    if (start_web_server()) {
      option_controller.set_wait_for_key_after_wifi();
    }
  #endif
}

void
new_game_play_white()
{
  board_controller.new_game(true);
  app_controller.set_controller(AppController::Ctrl::LAST);
}

void
new_game_play_black()
{
  board_controller.new_game(false);
  app_controller.set_controller(AppController::Ctrl::LAST);
}

static MenuViewer::MenuEntry menu[8] = {
  { MenuViewer::Icon::RETURN,      "Return to the chess board",            CommonActions::return_to_last},
  { MenuViewer::Icon::W_KNIGHT,    "New game, play white",                 new_game_play_white          },
  { MenuViewer::Icon::B_KNIGHT,    "New game, play black",                 new_game_play_black          },
  { MenuViewer::Icon::MAIN_PARAMS, "Main parameters",                      main_parameters              },
  { MenuViewer::Icon::CHESS,       "Chess parameters",                     chess_parameters             },
//{ MenuViewer::Icon::WIFI,        "WiFi Access to the games folder",      wifi_mode                     },
  { MenuViewer::Icon::INFO,        "About the Chess-InkPlate application", CommonActions::about         },
  { MenuViewer::Icon::POWEROFF,    "Power OFF (Deep Sleep)",               CommonActions::power_off     },
  { MenuViewer::Icon::END_MENU,     nullptr,                               nullptr                      }
};

void 
OptionController::enter()
{
  menu_viewer.show(menu);
  main_form_is_shown  = false;
  chess_form_is_shown = false;
}

void 
OptionController::leave(bool going_to_deep_sleep)
{
  if (going_to_deep_sleep) board_controller.save();
}

void 
OptionController::key_event(EventMgr::KeyEvent key)
{
  if (main_form_is_shown) {
    if (form_viewer.event(key)) {
      main_form_is_shown = false;
      // if (ok) {
        config.put(Config::Ident::ORIENTATION,      (int8_t) orientation);
        config.put(Config::Ident::PIXEL_RESOLUTION, (int8_t) resolution );
        config.put(Config::Ident::BATTERY,          show_battery        );
        config.put(Config::Ident::SHOW_HEAP,        show_heap           );
        config.put(Config::Ident::TIMEOUT,          timeout             );
        config.save();

        if (old_resolution != resolution) {
          fonts.clear_glyph_caches();
          screen.set_pixel_resolution(resolution);
        }

        if ((old_orientation != orientation) || 
            (old_resolution  != resolution )) {
          menu_viewer.show(menu, 2, true);
        }
      // }
    }
  }
  else if (chess_form_is_shown) {
    if (form_viewer.event(key)) {
      chess_form_is_shown = false;
      // if (ok) {
        config.put(Config::Ident::ENGINE_TIME,  engine_time);
        config.put(Config::Ident::DEFAULT_FONT, chess_font );
        config.save();

        if (old_chess_font  != chess_font ) fonts.setup();
        if (old_engine_time != engine_time) chess_engine.set_engine_time(15 * engine_time);
      // }
    }
  }
  #if CHESS_INKPLATE_BUILD
    else if (wait_for_key_after_wifi) {
      msg_viewer.show(MsgViewer::INFO, 
                      false, true, 
                      "Restarting", 
                      "The device is now restarting. Please wait.");
      wait_for_key_after_wifi = false;
      stop_web_server();
      esp_restart();
    }
  #endif
  else {
    if (menu_viewer.event(key)) {
      app_controller.set_controller(AppController::Ctrl::LAST);
    }
  }
}
