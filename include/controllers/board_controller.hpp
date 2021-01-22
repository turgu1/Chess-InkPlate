// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "controllers/event_mgr.hpp"

#include "chess_engine.hpp"

class BoardController
{
  public:
    BoardController() : 
                  msg(""),
         game_started(false  ),
      game_play_white(false   ),
           game_board(nullptr),
            game_over(false  ) { }
    
    void key_event(EventMgr::KeyEvent key);
    void     enter();
    void     leave(bool going_to_deep_sleep = false);

  private:
    static constexpr char const * TAG = "BoardController";

    std::string  msg;

    bool         game_started;
    Pos          cursor_pos;
    Pos          from_pos;
    Step         game_steps[1000];
    Step       * best_move;
    Position     game_pos;
    Position   * pos;
    int          game_play_number;
    bool         game_play_white;
    Board      * game_board;
    bool         game_over;

    void    new_game(bool user_play_white);
    void engine_play();
    void        play(Pos from_pos, Pos to_pos);
};

#if __BOARD_CONTROLLER__
  BoardController board_controller;
#else
  extern BoardController board_controller;
#endif
