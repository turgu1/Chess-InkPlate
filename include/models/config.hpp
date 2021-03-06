// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "config_template.hpp"

enum class ConfigIdent { 
  VERSION, SSID, PWD, PORT, BATTERY, TIMEOUT, 
  DEFAULT_FONT, PIXEL_RESOLUTION, SHOW_HEAP, ENGINE_TIME
};

typedef ConfigBase<ConfigIdent, 10> Config;

#if __CONFIG__
  #include <string>

  static const std::string CONFIG_FILE = MAIN_FOLDER "/config.txt";

  static int8_t   version;

  static char     ssid[32];
  static char     pwd[32];
  static int32_t  port;
  static int8_t   engine_time;
  static int8_t   battery;
  static int8_t   timeout;
  static int8_t   default_font;
  static int8_t   resolution;
  static int8_t   show_heap;

  static int32_t  default_port               = 80;
  static int8_t   default_engine_time        =  4;  // in multiple of 15 seconds
  static int8_t   default_battery            =  2;  // 0 = NONE, 1 = PERCENT, 2 = VOLTAGE, 3 = ICON
  static int8_t   default_timeout            = 15;  // 5, 15, 30 minutes
  static int8_t   default_default_font       =  1;  // 0 = CALADEA, 1 = CRIMSON, 2 = RED HAT, 3 = ASAP
  static int8_t   default_resolution         =  0;  // 0 = 1bit, 1 = 3bits
  static int8_t   default_show_heap          =  0;  // 0 = NO, 1 = YES
  static int8_t   the_version                =  1;

  // static Config::CfgType conf = {{

  template <>
  Config::CfgType Config::cfg = {{
    { Config::Ident::VERSION,            Config::EntryType::BYTE,   "version",            &version,            &the_version,                0 },
    { Config::Ident::SSID,               Config::EntryType::STRING, "wifi_ssid",          ssid,                "NONE",                     32 },
    { Config::Ident::PWD,                Config::EntryType::STRING, "wifi_pwd",           pwd,                 "NONE",                     32 },
    { Config::Ident::PORT,               Config::EntryType::INT,    "http_port",          &port,               &default_port,               0 },
    { Config::Ident::BATTERY,            Config::EntryType::BYTE,   "battery",            &battery,            &default_battery,            0 },
    { Config::Ident::TIMEOUT,            Config::EntryType::BYTE,   "timeout",            &timeout,            &default_timeout,            0 },
    { Config::Ident::DEFAULT_FONT,       Config::EntryType::BYTE,   "default_font",       &default_font,       &default_default_font,       0 },
    { Config::Ident::PIXEL_RESOLUTION,   Config::EntryType::BYTE,   "resolution",         &resolution,         &default_resolution,         0 },
    { Config::Ident::SHOW_HEAP,          Config::EntryType::BYTE,   "show_heap",          &show_heap,          &default_show_heap,          0 },
    { Config::Ident::ENGINE_TIME,        Config::EntryType::BYTE,   "engine_time",        &engine_time,        &default_engine_time,        0 },
  }};

  // Config config(conf, CONFIG_FILE);
  Config config(CONFIG_FILE, true);
#else
  extern Config config;
#endif
