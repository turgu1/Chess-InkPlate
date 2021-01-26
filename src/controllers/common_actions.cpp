// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __COMMON_ACTIONS__ 1
#include "controllers/common_actions.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/game_controller.hpp"
#include "controllers/event_mgr.hpp"
#include "viewers/msg_viewer.hpp"

#if CHESS_INKPLATE_BUILD
  #include "inkplate_platform.hpp"
  #include "esp.hpp"
#endif

void
CommonActions::return_to_last()
{
  app_controller.set_controller(AppController::Ctrl::LAST);
}

void
CommonActions::power_off()
{
  app_controller.going_to_deep_sleep();
  #if CHESS_INKPLATE_BUILD
    msg_viewer.show(MsgViewer::INFO, false, true, "Power OFF",
      "Entering Deep Sleep mode. Please press a key to restart the device.");
    ESP::delay(500);
    inkplate_platform.deep_sleep();
  #else
    exit(0);
  #endif
}

void
CommonActions::about()
{
  msg_viewer.show(
    MsgViewer::BOOK, 
    false,
    false,
    "About Chess-InkPlate", 
    "A Simple Chess Player for the InkPlate devices Version %s for the InkPlate e-paper display devices. "
    "This application was made by Guy Turcotte, Quebec, QC, Canada, "
    "with great support from e-Radionica.",
    APP_VERSION);
}
