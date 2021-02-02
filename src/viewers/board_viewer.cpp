// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOARD_VIEWER__ 1
#include "controllers/game_controller.hpp"
#include "viewers/board_viewer.hpp"

#include "models/ttf2.hpp"
#include "models/config.hpp"
#include "viewers/msg_viewer.hpp"

#include "chess_engine.hpp"

#if CHESS_INKPLATE_BUILD
  #include "viewers/battery_viewer.hpp"
#endif

#include "screen.hpp"
#include "alloc.hpp"

#include <iomanip>
#include <cstring>
#include <iostream>
#include <sstream>

#include "logging.hpp"

constexpr char white_on_white[] = " pnbrqk";
constexpr char white_on_black[] = "+PNBRQK";
constexpr char black_on_white[] = " omvtwl";
constexpr char black_on_black[] = "+OMVTWL";

constexpr char col_nbr[] = {
  '\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357', '\0'
};

constexpr char row_nbr[] = {
  '\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347'
};

void
BoardViewer::show_cursor(bool play_white, 
                         Dim dim, 
                         Pos cursor_pos, 
                         Page::Format & fmt, 
                         bool bold)
{
  Pos pos;

  if (play_white) {
    pos.x = fmt.margin_left + fmt.screen_left + (dim.width  * (     cursor_pos.x  + 1));
    pos.y = fmt.margin_top  + fmt.screen_top  + (dim.height * ((7 - cursor_pos.y) + 1));
  }
  else {
    pos.x = fmt.margin_left + fmt.screen_left + (dim.width  * ((7 - cursor_pos.x) + 1));
    pos.y = fmt.margin_top  + fmt.screen_top  + (dim.height * (     cursor_pos.y  + 1));
  }

  page.put_highlight(dim, pos);

  dim.width  -= 2; dim.height -= 2;
  pos.x      += 1; pos.y      += 1;

  page.put_highlight(dim, pos);

  if (bold) {
    dim.width  -= 2; dim.height -= 2;
    pos.x      += 1; pos.y      += 1;

    page.put_highlight(dim, pos);
  }
}

void
BoardViewer::show_board(bool        play_white, 
                        Pos         cursor_pos, 
                        Pos         from_pos, 
                        Step *      steps, 
                        int         step_count, 
                        std::string msg)
{
  Board * board = chess_engine.get_board();

  std::ostringstream stream;

  stream << "!\"\"\"\"\"\"\"\"#" << std::endl;
  
  bool white_pos = true;
  int  board_idx;

  if (play_white) {

    board_idx = 0;

    for (int row = 7; row >= 0; row--) {

      stream << row_nbr[row];

      for (int col = 0; col < 8; col++) {
        signed char f = (*board)[board_idx];

        if (f >= 0) stream << ((white_pos) ? white_on_white[ f] : white_on_black[ f]);
        else        stream << ((white_pos) ? black_on_white[-f] : black_on_black[-f]);
      
        board_idx++;
        white_pos = !white_pos;
      }

      stream << '%' << std::endl;
      white_pos = !white_pos;
    }
    stream << '/' << col_nbr << ')';
  }
  else {

    for (int row = 0; row < 8; row++) {

      stream << row_nbr[row];

      for (int col = 7; col >= 0; col--) {
        board_idx = ((7 - row) * 8) + col;

        signed char f = (*board)[board_idx];

        if (f >= 0) stream << ((white_pos) ? white_on_white[ f] : white_on_black[ f]); 
        else        stream << ((white_pos) ? black_on_white[-f] : black_on_black[-f]);

        white_pos = !white_pos;
      }

      stream << '%' << std::endl;
      white_pos = !white_pos;
    }

    stream << '/';
    for (int col = 7; col >= 0; col--) stream << col_nbr[col];
    stream << ')';
  }

  int8_t font_index;
  config.get(Config::Ident::DEFAULT_FONT, &font_index);
  if ((font_index < 0) || (font_index > 7)) font_index = 0;

  font_index += 5;

  Page::Format fmt = {
    .line_height_factor =   1.0,
    .font_index         =     0,
    .font_size          = CHESS_BOARD_FONT_SIZE,
    .indent             =     0,
    .margin_left        =     0,
    .margin_right       =     5,
    .margin_top         =     5,
    .margin_bottom      =    10,
    .screen_left        =     0,
    .screen_right       =     0,
    .screen_top         =     0,
    .screen_bottom      =     0,
    .width              =     0,
    .height             =     0,
    .trim               = false,
    .pre                = false,
    .font_style         = Fonts::FaceStyle::NORMAL,
    .align              = Page::Align::LEFT,
    .text_transform     = Page::TextTransform::NONE
  };

  fmt.font_index = font_index;

  TTF * font = fonts.get(font_index);
  int16_t chess_cell_height = font->get_em_height(CHESS_BOARD_FONT_SIZE);

  page.set_compute_mode(Page::ComputeMode::DISPLAY);
  page.start(fmt);

  Dim dim = page.add_text_raw(stream.str(), fmt);

  if (cursor_pos.x >= 0) show_cursor(play_white, dim, cursor_pos, fmt, true);
  if ((from_pos.x  >= 0) && (memcmp(&from_pos, &cursor_pos, sizeof(Pos)) != 0)) {
    show_cursor(play_white, dim, from_pos, fmt, false);
  }

  if (!msg.empty()) {
    fmt.font_index =  1;
    fmt.font_size  = CHESS_STEPS_FONT_SIZE;

    TTF * font = fonts.get(1);
    

    Pos pos;
    pos.x = fmt.margin_left + fmt.screen_left + dim.width;
    pos.y = fmt.margin_top  + fmt.screen_top  + (chess_cell_height / 5) + font->get_em_height(CHESS_STEPS_FONT_SIZE);

    page.put_str_at(msg, pos, fmt);
  }

  if (step_count > 0) {
    stream.str(""); stream.clear();

    int first = 0;
    if (step_count >= 50) {
      first = step_count - ((step_count % 50) + 25);
    }
    for (int i = first; i < step_count; i++) {
      if ((i & 1) == 0) stream << ((i / 2) + 1) << '.';
      stream << chess_engine.step_to_str(steps[i]) << ' ';
    }

    if (steps[step_count-1].check == CheckType::CHECKMATE) {
      stream <<  ((step_count & 1) ? " 1-0" : " 0-1"); 
    }

    fmt.font_index  =  1;
    fmt.font_size   = CHESS_STEPS_FONT_SIZE;
    fmt.margin_left =  5 + (9 * dim.width ) + 15;
    fmt.margin_top  =  6 +      dim.height  - 20;

    page.set_limits(fmt);

    page.new_paragraph(fmt);
    page.add_text(stream.str(), fmt);
    page.end_paragraph(fmt);
  }

  #if CHESS_INKPLATE_BUILD
    int8_t show_heap;
    config.get(Config::Ident::SHOW_HEAP, &show_heap);

    if (show_heap != 0) {     
      stream.str(std::string());
      stream << heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) 
            << " / " 
            << heap_caps_get_free_size(MALLOC_CAP_8BIT);

      fmt.font_index  =  1;
      fmt.font_size   =  9;
      fmt.align = Page::Align::RIGHT;

      TTF * font = fonts.get(1);
      page.put_str_at(stream.str(), Pos(-1, Screen::HEIGHT + font->get_descender_height(9) - 2), fmt);
    }

    BatteryViewer::show();
  #endif


  page.paint();
}