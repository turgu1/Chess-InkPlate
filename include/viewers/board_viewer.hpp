// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#if CHESS_LINUX_BUILD
#else
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/semphr.h"
#endif

#include <vector>
#include <string>
#include <unordered_map>

#include "global.hpp"
#include "viewers/page.hpp"
#include "models/fonts.hpp"

class BoardViewer
{
  private:
    static constexpr char const * TAG = "BoardViewer";

  public:

    BoardViewer() { }
   ~BoardViewer() { }

    /**
     * @brief Show a page on the display.
     * 
     */
    void show_board(bool        play_white, 
                    Pos         cursor_pos, 
                    Pos         from_pos, 
                    Step    * steps, 
                    int         step_count, 
                    std::string msg);

    void show_cursor(bool play_white, Dim dim, Pos pos, Page::Format & fmt, bool bold);

};

#if __BOARD_VIEWER__
  BoardViewer board_viewer;
#else
  extern BoardViewer board_viewer;
#endif
