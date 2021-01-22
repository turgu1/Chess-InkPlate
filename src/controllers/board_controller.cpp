// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOARD_CONTROLLER__ 1
#include "controllers/board_controller.hpp"

#include "controllers/app_controller.hpp"
#include "viewers/board_viewer.hpp"
#include "viewers/page.hpp"
#include "viewers/msg_viewer.hpp"

#if EPUB_INKPLATE_BUILD
  #include "nvs.h"
#endif

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

static inline bool is_white_fig(int8_t fig) { return fig > 0; }
static inline bool is_black_fig(int8_t fig) { return fig < 0; }

void
BoardController::new_game(bool user_play_white)
{
  game_over = false;
  chess_engine.load_board_from_fen(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -"
  );

  game_board = chess_engine.get_board();

  game_play_number = 0;

  cursor_pos = user_play_white ? Pos(3, 1) : Pos(3, 6);
  from_pos   = Pos(-1, -1);

  if (!user_play_white) engine_play();
}

void
BoardController::engine_play()
{
  board_viewer.show_board(
    game_play_white, Pos(-1, -1), Pos(-1, -1), 
    game_steps, game_play_number,
    msg = "Engine is playing.");

  msg.clear();

  position_t * pos       = chess_engine.get_pos(0);
  step_t     * best_move = chess_engine.get_best_move(0);

  for (int i = 0; i < MAXEPD; i++) best_move[i].c1 = -1;

  pos[0].best.c1 = -1;
  chess_engine.solve_step();

  if (pos[0].best.c1 != -1) {
    for (int i = 0; i < pos[0].steps_count; i++) {
      if ((pos[0].steps[i].c1   == pos[0].best.c1  ) && 
          (pos[0].steps[i].c2   == pos[0].best.c2  ) &&
          (pos[0].steps[i].type == pos[0].best.type)) {
        pos[0].cur_step = i;
      }
    }

    chess_engine.move_step(0, pos[0].steps[pos[0].cur_step]);
    chess_engine.move_pos (0, pos[0].steps[pos[0].cur_step]);

    // std::cout << "make move: " << chess_engine.step_to_str(pos[0].steps[pos[0].cur_step]) << std::endl;
    
    game_steps[game_play_number] = pos[0].steps[pos[0].cur_step];
    pos[0] = pos[1];
    game_play_number++;
  } 
  else {
    msg = "ERROR!";
  }
}

void
BoardController::play(Pos pos_from, Pos pos_to)
{
  position_t * pos       = chess_engine.get_pos(0);
  step_t     * best_move = chess_engine.get_best_move(0);

  std::ostringstream s;

  int8_t f1, f2;
  int8_t c1, c2;

  c1 = (((7 - pos_from.y) * 8) + pos_from.x);
  c2 = (((7 - pos_to.y  ) * 8) + pos_to.x  );

  f1 = (*game_board)[c1];
  f2 = (*game_board)[c2];

  if (game_play_white) {
    for (;;) {
      if (f1 == KING) {
        if (c1 == 60) {
          if      (c2 == 58) { s << "0-0-0"; break; }
          else if (c2 == 62) { s << "0-0"  ; break; }
        }
        s << 'K' 
          << chess_engine.board_idx_to_str(c1)
          << (is_black_fig(f2) ? 'x' : '-')
          << chess_engine.board_idx_to_str(c2);
      }
      else {
        if (f1 > PAWN) s << fig_symb[f1];
        s << chess_engine.board_idx_to_str(c1)
          << (is_black_fig(f2) ? 'x' : '-')
          << chess_engine.board_idx_to_str(c2);
      }
      break;
    }
  }
  else {
    for (;;) {
      if (f1 == -KING) {
        if (c1 == 4) {
          if      (c2 == 2) { s << "0-0-0"; break; }
          else if (c2 == 6) { s << "0-0"  ; break; }
        }
        s << 'K' 
          << chess_engine.board_idx_to_str(c1)
          << (is_white_fig(f2) ? 'x' : '-')
          << chess_engine.board_idx_to_str(c2);
      }
      else {
        if (f1 < -PAWN) s << fig_symb[-f1];
        s << chess_engine.board_idx_to_str(c1)
          << (is_white_fig(f2) ? 'x' : '-')
          << chess_engine.board_idx_to_str(c2);
      }
      break;
    }
  }  

  LOG_D("User Move: %s", s.str().c_str());

  chess_engine.generate_steps(0);
  best_move[0].c1 = -1;
  chess_engine.getbm(0, s.str());

  if (best_move[0].c1 != -1) {
    for (int i = 0; i < pos[0].steps_count; i++) {
      if ((pos[0].steps[i].c1   == best_move[0].c1  ) && 
          (pos[0].steps[i].c2   == best_move[0].c2  ) &&
          (pos[0].steps[i].type == best_move[0].type)) {
        pos[0].cur_step = i;
      }
    }

    chess_engine.move_step(0, pos[0].steps[pos[0].cur_step]);
    chess_engine. move_pos(0, pos[0].steps[pos[0].cur_step]);

    LOG_D("make move: %s", chess_engine.step_to_str(pos[0].steps[pos[0].cur_step]).c_str());
    
    game_steps[game_play_number] = pos[0].steps[pos[0].cur_step];

    pos[1].white_move = !pos[0].white_move;
    pos[0]            =  pos[1];

    game_play_number++;

    engine_play();
  } 
  else {
    msg = "Move is illegal. Please retry.";
  }
}

void 
BoardController::enter()
{ 
  if (!game_started) {
    new_game(game_play_white);
    game_started = true;
  }
  
  if (msg.empty()) msg = "User play. Please make a move:";

  board_viewer.show_board(
    game_play_white, 
    cursor_pos, from_pos, 
    game_steps, game_play_number,
    msg);
}

void 
BoardController::leave(bool going_to_deep_sleep)
{
}

void 
BoardController::key_event(EventMgr::KeyEvent key)
{
  switch (key) {
    case EventMgr::KeyEvent::PREV:
      if (game_play_white) cursor_pos.x = (cursor_pos.x == 0) ? 7 : cursor_pos.x - 1;
      else                 cursor_pos.x = (cursor_pos.x == 7) ? 0 : cursor_pos.x + 1;
      board_viewer.show_board(
        game_play_white, cursor_pos, from_pos, 
        game_steps, game_play_number, 
        msg);
      break;

    case EventMgr::KeyEvent::DBL_PREV:
      if (game_play_white) cursor_pos.y = (cursor_pos.y == 0) ? 7 : cursor_pos.y - 1;
      else                 cursor_pos.y = (cursor_pos.y == 7) ? 0 : cursor_pos.y + 1;
      board_viewer.show_board(
        game_play_white, cursor_pos, from_pos, 
        game_steps, game_play_number,
        msg);
      break;

    case EventMgr::KeyEvent::NEXT:
      if (game_play_white) cursor_pos.x = (cursor_pos.x == 7) ? 0 : cursor_pos.x + 1;
      else                 cursor_pos.x = (cursor_pos.x == 0) ? 7 : cursor_pos.x - 1;
      board_viewer.show_board(
        game_play_white, cursor_pos, from_pos, 
        game_steps, game_play_number,
        msg);
      break;

    case EventMgr::KeyEvent::DBL_NEXT:
      if (game_play_white) cursor_pos.y = (cursor_pos.y == 7) ? 0 : cursor_pos.y + 1;
      else                 cursor_pos.y = (cursor_pos.y == 0) ? 7 : cursor_pos.y - 1;
      board_viewer.show_board(
        game_play_white, cursor_pos, from_pos, 
        game_steps, game_play_number, 
        msg);
      break;
    
    case EventMgr::KeyEvent::SELECT: 
      {
        Pos null_pos = Pos(-1, -1);
      
        if (memcmp(&from_pos, &cursor_pos, sizeof(Pos)) == 0) {
          from_pos = null_pos;
          board_viewer.show_board(
            game_play_white, cursor_pos, from_pos, 
            game_steps, game_play_number,
            msg);
        }
        else if (memcmp(&from_pos, &null_pos, sizeof(Pos)) == 0) {
          int8_t board_idx = (((7 - cursor_pos.y) * 8) + cursor_pos.x);
          if ((game_play_white  && is_white_fig((*game_board)[board_idx])) ||
              (!game_play_white && is_black_fig((*game_board)[board_idx]))) {
            from_pos = cursor_pos;
          }
          board_viewer.show_board(
            game_play_white, cursor_pos, from_pos, 
            game_steps, game_play_number, 
            msg);
        }
        else {
          play(from_pos, cursor_pos);
          from_pos = null_pos;
          cursor_pos = game_play_white ? Pos(3, 3) : Pos(5, 3);

          if (msg.empty()) msg = "User play. Please make a move:";

          board_viewer.show_board(
            game_play_white, cursor_pos, from_pos, 
            game_steps, game_play_number, 
            msg);
        }
      }
      break;

    case EventMgr::KeyEvent::DBL_SELECT:
      app_controller.set_controller(AppController::Ctrl::OPTION);
      break;
      
    case EventMgr::KeyEvent::NONE:
      break;
  }
}