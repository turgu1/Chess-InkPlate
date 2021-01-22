#pragma once

#include <cinttypes>
#include <string>
#include <thread>

#if CHESS_LINUX_BUILD
  #include <mqueue.h>
#else
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
  
  // #include "freertos/semphr.h"
#endif

#include "chess_engine_types.hpp"

class ChessTask 
{
  public:
    ChessTask() { }

    void exec();

    inline void set_pos_idx(int pos_idx) { task_pos_idx = pos_idx; }
    void     retrieve_steps(int pos_idx);

  private:

    int                task_pos_idx;

    step_t steps[MAXSTEPS]; 
    int    steps_count;

    void  add_one_step(int c1, int c2);
    void add_king_step(int8_t board_idx);

};

#if CHESS_ENGINE
  ChessTask   chess_engine_task;
#else
  extern ChessTask   chess_engine_task;
#endif

class ChessEngine
{
  public:

    ChessEngine() : 
              TRACE(0),
         time_limit(1 * 60 * 1000),
        best_solved(false),
               zero(false),
              level(2),
              stats(true), 
         move_count(0),
           count_in(0),
          count_all(0),
          null_move(false),
          multi_pov(false),
           futility(true),
          lazy_eval(true),
             fdepth(4),
              depth(0),
         null_depth(0),
               lazy(false),
    last_best_depth(0),
               halt(false),
            endgame(false) { }

    void                      setup();
    void             generate_steps(int pos_idx);

    bool        load_board_from_fen(std::string str);
    std::string   export_pos_to_fen(int pos_idx);

    bool                 solve_step();

    void                  back_step(int pos_idx, step_t & step);
    void                  move_step(int pos_idx, step_t & step);
    void                   move_pos(int pos_idx, step_t & step);

    Board               * get_board();

    position_t            * get_pos(int pos_idx);
    step_t          * get_best_move(int move_idx);

    std::string         step_to_str(const step_t & step);
    std::string    board_idx_to_str(int board_idx);

    void                      getbm(int n, std::string ep);

    inline bool is_black_fig(int8_t fig) const { return fig < 0; }
    inline bool is_white_fig(int8_t fig) const { return fig > 0; }

  private:
    int TRACE;

    std::thread chess_task;

    bool      print_best(int dep);
    bool        checkd_w();
    bool        checkd_b();
    bool     draw_repeat(int pos_idx);
    int       quiescence(int pos_idx, int alpha, int beta, int depth_left);
    int       alpha_beta(int pos_idx, int alpha, int beta, int depth_left);
    int         evaluate(int pos_idx);
    void   kingpositions();
    bool         is_draw();
    void      sort_steps(int pos_idx);
    void   add_king_step(int pos_idx, int board_idx);
    void add_knight_step(int pos_idx, int board_idx);
    void   add_stra_step(int pos_idx, int board_idx);
    void   add_diag_step(int pos_idx, int board_idx);

    std::string get_time(long tim);

    unsigned long time_limit;
    std::chrono::time_point<std::chrono::steady_clock> start_time;

    bool   best_solved;
    bool   zero;
    int    level;

    bool   stats;
    int    move_count;
    int    count_in;
    int    count_all;

    bool   null_move;
    bool   multi_pov;
    bool   futility;
    bool   lazy_eval;

    int    fdepth;

    int    depth;
    int    null_depth;
    bool   lazy;
    int    last_best_depth;

    bool   halt;
    bool   endgame;

    step_t last_best_step;
    step_t best_move[MAXEPD];

};

#if CHESS_ENGINE
  ChessEngine chess_engine;
#else
  extern ChessEngine chess_engine;
#endif