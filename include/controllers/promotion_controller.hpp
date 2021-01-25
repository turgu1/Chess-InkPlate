// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "controllers/event_mgr.hpp"

class PromotionController
{
  private:
    static constexpr char const * TAG = "PromotionController";

  public:
    PromotionController() { };
                         
    void key_event(EventMgr::KeyEvent key);
    void enter();
    void leave(bool going_to_deep_sleep = false);
};

#if __PROMOTION_CONTROLLER__
  PromotionController promotion_controller;
#else
  extern PromotionController promotion_controller;
#endif
