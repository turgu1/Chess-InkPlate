// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __PROMOTION_CONTROLLER__ 1
#include "controllers/promotion_controller.hpp"

#include "controllers/app_controller.hpp"
#include "controllers/board_controller.hpp"
#include "viewers/menu_viewer.hpp"
#include "models/fonts.hpp"
#include "chess_engine_types.hpp"

#if CHESS_INKPLATE_BUILD
  #include "esp_system.h"
#endif

static void
promote_to_queen()
{
  board_controller.set_promotion_to(MoveType::PROMOTE_TO_QUEEN);
  app_controller.set_controller(AppController::Ctrl::LAST);
}

static void
promote_to_rook()
{
  board_controller.set_promotion_to(MoveType::PROMOTE_TO_ROOK);
  app_controller.set_controller(AppController::Ctrl::LAST);
}

static void
promote_to_bishop()
{
  board_controller.set_promotion_to(MoveType::PROMOTE_TO_BISHOP);
  app_controller.set_controller(AppController::Ctrl::LAST);
}

static void
promote_to_knight()
{
  board_controller.set_promotion_to(MoveType::PROMOTE_TO_KNIGHT);
  app_controller.set_controller(AppController::Ctrl::LAST);
}

static MenuViewer::MenuEntry white_menu[5] = {
  { MenuViewer::Icon::W_QUEEN,     "Promote to Queen",  promote_to_queen  },
  { MenuViewer::Icon::W_ROOK,      "Promote to Rook",   promote_to_rook   },
  { MenuViewer::Icon::W_BISHOP,    "Promote to Bishop", promote_to_bishop },
  { MenuViewer::Icon::W_KNIGHT,    "Promote to Knight", promote_to_knight },
  { MenuViewer::Icon::END_MENU,     nullptr,            nullptr           }
};

static MenuViewer::MenuEntry black_menu[5] = {
  { MenuViewer::Icon::B_QUEEN,     "Promote to Queen",  promote_to_queen  },
  { MenuViewer::Icon::B_ROOK,      "Promote to Rook",   promote_to_rook   },
  { MenuViewer::Icon::B_BISHOP,    "Promote to Bishop", promote_to_bishop },
  { MenuViewer::Icon::B_KNIGHT,    "Promote to Knight", promote_to_knight },
  { MenuViewer::Icon::END_MENU,     nullptr,            nullptr           }
};

void 
PromotionController::enter()
{
  menu_viewer.show(board_controller.is_game_play_white() ? white_menu : black_menu);
}

void 
PromotionController::leave(bool going_to_deep_sleep)
{
}

void 
PromotionController::key_event(EventMgr::KeyEvent key)
{
  menu_viewer.event(key);
}
