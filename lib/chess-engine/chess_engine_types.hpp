#pragma once

#include <cinttypes>

const int MAXSTEPS = 150;
const int MAXDEPTH =  30;
const int MAXEPD   =   5;

// Figures

const int8_t NO_FIG = 0;
const int8_t PAWN   = 1;
const int8_t KNIGHT = 2;
const int8_t BISHOP = 3;
const int8_t ROOK   = 4;
const int8_t QUEEN  = 5;
const int8_t KING   = 6;

enum class CheckType : int8_t { NONE, CHECK, CHECKMATE };
enum class MoveType  : int8_t { UNKNOWN = -1, SIMPLE, EN_PASSANT, CASTLE_KINGSIDE, CASTLE_QUEENSIDE, 
                                PROMOTE_TO_KNIGHT, PROMOTE_TO_BISHOP, PROMOTE_TO_ROOK, PROMOTE_TO_QUEEN };

const int fig_weight[] = { 0, 100, 320, 330, 500, 900, 0 };
const char  fig_symb[] = "  NBRQK";
const char fig_symb1[] = " pNBRQK";

typedef int8_t Board[64];

struct Step {
  int8_t      f1, f2; // Figure located at c1 (f1) and c2 (f2)
  signed char c1, c2; // Board location for the move, from c1 to c2
  CheckType   check;  // Indicates what king of check occurs with this step
  MoveType    type;   // Kind of move
  short       weight; // Step value
};

struct Position {
  bool    white_move;
  bool    white_castle_kingside_ok, 
          white_castle_queenside_ok, 
          black_castle_kingside_ok, 
          black_castle_queenside_ok;
  uint8_t en_passant_pp;         // En Passant Pawn Position on board, if valid
  Step    steps[MAXSTEPS + 1];
  int     steps_count;
  int     cur_step;
  Step    best;
  bool    check_on_table;
  short   weight_white;
  short   weight_black;
  short   weight_both;
};
