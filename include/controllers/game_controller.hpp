// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include "global.hpp"
#include "controllers/event_mgr.hpp"

#include "chess_engine.hpp"

class GameController
{
  public:
    GameController() : 
                         msg(""),
                game_started(false  ),
             game_play_white(true   ),
                  game_board(nullptr),
                   game_over(false  ),
          complete_user_move(false  ),
         promotion_move_type(MoveType::UNKNOWN) { }
    
    void           key_event(EventMgr::KeyEvent key);
    void               enter();
    void               leave(bool going_to_deep_sleep = false);
    void            new_game(bool user_play_white);
    void    set_promotion_to(MoveType move_type) { promotion_move_type = move_type; }
    MoveType   get_promotion() { return promotion_move_type; }
    bool  is_game_play_white() { return game_play_white;     }
    void                save();

  private:
    static constexpr char const * TAG = "GameController";
    static constexpr uint8_t      SAVED_GAME_FILE_VERSION = 1;

    std::string  msg;

    bool         game_started;
    Pos          cursor_pos;
    Pos          from_pos;
    Step         game_steps[1000];
    Step       * best_move;
    Position     game_pos;
    int16_t      game_play_number;
    bool         game_play_white;
    Board      * game_board;
    bool         game_over;
    bool         complete_user_move;

    MoveType     promotion_move_type;

    void   engine_play();
    void          play(Pos from_pos, Pos to_pos);
    void        replay();
    bool          load();
    void complete_move(bool async);
};

#if __BOARD_CONTROLLER__
  GameController game_controller;
#else
  extern GameController game_controller;
#endif
