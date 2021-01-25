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
#include <fstream>

static inline bool is_white_fig(int8_t fig) { return fig > 0; }
static inline bool is_black_fig(int8_t fig) { return fig < 0; }

bool 
BoardController::load()
{
  std::string   filename = MAIN_FOLDER "/current_game.save";
  std::ifstream file(filename, std::ios::in | std::ios::binary);

  LOG_D("Loading saved game from file %s.", filename.c_str());

  int8_t  version;
  int16_t step_count;

  if (!file.is_open()) {
    LOG_I("Unable to open saved game file.");
    return false;
  }

  for (;;) {
    if (file.read(reinterpret_cast<char *>(&version), 1).fail()) break;
    if (version != SAVED_GAME_FILE_VERSION) break;

    if (file.read(reinterpret_cast<char *>(&game_play_white), sizeof(game_play_white)).fail()) break;
    if (file.read(reinterpret_cast<char *>(&step_count     ), sizeof(step_count     )).fail()) break;

    for (int16_t i = 0; i < step_count; i++) {
      if (file.read(reinterpret_cast<char *>(&game_steps[i]), sizeof(Step)).fail()) break;
    }

    break;
  }

  bool res = !file.fail();
  file.close();

  LOG_D("Saved game load %s.", res ? "Success" : "Error");

  if (res) {
    game_play_number = step_count;
  }  

  return res;
}

void 
BoardController::save()
{
  std::string   filename = MAIN_FOLDER "/current_game.save";
  std::ofstream file(filename, std::ios::out | std::ios::binary);

  LOG_D("Saving game to file %s", filename.c_str());

  if (!file.is_open()) {
    LOG_E("Not able to open saved game file.");
    return;
  }

  int16_t step_count = game_play_number;

  for (;;) {
    if (file.write(reinterpret_cast<const char *>(&SAVED_GAME_FILE_VERSION), 1    ).fail()) break;

    if (file.write(reinterpret_cast<const char *>(&game_play_white), sizeof(game_play_white)).fail()) break;
    if (file.write(reinterpret_cast<const char *>(&step_count     ), sizeof(step_count     )).fail()) break;

    for (int16_t i = 0; i < step_count; i++) {
      if (file.write(reinterpret_cast<const char *>(&game_steps[i]), sizeof(Step)).fail()) break;
    }

    break;
  }

  bool res = !file.fail();
  file.close();

  LOG_D("Game saved %s.", res ? "Success" : "Error");

  return;
}

void 
BoardController::replay()
{
  Position * pos = chess_engine.get_pos(0);
  game_board     = chess_engine.get_board();

  chess_engine.load_board_from_fen(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -"
  );

  pos[0].white_move = true;

  for (int16_t step_idx = 0; step_idx < game_play_number; step_idx++) {
    chess_engine.move_step(0, game_steps[step_idx]);
    chess_engine.move_pos (0, game_steps[step_idx]);

    chess_engine.generate_steps(1);
    pos[1].white_move = !pos[0].white_move;
    pos[0]            =  pos[1];
  }

  cursor_pos = game_play_white ? Pos(3, 3) : Pos(4, 4);
  from_pos   = Pos(-1, -1);
}

void
BoardController::new_game(bool user_play_white)
{
  game_over    = false;
  game_started = true;

  chess_engine.load_board_from_fen(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -"
  );

  game_board = chess_engine.get_board();

  game_play_white  = user_play_white;
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

  Position * pos       = chess_engine.get_pos(0);
  Step     * best_move = chess_engine.get_best_move(0);

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

    if (game_steps[game_play_number].check == CheckType::NONE) {
      if (( pos[0].white_move && chess_engine.check_on_black_king()) ||
          (!pos[0].white_move && chess_engine.check_on_white_king())) {
        game_steps[game_play_number].check = CheckType::CHECK;
      }
    }

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
  Position * pos       = chess_engine.get_pos(0);
  Step     * best_move = chess_engine.get_best_move(0);

  int8_t f1, f2;
  int8_t c1, c2;

  c1 = (((7 - pos_from.y) * 8) + pos_from.x);
  c2 = (((7 - pos_to.y  ) * 8) + pos_to.x  );

  f1 = (*game_board)[c1];
  f2 = (*game_board)[c2];

  std::cout << "=====> f1:" << +f1 << " f2:" << +f2 << " c1:" << +c1 << " c2:" << +c2 << std::endl;

  pos[0].white_move = game_play_white;
  chess_engine.generate_steps(0);

  int step_idx;
  bool found = false;

  for (step_idx = 0; step_idx < pos[0].steps_count; step_idx++) {
    Step * s = &pos[0].steps[step_idx];
    std::cout << "---> f1:" << +s->f1 << " f2:" << +s->f2 << " c1:" << +s->c1 << " c2:" << +s->c2 << std::endl;
  }

  for (step_idx = 0; step_idx < pos[0].steps_count; step_idx++) {
    Step * s = &pos[0].steps[step_idx];
    if ((s->c1 == c1) && (s->c2 == c2)) {
      found = true;
      break;
    }
  }

  // best_move[0].c1 = -1;
  // chess_engine.getbm(0, s.str());

  // if (best_move[0].c1 != -1) {
  //   for (int i = 0; i < pos[0].steps_count; i++) {
  //     if ((pos[0].steps[i].c1   == best_move[0].c1  ) && 
  //         (pos[0].steps[i].c2   == best_move[0].c2  ) &&
  //         (pos[0].steps[i].type == best_move[0].type)) {
  //       pos[0].cur_step = i;
  //     }
  //   }

  if (found) {
    best_move[0] = pos[0].steps[step_idx];
    
    pos[0].cur_step = step_idx;
    chess_engine.move_step(0, pos[0].steps[pos[0].cur_step]);
    chess_engine. move_pos(0, pos[0].steps[pos[0].cur_step]);

    LOG_D("make move: %s", chess_engine.step_to_str(pos[0].steps[pos[0].cur_step]).c_str());
    
    game_steps[game_play_number] = pos[0].steps[pos[0].cur_step];

    if (game_steps[game_play_number].check == CheckType::NONE) {
      if (( pos[0].white_move && chess_engine.check_on_black_king()) ||
          (!pos[0].white_move && chess_engine.check_on_white_king())) {
        game_steps[game_play_number].check = CheckType::CHECK;
      }
    }

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
    if (load()) {
      replay();
    }
    else {
      new_game(game_play_white);
      game_started = true;
    }
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
  if (going_to_deep_sleep) save();
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
          cursor_pos = game_play_white ? Pos(3, 3) : Pos(4, 4);

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