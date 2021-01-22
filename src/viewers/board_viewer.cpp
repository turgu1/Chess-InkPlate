// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#define __BOARD_VIEWER__ 1
#include "controllers/board_controller.hpp"
#include "viewers/board_viewer.hpp"

#include "models/ttf2.hpp"
#include "models/config.hpp"
#include "viewers/msg_viewer.hpp"

#include "chess_engine.hpp"

#if EPUB_INKPLATE_BUILD
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
BoardViewer::show_cursor(bool play_white, Dim dim, Pos cursor_pos, Page::Format & fmt, bool bold)
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
                        Step *    steps, 
                        int         step_count, 
                        std::string msg)
{
  page.set_compute_mode(Page::ComputeMode::DISPLAY);

  int8_t font_index;
  config.get(Config::Ident::DEFAULT_FONT, &font_index);
  if ((font_index < 0) || (font_index > 7)) font_index = 0;

  font_index += 5;

  Page::Format fmt = {
    .line_height_factor =   1.0,
    .font_index         =     0,
    .font_size          =    25,
    .indent             =     0,
    .margin_left        =     5,
    .margin_right       =    10,
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

  page.start(fmt);

  Board      * board;

  board = chess_engine.get_board();

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

  Dim dim = page.add_text_raw(stream.str(), fmt);

  if (cursor_pos.x >= 0) show_cursor(play_white, dim, cursor_pos, fmt, true);
  if ((from_pos.x  >= 0) && (memcmp(&from_pos, &cursor_pos, sizeof(Pos)) != 0)) {
    show_cursor(play_white, dim, from_pos, fmt, false);
  }

  if (!msg.empty()) {
    fmt.font_index =  1;
    fmt.font_size  = 12;

    Pos pos;
    pos.x = fmt.margin_left + fmt.screen_left + dim.width;
    pos.y = fmt.margin_top  + fmt.screen_top  + 30;

    page.put_str_at(msg, pos, fmt);
  }

  if (step_count > 0) {
    stream.str(""); stream.clear();

    for (int i = 0; i < step_count; i++) {
      if ((i & 1) == 0) stream << ((i / 2) + 1) << '.';
      stream << chess_engine.Stepo_str(steps[i]) << ' ';
    }

    fmt.font_index  =  1;
    fmt.font_size   = 10;
    fmt.margin_left =  5 + (9 * dim.width) + 20;
    fmt.margin_top  =  6 + dim.height;

    page.set_limits(fmt);

    page.new_paragraph(fmt);
    page.add_text(stream.str(), fmt);
    page.end_paragraph(fmt);
  }

  page.paint();
}