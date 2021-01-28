// Copyright (c) 2021 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#pragma once

#include <cstdint>
#include <iostream>
#include <unordered_map>
#include <string>

#include "strlcpy.hpp"

#define APP_VERSION "1.1.0"

#if !(defined(CHESS_LINUX_BUILD) || defined(CHESS_INKPLATE_BUILD)) 
  #error "BUILD_TYPE Not Set."
#else
  #if !(CHESS_LINUX_BUILD || CHESS_INKPLATE_BUILD)
    #error "One of CHESS_LINUX_BUILD or CHESS_INKPLATE_BUILD must be set to 1"
  #endif
#endif

#if CHESS_LINUX_BUILD
  #define MAIN_FOLDER "/home/turgu1/Dev/Chess-InkPlate/SDCard"
#else
  #define MAIN_FOLDER "/sdcard"
#endif

#define FONTS_FOLDER MAIN_FOLDER "/fonts"
#define BOOKS_FOLDER MAIN_FOLDER "/books"

#ifndef DEBUGGING
  #define DEBUGGING 0
#endif

// Debugging aid
//
// These are simple tools to show information about formatting for a specific page
// in a book being tested. The problems are often reflected through bad offsets
// computation for the beginning or/and end of pages.
//
// To get good results for the user, pages location offsets computed and put in the books 
// database must be the same as the one found when the book is being displayed. 
// If not, lines at the beginning or the end of a page can be duplicated or 
// part of a paragraph can be lost.
//
// To use, set DEBUGGING_AID to 1 and select the page number (0 = first page), 
// setting the PAGE_TO_SHOW_LOCATION. At
// pages location refresh time (done by the book_dir class at start time), 
// a lot of data will be output on the terminal. Then, opening the book,
// and going to the selected page, some similar information will be sent to 
// the terminal. You can then compare the results and find the differences.
//
// To be used, only one book must be in the books folder and the books_dir.db 
// file must be deleted before any trial.

#define DEBUGGING_AID  0   ///< 1: Allow for specific page debugging output

#if DEBUGGING_AID
  #define PAGE_TO_SHOW_LOCATION 2
  #define SET_PAGE_TO_SHOW(val) { show_location = val == PAGE_TO_SHOW_LOCATION; }
  #define SHOW_LOCATION(msg) { if (show_location) { \
    std::cout << msg << " Offset:" << current_offset << " "; \
    page.show_controls("  "); \
    std::cout << "     "; \
    page.show_fmt(fmt, "  ");  }}

  #if __GLOBAL__
    bool show_location = false;
  #else
    extern bool show_location;
  #endif
#else
  #define SET_PAGE_TO_SHOW(val)
  #define SHOW_LOCATION(msg)
#endif

// The following data definitions are here for laziness... 
// ToDo: Move them to the appropriate location

struct Dim { 
  uint16_t width; 
  uint16_t height; 
  Dim(uint16_t w, uint16_t h) { 
    width = w; 
    height = h;
  }
  Dim() {}
};

struct Pos { 
  int16_t x; 
  int16_t y; 
  Pos(int16_t xpos, int16_t ypos) { 
    x = xpos; y = ypos; 
  }
  Pos() {}
};

// BREAK is BR... A defined BR in esp-idf is the cause of this!!
enum class Element { BODY, P, LI, BREAK, H1, H2, H3, H4, H5, H6, 
                      B, I, A, IMG, IMAGE, EM, DIV, SPAN, PRE,
                      BLOCKQUOTE, STRONG };

typedef std::unordered_map<std::string, Element> Elements;

#if __GLOBAL__
  Elements elements
  = {{"p",           Element::P}, {"div",     Element::DIV}, {"span", Element::SPAN}, {"br",  Element::BREAK}, {"h1",                 Element::H1},  
     {"h2",         Element::H2}, {"h3",       Element::H3}, {"h4",     Element::H4}, {"h5",     Element::H5}, {"h6",                 Element::H6}, 
     {"b",           Element::B}, {"i",         Element::I}, {"em",     Element::EM}, {"body", Element::BODY}, {"a",                   Element::A},
     {"img",       Element::IMG}, {"image", Element::IMAGE}, {"li",     Element::LI}, {"pre",   Element::PRE}, {"blockquote", Element::BLOCKQUOTE},
     {"strong", Element::STRONG}}
  ;
#else
  extern Elements elements;
#endif
