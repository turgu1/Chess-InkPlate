#define CHESS_ENGINE 1
#include "chess_engine.hpp"

#define _STEPS_ 1
#include "chess_engine_steps.hpp"

#define _WEIGHTS_ 1
#include "chess_engine_weights.hpp"

#include <cinttypes>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>
#include <cstring>

// =====Shared variables ==================================================

static Board board;

static int8_t idx_white_king = 0;
static int8_t idx_black_king = 0;

static position_t pos[MAXDEPTH];

static bool   endgame = false;

enum class TaskReq : int8_t { EXEC, STOP };

struct TaskQueueData {
  TaskReq req;
};

enum class EngineReq  : int8_t { COMPLETED };

struct EngineQueueData {
  EngineReq req;
};

#if CHESS_LINUX_BUILD  
  static mqd_t task_queue;
  static mqd_t engine_queue;

  static mq_attr task_attr      = { 0, 5, sizeof(     TaskQueueData), 0 };
  static mq_attr engine_attr    = { 0, 5, sizeof(   EngineQueueData), 0 };

  #define QUEUE_SEND(q, m, t)        mq_send(q, (const char *) &m, sizeof(m),       1)
  #define QUEUE_RECEIVE(q, m, t)  mq_receive(q,       (char *) &m, sizeof(m), nullptr)
#else
  #include <esp_pthread.h>

  static esp_pthread_cfg_t create_config(const char *name, int core_id, int stack, int prio)
  {
      auto cfg = esp_pthread_get_default_config();
      cfg.thread_name = name;
      cfg.pin_to_core = core_id;
      cfg.stack_size = stack;
      cfg.prio = prio;
      return cfg;
  }

  static xQueueHandle task_queue;
  static xQueueHandle engine_queue;
  
  #define QUEUE_SEND(q, m, t)        xQueueSend(q, &m, t)
  #define QUEUE_RECEIVE(q, m, t)  xQueueReceive(q, &m, t)
#endif

// ===== Shared funtions ==================================================

static bool 
check_on_white_king()
{
  signed char f2 = NO_FIG;
  int         j  = 0;
  
  if (board[idx_white_king] != KING) {
    for (int i = 0; i < 64; i++) {
      if (board[i] == KING) {
        idx_white_king = i;
        break;
      }
    }
  }

  while (diag_step[idx_white_king][j] != 99) {
    if (diag_step[idx_white_king][j] == 88) f2 = NO_FIG;
    else if (f2 == NO_FIG) {
      f2 = board[diag_step[idx_white_king][j]];
      if (f2 == -BISHOP || f2 == -QUEEN) return true;
    }
    j++;
  }

  f2 = NO_FIG;
  j  = 0;

  while (stra_step[idx_white_king][j] != 99) {
    if (stra_step[idx_white_king][j] == 88) f2 = NO_FIG;
    else if (f2 == NO_FIG) {
      f2 = board[stra_step[idx_white_king][j]];
      if ((f2 == -ROOK) || (f2 == -QUEEN)) return true;
      if ((j == 0) || (stra_step[idx_white_king][j] == 88))
        if (f2 == -KING) return true;
    }
    j++;
  }

  j = 0;
  while (knight_step[idx_white_king][j] != 99) {
    if (board[knight_step[idx_white_king][j]] == -KNIGHT) return true;
    j++;
  }
  if (row[idx_white_king] < 7) {
    if ((column[idx_white_king] > 1) && (board[idx_white_king - 9] == -PAWN)) return true;
    if ((column[idx_white_king] < 8) && (board[idx_white_king - 7] == -PAWN)) return true;
  }
  j = 0;
  while (king_step[idx_white_king][j] != 99) {
    if (board[king_step[idx_white_king][j]] == -KING) return true;
    j++;
  }
  
  return false;
}

static bool 
check_on_black_king()
{
  signed char f2 = NO_FIG;
  int         j  = 0;
  
  if (board[idx_black_king] != -KING) {
    for (int i = 0; i < 64; i++) { 
      if (board[i] == -KING) {
        idx_black_king = i;
        break;
      }
    }
  }

  while (diag_step[idx_black_king][j] != 99) {
    if (diag_step[idx_black_king][j] == 88) f2 = NO_FIG;
    else if (f2 == NO_FIG) {
      f2 = board[diag_step[idx_black_king][j]];
      if (f2 == BISHOP || f2 == QUEEN) return true;
    }
    j++;
  }

  f2 = NO_FIG;
  j  = 0;

  while (stra_step[idx_black_king][j] != 99) {
    if (stra_step[idx_black_king][j] == 88) f2 = NO_FIG;
    else if (f2 == NO_FIG) {
      f2 = board[stra_step[idx_black_king][j]];
      if (f2 == ROOK || f2 == QUEEN) return true;
      if (j == 0 || stra_step[idx_black_king][j] == 88) {
        if (f2 == KING) return true;
      }
    }
    j++;
  }

  j = 0;

  while (knight_step[idx_black_king][j] != 99) {
    if (board[knight_step[idx_black_king][j]] == KNIGHT) return true;
    j++;
  }

  if (row[idx_black_king] > 2) {
    if ((column[idx_black_king] > 1) && (board[idx_black_king + 7] == PAWN)) return true;
    if ((column[idx_black_king] < 8) && (board[idx_black_king + 9] == PAWN)) return true;
  }

  j = 0;

  while (king_step[idx_black_king][j] != 99) {
    if (board[king_step[idx_black_king][j]] == KING) return true;
    j++;
  }

  return false;
}

// ===== Chess Task =======================================================

// This task is parallellizing part of the move generator. It takes care of all
// moves related to pawns and kings.
void ChessTask::exec()
{
  int    pos_idx;
  int8_t f;
  //unsigned long task_tik;
  //unsigned long task_count = 0;

  TaskQueueData task_queue_data;

  for (;;) {
    QUEUE_RECEIVE(task_queue, task_queue_data, 0);
    if (task_queue_data.req == TaskReq::EXEC) {
      // task_tik=micros();
      pos_idx = task_pos_idx;
      if (board[idx_white_king] != KING) {
        for (int board_idx = 0; board_idx < 64; board_idx++) {
          if (board[board_idx] == KING) {
            idx_white_king = board_idx;
            break;
          }
        }
      }
      if (board[idx_black_king] != -KING) {
        for (int board_idx = 0; board_idx < 64; board_idx++) {
          if (board[board_idx] == -KING) {
            idx_black_king = board_idx;
            break;
          }
        }
      }
      if (pos_idx > 0) {
        if (pos[pos_idx - 1].steps[pos[pos_idx - 1].cur_step].check == CheckType::NONE) {
          if (pos[pos_idx].white_move) pos[pos_idx].check_on_table = check_on_white_king();
          else pos[pos_idx].check_on_table = check_on_black_king();
          pos[pos_idx - 1].steps[pos[pos_idx - 1].cur_step].check = pos[pos_idx].check_on_table ? CheckType::CHECK : CheckType::NONE;
        } 
        else pos[pos_idx].check_on_table = true;
      } 
      else if (pos[0].white_move) pos[0].check_on_table = check_on_white_king();
      else pos[0].check_on_table = check_on_black_king();

      steps_count = 0;

      for (int ii = 0; ii < 64; ii++) {
        int board_idx = (pos[pos_idx].white_move)  ? ii : 63 - ii;

        f = board[board_idx];

        if ((f == NO_FIG) || 
            (chess_engine.is_black_fig(f) &&  pos[pos_idx].white_move) || 
            (chess_engine.is_white_fig(f) && !pos[pos_idx].white_move)) continue;

        if (f == PAWN) {
          if ((row[board_idx] < 7) && (board[board_idx - 8] == NO_FIG)) add_one_step(board_idx, board_idx - 8);
          if ((row[board_idx] == 2) && (board[board_idx - 8] == NO_FIG) && (board[board_idx - 16] == NO_FIG)) add_one_step(board_idx, board_idx - 16);
          if (row[board_idx] == 7) {
            if (board[board_idx - 8] == NO_FIG) { // No piece on front on last row
              add_one_step(board_idx, board_idx - 8); steps[steps_count - 1].type = MoveType::PROMOTE_TO_KNIGHT;
              add_one_step(board_idx, board_idx - 8); steps[steps_count - 1].type = MoveType::PROMOTE_TO_BISHOP;
              add_one_step(board_idx, board_idx - 8); steps[steps_count - 1].type = MoveType::PROMOTE_TO_ROOK;
              add_one_step(board_idx, board_idx - 8); steps[steps_count - 1].type = MoveType::PROMOTE_TO_QUEEN;
            }
            if ((column[board_idx] > 1) && chess_engine.is_black_fig(board[board_idx - 9])) { // A piece can be taken on last row to the left
              add_one_step(board_idx, board_idx - 9); steps[steps_count - 1].type = MoveType::PROMOTE_TO_KNIGHT;
              add_one_step(board_idx, board_idx - 9); steps[steps_count - 1].type = MoveType::PROMOTE_TO_BISHOP;
              add_one_step(board_idx, board_idx - 9); steps[steps_count - 1].type = MoveType::PROMOTE_TO_ROOK;
              add_one_step(board_idx, board_idx - 9); steps[steps_count - 1].type = MoveType::PROMOTE_TO_QUEEN;
            }
            if ((column[board_idx] < 8) && chess_engine.is_black_fig(board[board_idx - 7])) { // A piece can be taken on last row to the right
              add_one_step(board_idx, board_idx - 7); steps[steps_count - 1].type = MoveType::PROMOTE_TO_KNIGHT;
              add_one_step(board_idx, board_idx - 7); steps[steps_count - 1].type = MoveType::PROMOTE_TO_BISHOP;
              add_one_step(board_idx, board_idx - 7); steps[steps_count - 1].type = MoveType::PROMOTE_TO_ROOK;
              add_one_step(board_idx, board_idx - 7); steps[steps_count - 1].type = MoveType::PROMOTE_TO_QUEEN;
            }
          } 
          else {
            if ((column[board_idx] > 1) && chess_engine.is_black_fig(board[board_idx - 9])) add_one_step(board_idx, board_idx - 9);
            if ((column[board_idx] < 8) && chess_engine.is_black_fig(board[board_idx - 7])) add_one_step(board_idx, board_idx - 7);
          }
        } 
        else if (f == -PAWN) {

          if ((row[board_idx] > 2) && (board[board_idx + 8] == NO_FIG)) add_one_step(board_idx, board_idx + 8);
          if ((row[board_idx] == 7) && (board[board_idx + 8] == NO_FIG) && (board[board_idx + 16] == NO_FIG)) add_one_step(board_idx, board_idx + 16);
          if (row[board_idx] == 2) {
            if (board[board_idx + 8] == NO_FIG) {
              add_one_step(board_idx, board_idx + 8); steps[steps_count - 1].type = MoveType::PROMOTE_TO_KNIGHT;
              add_one_step(board_idx, board_idx + 8); steps[steps_count - 1].type = MoveType::PROMOTE_TO_BISHOP;
              add_one_step(board_idx, board_idx + 8); steps[steps_count - 1].type = MoveType::PROMOTE_TO_ROOK;
              add_one_step(board_idx, board_idx + 8); steps[steps_count - 1].type = MoveType::PROMOTE_TO_QUEEN;
            }
            if ((column[board_idx] > 1) && chess_engine.is_white_fig(board[board_idx + 7])) {
              add_one_step(board_idx, board_idx + 7); steps[steps_count - 1].type = MoveType::PROMOTE_TO_KNIGHT;
              add_one_step(board_idx, board_idx + 7); steps[steps_count - 1].type = MoveType::PROMOTE_TO_BISHOP;
              add_one_step(board_idx, board_idx + 7); steps[steps_count - 1].type = MoveType::PROMOTE_TO_ROOK;
              add_one_step(board_idx, board_idx + 7); steps[steps_count - 1].type = MoveType::PROMOTE_TO_QUEEN;
            }
            if ((column[board_idx] < 8) && chess_engine.is_white_fig(board[board_idx + 9])) {
              add_one_step(board_idx, board_idx + 9); steps[steps_count - 1].type = MoveType::PROMOTE_TO_KNIGHT;
              add_one_step(board_idx, board_idx + 9); steps[steps_count - 1].type = MoveType::PROMOTE_TO_BISHOP;
              add_one_step(board_idx, board_idx + 9); steps[steps_count - 1].type = MoveType::PROMOTE_TO_ROOK;
              add_one_step(board_idx, board_idx + 9); steps[steps_count - 1].type = MoveType::PROMOTE_TO_QUEEN;
            }
          } 
          else {
            if ((column[board_idx] > 1) && chess_engine.is_white_fig(board[board_idx + 7])) add_one_step(board_idx, board_idx + 7);
            if ((column[board_idx] < 8) && chess_engine.is_white_fig(board[board_idx + 9])) add_one_step(board_idx, board_idx + 9);
          }
        } 
        else if (!endgame && (abs(f) == KING)) add_king_step(board_idx);
      }

      if ((pos[pos_idx].en_passant_pp != 0) && 
          (board[pos[pos_idx].en_passant_pp] == NO_FIG)) {
        if (pos[pos_idx].white_move) {
          if ((column[pos[pos_idx].en_passant_pp] > 1) && 
              (board[pos[pos_idx].en_passant_pp + 7] == PAWN)) {
            add_one_step(pos[pos_idx].en_passant_pp + 7, pos[pos_idx].en_passant_pp);
            steps[steps_count - 1].type = MoveType::EN_PASSANT;
            steps[steps_count - 1].f2   = -PAWN;
          }
          if ((column[pos[pos_idx].en_passant_pp] < 8) && 
              (board[pos[pos_idx].en_passant_pp + 9] == PAWN)) {
            add_one_step(pos[pos_idx].en_passant_pp + 9, pos[pos_idx].en_passant_pp);
            steps[steps_count - 1].type = MoveType::EN_PASSANT;
            steps[steps_count - 1].f2   = -PAWN;
          }
        } 
        else {
          if ((column[pos[pos_idx].en_passant_pp] > 1) && 
              (board[pos[pos_idx].en_passant_pp - 9] == -PAWN)) {
            add_one_step(pos[pos_idx].en_passant_pp - 9, pos[pos_idx].en_passant_pp);
            steps[steps_count - 1].type = MoveType::EN_PASSANT;
            steps[steps_count - 1].f2   = PAWN;
          }
          if ((column[pos[pos_idx].en_passant_pp] < 8) && 
              (board[pos[pos_idx].en_passant_pp - 7] == -PAWN)) {
            add_one_step(pos[pos_idx].en_passant_pp - 7, pos[pos_idx].en_passant_pp);
            steps[steps_count - 1].type = MoveType::EN_PASSANT;
            steps[steps_count - 1].f2   = PAWN;
          }
        }
      }
      //   task_execute+=micros()-task_tik;
      EngineQueueData engine_queue_data;
      engine_queue_data.req = EngineReq::COMPLETED;
      QUEUE_SEND(engine_queue, engine_queue_data, 0);
    }
    else {
      sched_yield();
    }

    // task_count++;

    // if (task_count > 7000000) {
    //   task_count = 0;
    //   if (!std::cin.eof()) {
    //     std::string s;
    //     std::getline(std::cin, s, '\n');
    //     trim(s);
    //     to_uppercase(s);
    //     if (s == "STOP") {
    //       halt = true;
    //       std::cout << "STOPPED" << std::endl;
    //     } //timelimith=millis(); }
    //   } 
    //   else delay(1);
    // }
  }
}

void 
ChessTask::add_one_step(int c1, int c2)
{
  steps[steps_count].type = MoveType::SIMPLE;
  steps[steps_count].c1   = c1;
  steps[steps_count].c2   = c2;
  steps[steps_count].f1   = board[c1];
  steps[steps_count].f2   = board[c2];
  steps_count++;
}

void
ChessTask::add_king_step(int8_t board_idx)
{
  signed char f1 = board[board_idx];
  signed char f2;
  int         j  = 0;

  while (king_step[board_idx][j] != 99) {
    f2 = board[king_step[board_idx][j]];
    if ((f2 == NO_FIG) || 
        (chess_engine.is_black_fig(f2) && chess_engine.is_white_fig(f1)) || 
        (chess_engine.is_white_fig(f2) && chess_engine.is_black_fig(f1))) {
      steps[steps_count].type =MoveType::SIMPLE;
      steps[steps_count].c1 = board_idx;
      steps[steps_count].c2 = king_step[board_idx][j];
      steps[steps_count].f1 = f1;
      steps[steps_count].f2 = f2;
      steps_count++;
    }
    j++;
  }
}

void
ChessTask::retrieve_steps(int pos_idx)
{
  int count = pos[pos_idx].steps_count;
  for (int i = 0; i < steps_count; i++) {
    pos[pos_idx].steps[count++] = steps[i];
  }
  pos[pos_idx].steps_count = count;
}

// ===== Chess Engine =====================================================

void 
ChessEngine::move_pos(int pos_idx, step_t & step)
{
  pos[pos_idx + 1].white_castle_kingside_ok  = pos[pos_idx].white_castle_kingside_ok;
  pos[pos_idx + 1].white_castle_queenside_ok = pos[pos_idx].white_castle_queenside_ok;
  pos[pos_idx + 1].black_castle_kingside_ok  = pos[pos_idx].black_castle_kingside_ok;
  pos[pos_idx + 1].black_castle_queenside_ok = pos[pos_idx].black_castle_queenside_ok;
  pos[pos_idx + 1].en_passant_pp = 0;

  if (pos[pos_idx].white_move) { //
    if (pos[pos_idx].white_castle_kingside_ok || pos[pos_idx].white_castle_queenside_ok) {
      if (step.c1 == 60) {
        pos[pos_idx + 1].white_castle_kingside_ok  = false;
        pos[pos_idx + 1].white_castle_queenside_ok = false;
      } 
      else if (step.c1 == 63) pos[pos_idx + 1].white_castle_kingside_ok  = false;
      else if (step.c1 == 56) pos[pos_idx + 1].white_castle_queenside_ok = false;
    }
    if (step.type == MoveType::SIMPLE && step.f1 == PAWN && step.c2 == step.c1 - 16) {
      if (((column[step.c2] > 1) && (board[step.c2 - 1] == -PAWN)) || 
          ((column[step.c2] < 8) && (board[step.c2 + 1] == -PAWN))) {
        pos[pos_idx + 1].en_passant_pp = step.c1 - 8;
      }
    }
    pos[pos_idx + 1].weight_white = pos[pos_idx].weight_white;
    pos[pos_idx + 1].weight_black = pos[pos_idx].weight_black;
    if (step.f2 != NO_FIG) {
      pos[pos_idx + 1].weight_black -= fig_weight[-step.f2];
    }
    if (step.type > MoveType::CASTLE_QUEENSIDE) {
      pos[pos_idx + 1].weight_white += fig_weight[(int)(step.type) - 2] - 100;
    }
    
    if (stats) {
      if (step.f1 == KING && endgame) {
        pos[pos_idx + 1].weight_both = pos[pos_idx].weight_both + 
                                       stat_weight_white[KING][step.c2] - 
                                       stat_weight_white[KING][step.c1];
      }
      else {
        pos[pos_idx + 1].weight_both = pos[pos_idx].weight_both + 
                                       stat_weight_white[step.f1 - 1][step.c2] - 
                                       stat_weight_white[step.f1 - 1][step.c1];
      }
      if (step.f2 != NO_FIG) pos[pos_idx + 1].weight_both += stat_weight_black[-step.f2 - 1][step.c2];
    }
  } 
  else { //
    if (pos[pos_idx].black_castle_kingside_ok || pos[pos_idx].black_castle_queenside_ok) {
      if (step.c1 == 4) {
        pos[pos_idx + 1].black_castle_kingside_ok  = false;
        pos[pos_idx + 1].black_castle_queenside_ok = false;
      } 
      else if (step.c1 == 7) pos[pos_idx + 1].black_castle_kingside_ok  = false;
      else if (step.c1 == 0) pos[pos_idx + 1].black_castle_queenside_ok = false;
    }
    if (step.type == MoveType::SIMPLE && step.f1 == -PAWN && step.c2 == step.c1 + 16) {
      if (((column[step.c2] > 1) && (board[step.c2 - 1] == PAWN)) || 
          ((column[step.c2] < 8) && (board[step.c2 + 1] == PAWN))) {
        pos[pos_idx + 1].en_passant_pp = step.c1 + 8;
      }
    }
    pos[pos_idx + 1].weight_white = pos[pos_idx].weight_white;
    pos[pos_idx + 1].weight_black = pos[pos_idx].weight_black;
    if (step.f2 != NO_FIG) pos[pos_idx + 1].weight_white -= fig_weight[step.f2];
    if (step.type > MoveType::CASTLE_QUEENSIDE) {
      pos[pos_idx + 1].weight_black += fig_weight[(int)(step.type) - 2] - 100;
    }
    
    if (stats) {
      if ((step.f1 == -KING) && endgame)
        pos[pos_idx + 1].weight_both = pos[pos_idx].weight_both - 
                                       stat_weight_black[KING][step.c2] + 
                                       stat_weight_black[KING][step.c1];
      else
        pos[pos_idx + 1].weight_both = pos[pos_idx].weight_both - 
                                       stat_weight_black[-step.f1 - 1][step.c2] + 
                                       stat_weight_black[-step.f1 - 1][step.c1];
      if (step.f2 != NO_FIG) {
        pos[pos_idx + 1].weight_both -= stat_weight_white[step.f2 - 1][step.c2];
      }
    }
  }

  move_count++;
}

void 
ChessEngine::move_step(int pos_idx, step_t & step)
{
  //checks(l,s);
  board[step.c1] = 0;
  board[step.c2] = step.f1;

  if (pos[pos_idx].white_move) {
    if (step.f1 == KING) idx_white_king = step.c2;
    switch (step.type) {
      case MoveType::SIMPLE:
        return;

      case MoveType::EN_PASSANT:
        board[step.c2 + 8] = 0;
        break;

      case MoveType::CASTLE_KINGSIDE:
        board[60] = 0;
        board[61] = ROOK;
        board[62] = KING;
        board[63] = 0;
        idx_white_king = 62;
        break;

      case MoveType::CASTLE_QUEENSIDE:
        board[60] = 0;
        board[59] = ROOK;
        board[58] = KING;
        board[57] = 0;
        board[56] = 0;
        idx_white_king = 58;
        break;

      case MoveType::PROMOTE_TO_KNIGHT: 
      case MoveType::PROMOTE_TO_BISHOP: 
      case MoveType::PROMOTE_TO_ROOK: 
      case MoveType::PROMOTE_TO_QUEEN:
        board[step.c2] = (int)(step.type) - 2;
        break;

      default:
        break;
    }
  } 
  else {
    if (step.f1 == -KING) idx_black_king = step.c2;
    switch (step.type) {
      case MoveType::SIMPLE:
        return;

      case MoveType::EN_PASSANT:
        board[step.c2 - 8] = 0;
        break;

      case MoveType::CASTLE_KINGSIDE:
        board[4] = 0;
        board[5] = -ROOK;
        board[6] = -KING;
        board[7] = 0;
        idx_black_king = 6;
        break;

      case MoveType::CASTLE_QUEENSIDE:
        board[4] = 0;
        board[3] = -ROOK;
        board[2] = -KING;
        board[1] = 0;
        board[0] = 0;
        idx_black_king = 2;
        break;

      case MoveType::PROMOTE_TO_KNIGHT: 
      case MoveType::PROMOTE_TO_BISHOP: 
      case MoveType::PROMOTE_TO_ROOK: 
      case MoveType::PROMOTE_TO_QUEEN:
        board[step.c2] = 2 - (int)(step.type);
        break;

      default:
        break;
    }
  }
}

void 
ChessEngine::back_step(int pos_idx, step_t & step)
{
  board[step.c1] = step.f1;
  board[step.c2] = step.f2;

  if (pos[pos_idx].white_move) {
    if (step.f1 == KING) idx_white_king = step.c1;
    switch (step.type) {
      case MoveType::SIMPLE:
        return;

      case MoveType::EN_PASSANT:
        board[step.c2] = 0;
        board[step.c2 + 8] = -PAWN;
        break;

      case MoveType::CASTLE_KINGSIDE:
        board[60] = KING;
        board[61] = 0;
        board[62] = 0;
        board[63] = ROOK;
        idx_white_king = 60;
        break;
        
      case MoveType::CASTLE_QUEENSIDE:
        board[60] = KING;
        board[59] = 0;
        board[58] = 0;
        board[57] = 0;
        board[56] = ROOK;
        idx_white_king = 60;
        break;

      default:
        break;
    }
  } 
  else { //
    if (step.f1 == -KING) idx_black_king = step.c1;
    switch (step.type) {
      case MoveType::SIMPLE:
        return;

      case MoveType::EN_PASSANT: //
        board[step.c2] = 0;
        board[step.c2 - 8] = PAWN;
        break;

      case MoveType::CASTLE_KINGSIDE: //
        board[4] = -KING;
        board[5] = 0;
        board[6] = 0;
        board[7] = -ROOK;
        idx_black_king = 4;
        break;
        
      case MoveType::CASTLE_QUEENSIDE: //
        board[4] = -KING;
        board[3] = 0;
        board[2] = 0;
        board[1] = 0;
        board[0] = -ROOK;
        idx_black_king = 4;
        break;

      default:
        break;
    }
  }
}

void 
ChessEngine::add_king_step(int pos_idx, int board_idx)
{
  signed char f1 = board[board_idx];
  signed char f2;
  int         j  = 0;

  while (king_step[board_idx][j] != 99) {
    f2 = board[king_step[board_idx][j]];
    if ((f2 == NO_FIG) || 
        (is_black_fig(f2) && is_white_fig(f1)) || 
        (is_white_fig(f2) && is_black_fig(f1))) {
      pos[pos_idx].steps[pos[pos_idx].steps_count].type = MoveType::SIMPLE;
      pos[pos_idx].steps[pos[pos_idx].steps_count].c1   = board_idx;
      pos[pos_idx].steps[pos[pos_idx].steps_count].c2   = king_step[board_idx][j];
      pos[pos_idx].steps[pos[pos_idx].steps_count].f1   = f1;
      pos[pos_idx].steps[pos[pos_idx].steps_count].f2   = f2;
      pos[pos_idx].steps_count++;
    }
    j++;
  }
}

void 
ChessEngine::add_knight_step(int pos_idx, int board_idx)
{
  signed char f1 = board[board_idx];
  signed char f2;
  int         j  = 0;

  while (knight_step[board_idx][j] != 99) {
    f2 = board[knight_step[board_idx][j]];
    if ((f2 == NO_FIG) || 
        (is_black_fig(f2) && is_white_fig(f1)) || 
        (is_white_fig(f2) && is_black_fig(f1))) {
      pos[pos_idx].steps[pos[pos_idx].steps_count].type = MoveType::SIMPLE;
      pos[pos_idx].steps[pos[pos_idx].steps_count].c1   = board_idx;
      pos[pos_idx].steps[pos[pos_idx].steps_count].c2   = knight_step[board_idx][j];
      pos[pos_idx].steps[pos[pos_idx].steps_count].f1   = f1;
      pos[pos_idx].steps[pos[pos_idx].steps_count].f2   = f2;
      pos[pos_idx].steps_count++;
    }
    j++;
  }
}

void 
ChessEngine::add_stra_step(int pos_idx, int board_idx)
{
  signed char f1 = board[board_idx];
  signed char f2 = NO_FIG;
  int         j  = 0;

  while (stra_step[board_idx][j] != 99) {
    if (stra_step[board_idx][j] == 88) f2 = NO_FIG;
    //Serial.print(str_board_idx(i)); Serial.print(" "); Serial.println(stra_step[i][j]);
    else if (f2 == NO_FIG) {
      f2 = board[stra_step[board_idx][j]];
      if ((f2 == NO_FIG) || 
          (is_black_fig(f2) && is_white_fig(f1)) || 
          (is_white_fig(f2) && is_black_fig(f1))) {
        pos[pos_idx].steps[pos[pos_idx].steps_count].type = MoveType::SIMPLE;
        pos[pos_idx].steps[pos[pos_idx].steps_count].c1   = board_idx;
        pos[pos_idx].steps[pos[pos_idx].steps_count].c2   = stra_step[board_idx][j];
        pos[pos_idx].steps[pos[pos_idx].steps_count].f1   = f1;
        pos[pos_idx].steps[pos[pos_idx].steps_count].f2   = f2;
        //Serial.print(str_board_idx(i));
        //Serial.print("-"); Serial.println(str_board_idx(stra_step[i][j]));
        pos[pos_idx].steps_count++;
      }
    }
    j++;
  }
}

void 
ChessEngine::add_diag_step(int pos_idx, int board_idx)
{
  signed char f1 = board[board_idx];
  signed char f2 = NO_FIG;
  int         j  = 0;

  while (diag_step[board_idx][j] != 99) {
    if (diag_step[board_idx][j] == 88) f2 = NO_FIG;
    else if (f2 == NO_FIG) {
      f2 = board[diag_step[board_idx][j]];
      if ((f2 == NO_FIG) || 
          (is_black_fig(f2) && is_white_fig(f1)) || 
          (is_white_fig(f2) && is_black_fig(f1))) {
        pos[pos_idx].steps[pos[pos_idx].steps_count].type = MoveType::SIMPLE;
        pos[pos_idx].steps[pos[pos_idx].steps_count].c1   = board_idx;
        pos[pos_idx].steps[pos[pos_idx].steps_count].c2   = diag_step[board_idx][j];
        pos[pos_idx].steps[pos[pos_idx].steps_count].f1   = f1;
        pos[pos_idx].steps[pos[pos_idx].steps_count].f2   = f2;
        pos[pos_idx].steps_count++;
      }
    }
    j++;
  }
}

void 
ChessEngine::getbm(int move_idx, std::string ep)
{
  if (ep == "0-0") {
    for (int i = 0; i < pos[0].steps_count; i++)
      if (pos[0].steps[i].type == MoveType::CASTLE_KINGSIDE)   { //
        best_move[move_idx] = pos[0].steps[i];
        return;
      }
  } 
  else if (ep == "0-0-0") {
    for (int i = 0; i < pos[0].steps_count; i++)
      if (pos[0].steps[i].type == MoveType::CASTLE_QUEENSIDE)   { //
        best_move[move_idx] = pos[0].steps[i];
        return;
      }
  } 
  else if (ep.find("=") != std::string::npos) { //
    char fi = ep.at(ep.length() - 1);
    MoveType type = MoveType::PROMOTE_TO_QUEEN;
    if      (fi == 'N') type = MoveType::PROMOTE_TO_KNIGHT;
    else if (fi == 'B') type = MoveType::PROMOTE_TO_BISHOP;
    else if (fi == 'R') type = MoveType::PROMOTE_TO_ROOK;
    for (int i = 0; i < pos[0].steps_count; i++) {
      if (pos[0].steps[i].type == type) { //
        best_move[move_idx] = pos[0].steps[i];
        return;
      }
    }
  } 
  else {
    auto posp1 = ep.find("+");
    auto posp2 = ep.find("#");
    if ((posp1 != std::string::npos) || (posp2 != std::string::npos)) {
      ep = ep.substr(0, ep.length() - 1);
    }
    char co = ep.at(ep.length() - 2);
    char ro = ep.at(ep.length() - 1);
    int  c2 = (8 * (7 - (ro - '1'))) + (co - 'a');
    char fi = ep.at(0);
    // int found=0;
    for (int i = 0; i < pos[0].steps_count; i++) {
      if (pos[0].steps[i].c2 == c2) {
        if (fig_symb1[abs(pos[0].steps[i].f1)] == fi) {
          if ((ep.length() == 3 ) || ((ep.length() == 4) && (ep.at(1) == 'x'))) {
            best_move[move_idx] = pos[0].steps[i];
            return;
          } 
          else if (int(ep.at(1)) - int('a') == column[pos[0].steps[i].c1] - 1) {
            best_move[move_idx] = pos[0].steps[i];
            return;
          }
        } 
        else if (ep.length() == 2 && abs(pos[0].steps[i].f1) == PAWN) {
          best_move[move_idx] = pos[0].steps[i];
          return;
        }
        if (step_to_str(pos[0].steps[i]) == ep) {
          best_move[move_idx] = pos[0].steps[i];
        }
        else {
          std::string st = step_to_str(pos[0].steps[i]);
          st = st.substr(0, 1) + st.substr(2);
          if ((pos[0].steps[i].f2 != NO_FIG) && (ep.at(1) == 'x') && (st == ep)) {
            best_move[move_idx] = pos[0].steps[i];
          }
        }
      }
    }
  }
}

bool 
ChessEngine::checkd_w()
{
  signed char f2 = NO_FIG;
  int         j  = 0;

  while (diag_step[idx_white_king][j] != 99) {
    if (diag_step[idx_white_king][j] == 88) f2 = NO_FIG;
    else if (f2 == NO_FIG) {
      f2 = board[diag_step[idx_white_king][j]];
      if (f2 == -BISHOP || f2 == -QUEEN) return true;
    }
    j++;
  }

  f2 = NO_FIG;
  j  = 0;

  while (stra_step[idx_white_king][j] != 99) {
    if (stra_step[idx_white_king][j] == 88) f2 = NO_FIG;
    else if (f2 == NO_FIG) {
      f2 = board[stra_step[idx_white_king][j]];
      if (f2 == -ROOK || f2 == -QUEEN) return true;
      if (j == 0 || stra_step[idx_white_king][j] == 88) {
        if (f2 == -KING) return true;
      }
    }
    j++;
  }

  return false;
}

bool 
ChessEngine::checkd_b()
{
  signed char f2 = NO_FIG;
  int         j  = 0;

  while (diag_step[idx_black_king][j] != 99) {
    if (diag_step[idx_black_king][j] == 88) f2 = NO_FIG;
    else if (f2 == NO_FIG) {
      f2 = board[diag_step[idx_black_king][j]];
      if (f2 == BISHOP || f2 == QUEEN) return true;
    }
    j++;
  }

  f2 = NO_FIG;
  j  = 0;

  while (stra_step[idx_black_king][j] != 99) {
    if (stra_step[idx_black_king][j] == 88) f2 = NO_FIG;
    else if (f2 == NO_FIG) {
      f2 = board[stra_step[idx_black_king][j]];
      if (f2 == ROOK || f2 == QUEEN) return true;
      if (j == NO_FIG || stra_step[idx_black_king][j] == 88) {
        if (f2 == KING) return true;
      }
    }
    j++;
  }

  return false;
}

void 
ChessEngine::sort_steps(int pos_idx)
{
  step_t tmp;
  for (int i = 0; i < pos[pos_idx].steps_count - 1; i++) {
    int maxweight = pos[pos_idx].steps[i].weight;
    int maxj      = i;

    for (int j = i + 1; j < pos[pos_idx].steps_count; j++)  {
      if (pos[pos_idx].steps[j].weight > maxweight) {
        maxweight = pos[pos_idx].steps[j].weight;
        maxj = j;
      }
    }

    if (maxweight == 0 && pos_idx > 0) return;
    if (maxj == i) continue;

    tmp = pos[pos_idx].steps[i];
    pos[pos_idx].steps[i]    = pos[pos_idx].steps[maxj];
    pos[pos_idx].steps[maxj] = tmp;
  }
}

void 
ChessEngine::generate_steps(int pos_idx)
{
  pos[pos_idx].cur_step = 0;
  pos[pos_idx].steps_count = 0;
  int check;
  int8_t f;

  chess_engine_task.set_pos_idx(pos_idx);
  TaskQueueData task_queue_data;
  task_queue_data.req = TaskReq::EXEC;
  QUEUE_SEND(task_queue, task_queue_data, 0);

  for (int ii = 0; ii < 64; ii++) {
    int target_idx = (pos[pos_idx].white_move) ? ii : 63 - ii;
    f = board[target_idx];
    if ((f == NO_FIG) || 
        (is_black_fig(f) &&  pos[pos_idx].white_move) || 
        (is_white_fig(f) && !pos[pos_idx].white_move)) continue;
    switch (abs(f)) {
      case KNIGHT:  add_knight_step(pos_idx, target_idx); break;
      case BISHOP:    add_diag_step(pos_idx, target_idx); break;
      case ROOK:      add_stra_step(pos_idx, target_idx); break;
      case QUEEN:     add_stra_step(pos_idx, target_idx); 
                      add_diag_step(pos_idx, target_idx); break;
      case KING: if (endgame) add_king_step(pos_idx, target_idx); break;
    }
  } //
  //int in=0;

  EngineQueueData engine_queue_data;
  QUEUE_RECEIVE(engine_queue, engine_queue_data, 0);

  //if (in) count_in++;
  //count_all++;
  if (pos[pos_idx].white_move && !pos[pos_idx].check_on_table) { //
    if (pos[pos_idx].white_castle_kingside_ok) { //
      if ((board[60] == KING  ) && 
          (board[61] == NO_FIG) && 
          (board[62] == NO_FIG) && 
          (board[63] == ROOK  )) {
        board[60]      = NO_FIG;
        board[61]      = KING;
        idx_white_king = 61;
        check          = check_on_white_king();
        board[60]      = KING;
        idx_white_king = 60;
        board[61]      = NO_FIG;

        if (!check) {
          pos[pos_idx].steps[pos[pos_idx].steps_count].type = MoveType::CASTLE_KINGSIDE;
          pos[pos_idx].steps[pos[pos_idx].steps_count].c1   =     60;
          pos[pos_idx].steps[pos[pos_idx].steps_count].c2   =     62;
          pos[pos_idx].steps[pos[pos_idx].steps_count].f1   =   KING;
          pos[pos_idx].steps[pos[pos_idx].steps_count].f2   = NO_FIG;
          pos[pos_idx].steps_count++;
        }
      }
    }
    if (pos[pos_idx].white_castle_queenside_ok) { //
      if ((board[60] == KING  ) && 
          (board[59] == NO_FIG) && 
          (board[58] == NO_FIG) && 
          (board[57] == NO_FIG) && 
          (board[56] == ROOK  )) {
        board[60]      = NO_FIG;
        board[59]      = KING;
        idx_white_king = 59;
        check          = check_on_white_king();
        board[60]      = KING;
        idx_white_king = 60;
        board[59]      = NO_FIG;

        if (!check) {
          pos[pos_idx].steps[pos[pos_idx].steps_count].type = MoveType::CASTLE_QUEENSIDE;
          pos[pos_idx].steps[pos[pos_idx].steps_count].c1   =     60;
          pos[pos_idx].steps[pos[pos_idx].steps_count].c2   =     58;
          pos[pos_idx].steps[pos[pos_idx].steps_count].f1   =   KING;
          pos[pos_idx].steps[pos[pos_idx].steps_count].f2   = NO_FIG;
          pos[pos_idx].steps_count++;
        }
      }
    }
  } 
  else if (!pos[pos_idx].white_move && !pos[pos_idx].check_on_table) { //
    if (pos[pos_idx].black_castle_kingside_ok) { //
      if ((board[4] == -KING ) && 
          (board[5] == NO_FIG) && 
          (board[6] == NO_FIG) && 
          (board[7] == -ROOK )) {
        board[4]       = NO_FIG;
        board[5]       = -KING;
        idx_black_king = 5;
        check          = check_on_black_king();
        board[4]       = -KING;
        idx_black_king = 4;
        board[5]       = NO_FIG;

        if (!check) {
          pos[pos_idx].steps[pos[pos_idx].steps_count].type = MoveType::CASTLE_KINGSIDE;
          pos[pos_idx].steps[pos[pos_idx].steps_count].c1   =      4;
          pos[pos_idx].steps[pos[pos_idx].steps_count].c2   =      6;
          pos[pos_idx].steps[pos[pos_idx].steps_count].f1   =  -KING;
          pos[pos_idx].steps[pos[pos_idx].steps_count].f2   = NO_FIG;
          pos[pos_idx].steps_count++;
        }
      }
    }
    if (pos[pos_idx].black_castle_queenside_ok) { //
      if ((board[4] == -KING ) && 
          (board[3] == NO_FIG) && 
          (board[2] == NO_FIG) && 
          (board[1] == NO_FIG) && 
          (board[0] == -ROOK )) {
        board[4]       = NO_FIG;
        board[3]       = -KING;
        idx_black_king = 3;
        check          = check_on_black_king();
        board[4]       = -KING;
        idx_black_king = 4;
        board[3]       = NO_FIG;
        if (!check) {
          pos[pos_idx].steps[pos[pos_idx].steps_count].type = MoveType::CASTLE_QUEENSIDE;
          pos[pos_idx].steps[pos[pos_idx].steps_count].c1   =      4;
          pos[pos_idx].steps[pos[pos_idx].steps_count].c2   =      2;
          pos[pos_idx].steps[pos[pos_idx].steps_count].f1   =  -KING;
          pos[pos_idx].steps[pos[pos_idx].steps_count].f2   = NO_FIG;
          pos[pos_idx].steps_count++;
        }
      }
    }
  }

  chess_engine_task.retrieve_steps(pos_idx);

  for (int i = 0; i < pos[pos_idx].steps_count; i++) {
    pos[pos_idx].steps[i].check  = CheckType::NONE;
    pos[pos_idx].steps[i].weight = abs(pos[pos_idx].steps[i].f2);
    if (pos[pos_idx].steps[i].type > MoveType::CASTLE_QUEENSIDE) {
      pos[pos_idx].steps[i].weight += fig_weight[(int)(pos[pos_idx].steps[i].type) - 2];
    }
    pos[pos_idx].steps[i].weight <<= 2;

    if (pos_idx > 0) {
      if (pos[pos_idx].best.c2 == pos[pos_idx].steps[i].c2 && 
          pos[pos_idx].best.c1 == pos[pos_idx].steps[i].c1) {
        pos[pos_idx].steps[i].weight += 5;
      }
      if (pos[pos_idx].steps[i].c2 == pos[pos_idx - 1].steps[pos[pos_idx - 1].cur_step].c2) {
        pos[pos_idx].steps[i].weight += 8;
      }
    }
    //if (l<level) {
    //if (action(pos[pos_idx].steps[i])) {
    //  pos[pos_idx].steps[i].weight=1;
    //  show_position();
    //    Serial.println("="+step_to_str(pos[pos_idx].steps[i]));
    //   delay(20000);
    //}
    //}


  } //i
  //Serial.println("TIME GENER="+std::string(micros()-sta));
  //show_steps(0);
  //delay(100000000);
  sort_steps(pos_idx);
  //halt=true;
  //show_steps(0);

}

int 
ChessEngine::evaluate(int pos_idx)
{
  if (!stats) {
    if (pos[pos_idx].white_move) return pos[pos_idx].weight_white - pos[pos_idx].weight_black;
    else return pos[pos_idx].weight_black - pos[pos_idx].weight_white;
  } 
  else {
    if (pos[pos_idx].white_move) return 5000 * (pos[pos_idx].weight_white - pos[pos_idx].weight_black + pos[pos_idx].weight_both) / (pos[pos_idx].weight_white + pos[pos_idx].weight_black + 2000);
    else return 5000 * (pos[pos_idx].weight_black - pos[pos_idx].weight_white - pos[pos_idx].weight_both) / (pos[pos_idx].weight_white + pos[pos_idx].weight_black + 2000);
  }
}

void 
ChessEngine::kingpositions()
{
  for (int i = 0; i < 64; i++) {
    if (board[i] ==  KING) idx_white_king = i;
    if (board[i] == -KING) idx_black_king = i;
  }
}

std::string 
ChessEngine::get_time(long time)
{
  std::ostringstream stream;

  stream.fill('0'); stream.width(2);

  if (time > 360000) time = 0;
  stream <<  time / 3600        << ':' 
         << (time % 3600) / 60 << ':' 
         <<  time % 60;

  return stream.str();
}

bool 
ChessEngine::draw_repeat(int pos_idx)
{
  if (pos_idx <= 12 || zero) return 0;
  for (int i = 0; i < 4; i++) {
    int li = pos_idx - i;
    if (pos[li].steps[pos[li].cur_step].c1 != pos[li - 4].steps[pos[li - 4].cur_step].c1 ||
        pos[li].steps[pos[li].cur_step].c1 != pos[li - 8].steps[pos[li - 8].cur_step].c1 ||
        pos[li].steps[pos[li].cur_step].c2 != pos[li - 4].steps[pos[li - 4].cur_step].c2 ||
        pos[li].steps[pos[li].cur_step].c2 != pos[li - 8].steps[pos[li - 8].cur_step].c2) return false;
  }
  if (TRACE > 0) std::cout << "repeat!" << std::endl;
  return true;
}

bool 
ChessEngine::is_draw()
{
  bool draw = false;
  int cn = 0, cbw = 0, cbb = 0, co = 0, cb = 0, cw = 0;
  
  for (int board_idx = 0; board_idx < 64; board_idx++) {

    if (abs(board[board_idx]) == PAWN) co++;
    if (abs(board[board_idx]) > BISHOP && abs(board[board_idx]) < KING) co++;
    if (abs(board[board_idx]) == KING) continue;
    if (abs(board[board_idx]) == KNIGHT) cn++;
    if (abs(board[board_idx]) == BISHOP && (column[board_idx] + row[board_idx]) % 2 == 0) cbb++;
    if (abs(board[board_idx]) == BISHOP && (column[board_idx] + row[board_idx]) % 2 == 1) cbw++;

    if (board[board_idx] ==  BISHOP) cw++;
    if (board[board_idx] == -BISHOP) cb++;
  }

  if (cn == 1 && co + cbb + cbw == 0) draw = true;
  if (cbb + cbw == 1 && co + cn == 0) draw = true;
  if (co + cn + cbb == 0 || co + cn + cbw == 0) draw = true;
  if (co + cn == 0 && cb == 1 && cw == 1) draw = true;

  return draw;
}

int 
active(step_t & step)  //
{
  int j;
  if (step.f2 != NO_FIG || step.type > MoveType::CASTLE_QUEENSIDE) return 1;
  if (abs(step.f2) == KING) return -1;
  switch (step.f1) {
    case PAWN:
      if (row[step.c2] > 5) return 1;
      if (((column[step.c2] > 1) && (idx_black_king == step.c2 - 9)) ||
          ((column[step.c2] < 8) && (idx_black_king == step.c2 - 7))) return 1; //
      return -1;

    case -PAWN:
      if (row[step.c2] < 4) return 1;
      if (((column[step.c2] > 1) && (idx_white_king == step.c2 + 7)) ||
          ((column[step.c2] < 8) && (idx_black_king == step.c2 + 9))) return 1; //
      return -1;

    case KNIGHT:
      j = 0;
      while (knight_step[step.c2][j] != 99) {
        if (board[knight_step[step.c2][j]] == -KING) return 1;
        j++;
      }
      return 0;

    case -KNIGHT:
      j = 0;
      while (knight_step[step.c2][j] != 99) {
        if (board[knight_step[step.c2][j]] == KING) return 1; //
        j++;
      }
      return 0;

    case BISHOP:
      if (diag1[step.c2] != diag1[idx_black_king] && diag2[step.c2] != diag2[idx_black_king]) return -1; //
      return 0;

    case -BISHOP:
      if (diag1[step.c2] != diag1[idx_white_king] && diag2[step.c2] != diag2[idx_white_king]) return -1; //
      return 0;

    case ROOK:
      if (row[step.c2] != row[idx_black_king] && column[step.c2] != column[idx_black_king]) return -1; // -
      return 0;

    case -ROOK:
      if (row[step.c2] != row[idx_white_king] && column[step.c2] != column[idx_white_king]) return -1; // -
      return 0;

    case QUEEN:
      if (diag1[step.c2] != diag1[idx_black_king] && diag2[step.c2] != diag2[idx_black_king] && //
          row[step.c2] != row[idx_black_king] && column[step.c2] != column[idx_black_king]) return -1; // -
      return 0;
    case -QUEEN:
      if (diag1[step.c2] != diag1[idx_white_king] && diag2[step.c2] != diag2[idx_white_king] && //
          row[step.c2] != row[idx_white_king] && column[step.c2] != column[idx_white_king]) return -1; // -
      return 0;
  } //switch
  return 0;
}

int 
ChessEngine::quiescence(int pos_idx, int alpha, int beta, int depth_left)
{
  if (depth_left <= 0) {
    if (pos_idx > depth) depth = pos_idx;
    return evaluate(pos_idx);
  }

  int score = -20000;
  generate_steps(pos_idx);

  if (!pos[pos_idx].check_on_table) {
    int weight = evaluate(pos_idx);
    if (weight >= score) score = weight;
    if (score > alpha) alpha = score;
    if (alpha >= beta) return alpha;
  }

  bool check, checked;
  int  act;
  
  for (int i = 0; i < pos[pos_idx].steps_count; i++) {
    act = 1;
    if (!pos[pos_idx].check_on_table) {
      act = active(pos[pos_idx].steps[i]);
      if (act == -1) continue;
    }
    move_step(pos_idx, pos[pos_idx].steps[i]);
    check = false;
    if (act == 0) {
      check = (pos[pos_idx].white_move) ? checkd_b() : checkd_w();
      pos[pos_idx].steps[i].check = check ? CheckType::CHECK : CheckType::NONE;
      if (!check) {
        back_step(pos_idx, pos[pos_idx].steps[i]);
        continue;
      }
    }
    checked = (pos[pos_idx].white_move) ? check_on_white_king() : check_on_black_king();
    if (checked) {
      back_step(pos_idx, pos[pos_idx].steps[i]);  //  -
      continue;
    }

    if (check && (depth_left == 1) && (pos_idx < MAXDEPTH - 1)) depth_left++;

    pos[pos_idx].cur_step = i;

    move_pos(pos_idx, pos[pos_idx].steps[i]);
    int tmp = -quiescence(pos_idx + 1, -beta, -alpha, depth_left - 1);
    back_step(pos_idx, pos[pos_idx].steps[i]);
    if (draw_repeat(pos_idx)) tmp = 0;
    if (tmp > score) score = tmp;
    if (score > alpha) {
      alpha = score;
      pos[pos_idx].best = pos[pos_idx].steps[i];
    }
    if (alpha >= beta ) return alpha;
  }
  if (score == -20000) {
    if (pos[pos_idx].check_on_table) {
      score = -10000 + pos_idx;
      pos[pos_idx - 1].steps[pos[pos_idx - 1].cur_step].check = CheckType::CHECKMATE;
    }
  }
  return score;
}

int 
ChessEngine::alpha_beta(int pos_idx, int alpha, int beta, int depth_left)
{
  int score = -20000, check, ext, tmp;
  if (depth_left <= 0) {
    int fd = fdepth; //4-6-8
    if ((pos_idx > 0) && pos[pos_idx - 1].steps[pos[pos_idx - 1].cur_step].f2 != NO_FIG) fd += 2;
    return quiescence(pos_idx, alpha, beta, fd);
  }
  if (pos_idx > 0) generate_steps(pos_idx);
  if ((pos_idx >= null_depth) && !zero && (depth_left > 2)) {//2
    if ((pos_idx > 0) && !pos[pos_idx].check_on_table && (pos[pos_idx - 1].steps[pos[pos_idx - 1].cur_step].f2 == NO_FIG))  {
      zero = true;
      pos[pos_idx + 1].white_castle_kingside_ok  = pos[pos_idx].white_castle_kingside_ok;
      pos[pos_idx + 1].white_castle_queenside_ok = pos[pos_idx].white_castle_queenside_ok;
      pos[pos_idx + 1].black_castle_kingside_ok  = pos[pos_idx].black_castle_kingside_ok;
      pos[pos_idx + 1].black_castle_queenside_ok = pos[pos_idx].black_castle_queenside_ok;
      pos[pos_idx + 1].weight_white = pos[pos_idx].weight_white;
      pos[pos_idx + 1].weight_black = pos[pos_idx].weight_black;
      pos[pos_idx + 1].weight_both  = pos[pos_idx].weight_both;
      pos[pos_idx + 1].en_passant_pp = 0;
      pos[pos_idx].cur_step           = MAXSTEPS;
      pos[pos_idx].steps[MAXSTEPS].f2 = NO_FIG;
      int tmpz = -alpha_beta(pos_idx + 1, -beta, -beta + 1, depth_left - 3);
      zero = false;
      if (tmpz >= beta) return beta;
    }
  }
  if ((pos_idx > 4)                && 
      !zero                        && 
      (depth_left <= 2)            && 
      futility                     && 
      !pos[pos_idx].check_on_table && 
      (pos[pos_idx - 1].steps[pos[pos_idx - 1].cur_step].f2 == 0)) { //futility pruning
    int weight = evaluate(pos_idx);
    if (weight - 200 >= beta) return beta;
  }
  for (int i = 0; i < pos[pos_idx].steps_count; i++) {
    ext = 0;
    if (pos_idx == 0) {
      depth = depth_left;
      if (level < 7) if (pos[0].steps[pos[0].cur_step].check != CheckType::NONE) ext = 2;
    }
    move_step(pos_idx, pos[pos_idx].steps[i]);
    if (pos[pos_idx].white_move) check = check_on_white_king();
    else check = check_on_black_king();

    if (check) {
      back_step(pos_idx, pos[pos_idx].steps[i]);
      continue;
    }

    pos[pos_idx].cur_step = i;
    move_pos(pos_idx, pos[pos_idx].steps[i]);

    if (TRACE > 0) {
      if (pos_idx == 0) {
        std::cout << step_to_str(pos[0].steps[i]) << "  " << i + 1 << '/' << pos[0].steps_count;
        //if (pos[0].steps[i].weight<-9000) { Serial.println(F(" checkmate")); continue; }
      } 
      else if (TRACE > pos_idx) {
        std::cout << std::endl;
        for (int ll = 0; ll < pos_idx; ll++) std::cout << "      ";
        std::cout << pos_idx + 1 << "- " << step_to_str(pos[pos_idx].steps[i]);
      }
    } //TRACE

    if ((pos_idx > 2) && !lazy && !zero && lazy_eval && pos[pos_idx].steps[i].f2 != NO_FIG && 
        (pos[0].steps[pos[0].cur_step].check == CheckType::NONE) && 
        (evaluate(pos_idx + 1) + 100 <= alpha) &&
        (( pos[pos_idx].white_move && !check_on_black_king()) ||
         (!pos[pos_idx].white_move && !check_on_white_king()))) {
      lazy = true;
      if (-alpha_beta(pos_idx + 1, -beta, -alpha, depth_left - 3) <= alpha) tmp = alpha;
      else {
        lazy = false;
        tmp = -alpha_beta(pos_idx + 1, -beta, -alpha, depth_left - 1 + ext);
      }
      lazy = false;
    } 
    else tmp = -alpha_beta(pos_idx + 1, -beta, -alpha, depth_left - 1 + ext);

    back_step(pos_idx, pos[pos_idx].steps[i]);
    if (draw_repeat(pos_idx)) tmp = 0;
    if (tmp > score) score = tmp;
    pos[pos_idx].steps[i].weight = tmp;

    if (score > alpha) {
      alpha = score;
      pos[pos_idx].best = pos[pos_idx].steps[i];
      if (pos_idx == 0 && level > 3) {
        if (print_best(depth_left)) return alpha;
      }
    }

    if (pos_idx == 0) {
      if (TRACE > 0) std::cout << "        " << tmp << std::endl;
      //if (alpha==9999||alpha==-5000) break;
    } 
    else if (TRACE > pos_idx) {
      std::cout << " = " << tmp;
    }

    if (alpha >= beta) return alpha;

    auto end_time = std::chrono::steady_clock::now();
    unsigned long duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (halt || (pos_idx < 3 && duration > time_limit)) //
      return score;
  }
  if (score == -20000) {
    if ((pos_idx > 0) && pos[pos_idx].check_on_table) {
      score = -10000 + pos_idx;
      pos[pos_idx - 1].steps[pos[pos_idx - 1].cur_step].check = CheckType::CHECKMATE;
    } 
    else score = 0;
  }
  return score;
}

bool 
ChessEngine::print_best(int dep)
{
  auto end_time = std::chrono::steady_clock::now();
  unsigned long duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

  if (halt || (duration > time_limit)) return false;
  
  if (last_best_depth == dep && 
      pos[0].best.type == last_best_step.type &&
      pos[0].best.c1 == last_best_step.c1 && pos[0].best.c2 == last_best_step.c2) return false;
  bool ret = false;
  if (pos[0].best.type == last_best_step.type && pos[0].best.c1 == last_best_step.c1 && pos[0].best.c2 == last_best_step.c2) {
    for (int i = 0; i < MAXEPD; i++) {
      if (last_best_step.c2 == best_move[i].c2 && last_best_step.c1 == best_move[i].c1 && last_best_step.type == best_move[i].type) {
        ret         = true;
        best_solved = true;
        break;
      }
    }
  }
  last_best_depth = dep;
  last_best_step = pos[0].best;
  std::string st = step_to_str(pos[0].best);
  std::cout << (pos[0].white_move ? "1." : "1...") << st;

  for (std::size_t i = 0; i < 10 - st.length(); i++) std::cout << ' ';
  std::string depf = "/" + std::to_string(depth + 1) + " ";

  end_time = std::chrono::steady_clock::now();
  duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();

  std::cout << "(";

  std::string wei;
  if (pos[0].best.weight > 9000) {
    std::cout << "+M" << ((10001 - pos[0].best.weight) / 2);
  }
  else {
    std::cout << std::setprecision(2) << (pos[0].best.weight / 100.);
  }

  std::cout << ") Depth: " << dep << depf << get_time(duration) << " " << (move_count / 1000) << "kN" << std::endl;
  return ret;
}

bool 
ChessEngine::solve_step()
{
  int  score;
  bool solved = false;

  move_count  = 0;
  count_in    = 0;
  count_all   = 0;
  zero        = false;
  lazy        = false;

  for (int i = 1; i < MAXDEPTH; i++) {
    if (i % 2) pos[i].white_move = !pos[0].white_move;
    else pos[i].white_move = pos[0].white_move;
    pos[i].en_passant_pp = 0;
  }

  start_time = std::chrono::steady_clock::now();

  if (is_draw()) {
    std::cout << " DRAW!" << std::endl;  //
    return true;
  }

  last_best_depth         = -1;
  last_best_step.type     =  MoveType::SIMPLE;
  last_best_step.c1       = -1;
  last_best_step.c2       = -1;
  best_solved             =  false;

  pos[0].weight_black     = 0;
  pos[0].weight_white     = 0;

  for (int i = 0; i < 64; i++) { //
    if (board[i] < 0) {
      pos[0].weight_black += fig_weight[-board[i]];
    } 
    else if (board[i] > 0) {
      pos[0].weight_white += fig_weight[board[i]]; //   8000
    }
  }

  if (pos[0].weight_white + pos[0].weight_black < 3500) endgame = true;
  else endgame = false;  //3500?

  //Serial.println("endgame="+std::string(endgame));

  pos[0].weight_both = 0;

  for (int i = 0; i < 64; i++) { //
    int f = board[i];

    if (f == NO_FIG) continue;
    if (abs(f) == KING && endgame) {
      if (is_black_fig(f))
        pos[0].weight_both -= stat_weight_black[6][i];
      else
        pos[0].weight_both += stat_weight_white[6][i];
    } 
    else {
      if (is_black_fig(f))
        pos[0].weight_both -= stat_weight_black[-f - 1][i];
      else
        pos[0].weight_both += stat_weight_white[f - 1][i];
    }
  }

  kingpositions();

  if (TRACE > 0) std::cout << " start score=" << evaluate(0) << std::endl;

  generate_steps(0);

  int  legal = 0;
  bool check;
  int  samebest = 0;

  for (int i = 0; i < pos[0].steps_count; i++) { //
    move_step(0, pos[0].steps[i]);

    if (pos[0].white_move) check = check_on_white_king();
    else check = check_on_black_king();

    pos[0].check_on_table = check;

    if (!check) legal++;
    if (!check) pos[0].steps[i].weight = 0;
    else pos[0].steps[i].weight = -30000;

    back_step(0, pos[0].steps[i]);
  }

  if (legal == 0) {
    std::cout << ((pos[0].check_on_table) ? " CHECKMATE!" : " PAT!") << std::endl;
    return true;
  }

  sort_steps(0);
  pos[0].steps_count = legal;

  int alpha = -20000;
  int beta  =  20000;

  level = (time_limit > 300000) ? 4 : 2;

  for (int x = 0; x < MAXDEPTH; x++) {
    pos[x].best.f1 =  NO_FIG;
    pos[x].best.c2 = -1;
  }

  stats = true;

  while (level <= 20) {
    if (TRACE > 0) {
      std::cout << "******* LEVEL=" << level << std::endl;
    }

    for (int x = 1; x < MAXDEPTH; x++) {
      pos[x].best.f1 =  NO_FIG;
      pos[x].best.c2 = -1;
    }

    for (int i = 0; i < pos[0].steps_count; i++) {
      move_step(0, pos[0].steps[i]);
      if (pos[0].white_move) pos[0].steps[i].check = check_on_black_king() ? CheckType::CHECK : CheckType::NONE;
      else pos[0].steps[i].check = check_on_white_king() ? CheckType::CHECK : CheckType::NONE;

      pos[0].steps[i].weight += evaluate(0) + ((int)(pos[0].steps[i].check)) * 500;

      if (pos[0].steps[i].f2 != NO_FIG) pos[0].steps[i].weight -= pos[0].steps[i].f1;
      back_step(0, pos[0].steps[i]);
    }

    pos[0].steps[0].weight += 10000; // -
    sort_steps(0);
    for (int i = 0; i < pos[0].steps_count; i++) pos[0].steps[i].weight = -8000;

    if (null_move) null_depth = 3;
    else  null_depth = 93;
    //beta=10000; alpha=9900;
    //int sec=(millis()-start_time)/1000;
    fdepth = 4;
    score  = alpha_beta(0, alpha, beta, level);

    auto end_time = std::chrono::steady_clock::now();
    unsigned long duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    bool out = 0;
    if (score >= beta) out = 1;

    if (multi_pov || samebest > 2 || out) {
      samebest = 0;
      alpha = -20000;
      beta  =  20000;
    } 
    else {
      alpha = score - 100;
      beta  = score + 100;
    }

    if ((duration > (time_limit * 0.2)) && !out) {
      stats = false;
      alpha = score - 300;
      beta  = score + 300;
    }

    sort_steps(0);    //
    if (print_best(level) || best_solved || score > 9900) {
      solved = true;
      break;
    }
    if (duration > time_limit || halt) break;
    if (pos[0].best.type == last_best_step.type && pos[0].best.c1 == last_best_step.c1 && pos[0].best.c2 == last_best_step.c2) {
      samebest++;
    } 
    else samebest = 0;

    level++;

    //Serial.println(level);
    //Serial.println(duration/1000);
  } //while level
  //Serial.println(std::string(count_in)+"/"+std::string(count_all));
  //Serial.println("Task load: "+std::string(0.1*task_execute/(millis()-start_time))+"%");
  return solved;
}

bool 
ChessEngine::load_board_from_fen(std::string str)
{
  int8_t col  = 0;
  int8_t row  = 0;
  bool   load = false;

  std::memset(board, 0, sizeof(Board));

  pos[0].white_move                = true;
  pos[0].white_castle_kingside_ok  = false;
  pos[0].white_castle_queenside_ok = false;
  pos[0].black_castle_kingside_ok  = false;
  pos[0].black_castle_queenside_ok = false;
  pos[0].en_passant_pp                        = 0;
  pos[0].cur_step                  = 0;
  pos[0].steps_count               = 0;

  int spaces = 0;

  for (std::size_t ss_idx = 0; ss_idx < str.length(); ss_idx++) {
    char ch = str[ss_idx];
    if (col > 7) {
      col = 0;
      row++;
    }
    if ((spaces == 3) && (ch >= 'a') && (ch <= 'h')) {
      char ch1 = str[++ss_idx];
      if ((ch1 >= '1') && (ch1 <= '8')) {
        pos[0].en_passant_pp = (8 * (7 - (ch1 - '1'))) + (ch - 'a');
      }
    } 
    else {
      int board_idx = row * 8 + col;
      switch (ch) {
        case '/': col = 0;                           break;
        case 'p': board[board_idx] = -PAWN;   col++; break;
        case 'P': board[board_idx] =  PAWN;   col++; break;
        case 'n': board[board_idx] = -KNIGHT; col++; break;
        case 'N': board[board_idx] =  KNIGHT; col++; break;
        case 'b': if      (spaces == 0) { board[board_idx] = -BISHOP; col++;      } 
                  else if (spaces == 1) { pos[0].white_move = false; load = true; } 
                  break;
        case 'B': board[board_idx] =  BISHOP; col++; break;
        case 'r': board[board_idx] = -ROOK;   col++; break;
        case 'R': board[board_idx] =  ROOK;   col++; break;
        case 'q': if (spaces == 0) { board[board_idx] = -QUEEN; col++; } 
                  else pos[0].black_castle_queenside_ok = true; 
                  break;
        case 'Q': if (spaces == 0) { board[board_idx] =  QUEEN; col++; } 
                  else pos[0].white_castle_queenside_ok = true; 
                  break;
        case 'k': if (spaces == 0) { board[board_idx] = -KING;  col++; } 
                  else pos[0].black_castle_kingside_ok  = true; 
                  break;
        case 'K': if (spaces == 0) { board[board_idx] =  KING;  col++; } 
                  else pos[0].white_castle_kingside_ok  = true; 
                  break;
        case '1': col++;           break;
        case '2': col += 2;        break;
        case '3': col += 3;        break;
        case '4': col += 4;        break;
        case '5': col += 5;        break;
        case '6': col += 6;        break;
        case '7': col += 7;        break;
        case '8': col  = 0; row++; break;
        case ' ': spaces++;        break;
        case 'w': if (spaces == 1) { pos[0].white_move = true; load = true; } break;
      }
    }
    if (spaces == 4) break;
  }

  return load;
}

std::string 
ChessEngine::export_pos_to_fen(int pos_idx)
{
  std::ostringstream stream;
  for (int row = 0; row < 8; row++) {

    if (row > 0) stream << "/";
    int empty = 0;
    for (int col = 0; col < 8; col++) {

      int f = board[col + row * 8];

      if (f == NO_FIG) empty++;

      if (f != NO_FIG || col == 7) {
        if (empty > 0) {
          stream << empty;
          empty = 0;
        }
      }

      switch (f) {
        case  PAWN:   stream << "P"; break;
        case -PAWN:   stream << "p"; break;
        case  KNIGHT: stream << "N"; break;
        case -KNIGHT: stream << "n"; break;
        case  BISHOP: stream << "B"; break;
        case -BISHOP: stream << "b"; break;
        case  ROOK:   stream << "R"; break;
        case -ROOK:   stream << "r"; break;
        case  QUEEN:  stream << "Q"; break;
        case -QUEEN:  stream << "q"; break;
        case  KING:   stream << "K"; break;
        case -KING:   stream << "k"; break;
      }
    }
  }
  
  stream << ((pos[pos_idx].white_move) ? " w " : " b ");

  if (!(pos[pos_idx].white_castle_kingside_ok  || 
        pos[pos_idx].white_castle_queenside_ok || 
        pos[pos_idx].black_castle_kingside_ok  || 
        pos[pos_idx].black_castle_queenside_ok)) stream << "-";
  else {
    if (pos[pos_idx].white_castle_kingside_ok ) stream << "K";
    if (pos[pos_idx].white_castle_queenside_ok) stream << "Q";
    if (pos[pos_idx].black_castle_kingside_ok ) stream << "k";
    if (pos[pos_idx].black_castle_queenside_ok) stream << "q";
  }
  if (pos[pos_idx].en_passant_pp != 0) {
    stream << " " << board_idx_to_str(pos[pos_idx].en_passant_pp);
  }
  else stream << " -";

  return stream.str();
}

std::string 
ChessEngine::step_to_str(const step_t & step)
{
  std::ostringstream stream;

  if (step.f1 == NO_FIG) return stream.str();
  if      (step.type == MoveType::CASTLE_KINGSIDE ) stream << "0-0";
  else if (step.type == MoveType::CASTLE_QUEENSIDE) stream << "0-0-0";
  else  {
    if (abs(step.f1) > PAWN) stream << fig_symb[abs(step.f1)];
    stream << board_idx_to_str(step.c1);
    stream << ((step.f2 == NO_FIG) ? "-" : "x");
    stream << board_idx_to_str(step.c2);
  }
  if (step.type > MoveType::CASTLE_QUEENSIDE ) stream << "=" << fig_symb[(int)step.type - 2];
  if      (step.check == CheckType::CHECK    ) stream << "+";
  else if (step.check == CheckType::CHECKMATE) stream << "#";

  return stream.str();
}

Board * 
ChessEngine::get_board()
{
  return &board;
}

std::string 
ChessEngine::board_idx_to_str(int board_idx)
{
  char buf[2];
  buf[0] = 'a' + (board_idx % 8);
  buf[1] = '1' + (7 - (board_idx / 8));
  return std::string(buf, 2);
}

position_t * 
ChessEngine::get_pos(int pos_idx)
{ 
  return &pos[pos_idx]; 
}

step_t * 
ChessEngine::get_best_move(int move_idx) 
{ 
  return &best_move[move_idx]; 
}

static void
chess_task_start()
{
  chess_engine_task.exec();
}

void
ChessEngine::setup()
{ 
  #if CHESS_LINUX_BUILD
    mq_unlink("/chess_task");
    mq_unlink("/chess_engine");

    task_queue      = mq_open("/chess_task",      O_RDWR|O_CREAT, S_IRWXU, &task_attr);
    if (task_queue == -1) { std::cerr << "Unable to open task_queue:" << errno << std::endl; return; }

    engine_queue    = mq_open("/chess_engine",    O_RDWR|O_CREAT, S_IRWXU, &engine_attr);
    if (engine_queue == -1) { std::cerr << "Unable to open engine_queue:" << errno << std::endl; return; }

    chess_task = std::thread(chess_task_start); 
  #else
    task_queue      = xQueueCreate(5, sizeof(TaskQueueData));
    engine_queue    = xQueueCreate(5, sizeof(EngineQueueData));

    auto cfg = create_config("chessTask", 0, 10 * 1024, configMAX_PRIORITIES - 2);
    cfg.inherit_cfg = true;
    esp_pthread_set_cfg(&cfg);
    chess_task = std::thread(chess_task_start);
  #endif
}
