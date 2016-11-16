// Copyright (c) 2015 MIT License by 6.172 Staff

#include "./fen.h"

#include <stdbool.h>
#include <stdio.h>

#include "./move_gen.h"
#include "./tbassert.h"

static void fen_error(char *fen, int c_count, char *msg) {
  fprintf(stderr, "\nError in FEN string:\n");
  fprintf(stderr, "   %s\n  ", fen);  // Indent 3 spaces
  for (int i = 0; i < c_count; ++i) {
    fprintf(stderr, " ");
  }
  fprintf(stderr, "^\n");  // graphical pointer to error character in string
  fprintf(stderr, "%s\n", msg);
}

// parse_fen_board
// Input:   board representation as a fen string
//          unpopulated board position struct
// Output:  index of where board description ends or 0 if parsing error
//          (populated) board position struct
static int parse_fen_board(position_t *p, char *fen) {
  // Invariant: square (f, r) is last square filled.
  // Fill from last rank to first rank, from first file to last file
  fil_t f = -1;
  rnk_t r = BOARD_WIDTH - 1;

  // Current and next characters from input FEN description
  char c, next_c;

  // Invariant: fen[c_count] is next character to be read
  int c_count = 0;

  // Loop also breaks internally if (f, r) == (BOARD_WIDTH-1, 0)
  while ((c = fen[c_count++]) != '\0') {
    int ori;
    ptype_t typ;

    switch (c) {
      // ignore whitespace until the end
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        if ((f == BOARD_WIDTH - 1) && (r == 0)) {  // our job is done
          return c_count;
        }
        break;

        // digits
      case '1':
        if (fen[c_count] == '0') {
          c_count++;
          c += 9;
        }
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        while (c > '0') {
          if (++f >= BOARD_WIDTH) {
            fen_error(fen, c_count, "Too many squares in rank.\n");
            return 0;
          }
          set_ptype(&p->board[square_of(f, r)], EMPTY);
          c--;
        }
        break;

        // pieces
      case 'N':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'N') {  // White King facing North
          ori = NN;
          typ = KING;
        } else if (next_c == 'W') {  // White Pawn facing NW
          ori = NW;
          typ = PAWN;
        } else if (next_c == 'E') {  // White Pawn facing NE
          ori = NE;
          typ = PAWN;
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        set_ptype(&p->board[square_of(f, r)], typ);
        set_color(&p->board[square_of(f, r)], WHITE);
        set_ori(&p->board[square_of(f, r)], ori);
        break;

      case 'n':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'n') {  // Black King facing North
          ori = NN;
          typ = KING;
        } else if (next_c == 'w') {  // Black Pawn facing NW
          ori = NW;
          typ = PAWN;
        } else if (next_c == 'e') {  // Black Pawn facing NE
          ori = NE;
          typ = PAWN;
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        set_ptype(&p->board[square_of(f, r)], typ);
        set_color(&p->board[square_of(f, r)], BLACK);
        set_ori(&p->board[square_of(f, r)], ori);
        break;

      case 'S':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'S') {  // White King facing SOUTH
          ori = SS;
          typ = KING;
        } else if (next_c == 'W') {  // White Pawn facing SW
          ori = SW;
          typ = PAWN;
        } else if (next_c == 'E') {  // White Pawn facing SE
          ori = SE;
          typ = PAWN;
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        set_ptype(&p->board[square_of(f, r)], typ);
        set_color(&p->board[square_of(f, r)], WHITE);
        set_ori(&p->board[square_of(f, r)], ori);
        break;

      case 's':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 's') {  // Black King facing South
          ori = SS;
          typ = KING;
        } else if (next_c == 'w') {  // Black Pawn facing SW
          ori = SW;
          typ = PAWN;
        } else if (next_c == 'e') {  // Black Pawn facing SE
          ori = SE;
          typ = PAWN;
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        set_ptype(&p->board[square_of(f, r)], typ);
        set_color(&p->board[square_of(f, r)], BLACK);
        set_ori(&p->board[square_of(f, r)], ori);
        break;

      case 'E':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'E') {  // White King facing East
          set_ptype(&p->board[square_of(f, r)], KING);
          set_color(&p->board[square_of(f, r)], WHITE);
          set_ori(&p->board[square_of(f, r)], EE);
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        break;

      case 'W':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'W') {  // White King facing West
          set_ptype(&p->board[square_of(f, r)], KING);
          set_color(&p->board[square_of(f, r)], WHITE);
          set_ori(&p->board[square_of(f, r)], WW);
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        break;

      case 'e':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'e') {  // Black King facing East
          set_ptype(&p->board[square_of(f, r)], KING);
          set_color(&p->board[square_of(f, r)], BLACK);
          set_ori(&p->board[square_of(f, r)], EE);
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        break;

      case 'w':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'w') {  // Black King facing West
          set_ptype(&p->board[square_of(f, r)], KING);
          set_color(&p->board[square_of(f, r)], BLACK);
          set_ori(&p->board[square_of(f, r)], WW);
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        break;

        // end of rank
      case '/':
        if (f == BOARD_WIDTH - 1) {
          f = -1;
          if (--r < 0) {
            fen_error(fen, c_count, "Too many ranks");
            return 0;
          }
        } else {
          fen_error(fen, c_count, "Too few squares in rank");
          return 0;
        }
        break;

      default:
        fen_error(fen, c_count, "Syntax error");
        return 0;
        break;
    }  // end switch
  }  // end while

  if ((f == BOARD_WIDTH - 1) && (r == 0)) {
    return c_count;
  } else {
    fen_error(fen, c_count, "Too few squares specified");
    return 0;
  }
}

// returns 0 if no error
static int get_sq_from_str(char *fen, int *c_count, int *sq) {
  char c, next_c;

  while ((c = fen[*c_count++]) != '\0') {
    // skip whitespace
    if ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r')) {
      continue;
    } else {
      break;  // found nonwhite character
    }
  }

  if (c == '\0') {
    *sq = 0;
    return 0;
  }

  // get file and rank
  if ((c - 'a' < 0) || (c - 'a') > BOARD_WIDTH) {
    fen_error(fen, *c_count, "Illegal specification of last move");
    return 1;
  }
  next_c = fen[*c_count++];
  if (next_c == '\0') {
    fen_error(fen, *c_count, "FEN ended before last move fully specified");

    if ((next_c - '0' < 0) || (next_c - '0') > BOARD_WIDTH) {
      fen_error(fen, *c_count, "Illegal specification of last move");
      return 1;
    }

    *sq = square_of(c - 'a', next_c - '0');
    return 0;
  }
  return 0;
}

// Translate a fen string into a board position struct
//
int fen_to_pos(position_t *p, char *fen) {
  static  position_t dmy1, dmy2;

  // these sentinels simplify checking previous
  // states without stepping past null pointers.
  dmy1.key = 0;
  dmy1.victims.zapped_count = 1;
  dmy1.victims.zapped[0] = 1;
  dmy1.history = NULL;

  dmy2.key = 0;
  dmy2.victims.zapped_count = 1;
  dmy2.victims.zapped[0] = 1;
  dmy2.history = &dmy1;


  p->key = 0;          // hash key
  p->victims.zapped_count = 0;       // piece destroyed by shooter
  p->history = &dmy2;  // history


  if (fen[0] == '\0') {  // Empty FEN => use starting position
    fen = "ss3nw3/3nw4/2nw1nw3/1nw3SE1SE/nw1nw3SE1/3SE1SE2/4SE3/3SE3NN W";
  }

  int c_count = 0;  // Invariant: fen[c_count] is next char to be read

  for (int i = 0; i < ARR_SIZE; ++i) {
    p->board[i] = 0;
    set_ptype(&p->board[i], INVALID);  // squares are invalid until filled
  }

  c_count = parse_fen_board(p, fen);
  if (!c_count) {
    return 1;  // parse error of board
  }

  // King check

  int Kings[2] = {0, 0};
  for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
    for (rnk_t r = 0; r < BOARD_WIDTH; ++r) {
      square_t sq = square_of(f, r);
      piece_t x = p->board[sq];
      ptype_t typ = ptype_of(x);
      if (typ == KING) {
        Kings[color_of(x)]++;
        p->kloc[color_of(x)] = sq;
      }
    }
  }

  if (Kings[WHITE] == 0) {
    fen_error(fen, c_count, "No White Kings");
    return 1;
  } else if (Kings[WHITE] > 1) {
    fen_error(fen, c_count, "Too many White Kings");
    return 1;
  } else if (Kings[BLACK] == 0) {
    fen_error(fen, c_count, "No Black Kings");
    return 1;
  } else if (Kings[BLACK] > 1) {
    fen_error(fen, c_count, "Too many Black Kings");
    return 1;
  }

  char c;
  bool done = false;
  // Look for color to move and set ply accordingly
  while (!done && (c = fen[c_count++]) != '\0') {
    switch (c) {
      // ignore whitespace until the end
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        break;

        // White to move
      case 'W':
      case 'w':
        p->ply = 0;
        done = true;
        break;

        // Black to move
      case 'B':
      case 'b':
        p->ply = 1;
        done = true;
        break;

      default:
        fen_error(fen, c_count, "Must specify White (W) or Black (B) to move");
        return 1;
        break;
    }
  }

  // Look for last move, if it exists
  int lm_from_sq, lm_to_sq, lm_rot;
  if (get_sq_from_str(fen, &c_count, &lm_from_sq) != 0) {  // parse error
    return 1;
  }
  if (lm_from_sq == 0) {   // from-square of last move
    p->last_move = 0;  // no last move specified
    p->key = compute_zob_key(p);
    p->mask[0] = compute_mask(p, 0);
    p->mask[1] = compute_mask(p, 1);
    return 0;
  }

  c = fen[c_count];

  switch (c) {
    case 'R':
    case 'r':
      lm_to_sq = lm_from_sq;
      lm_rot = RIGHT;
      break;
    case 'U':
    case 'u':
      lm_to_sq = lm_from_sq;
      lm_rot = UTURN;
      break;
    case 'L':
    case 'l':
      lm_to_sq = lm_from_sq;
      lm_rot = LEFT;
      break;

    default:  // Not a rotation
      lm_rot = NONE;
      if (get_sq_from_str(fen, &c_count, &lm_to_sq) != 0) {
        return 1;
      }
      break;
  }
  p->last_move = move_of(EMPTY, lm_rot, lm_from_sq, lm_to_sq);
  p->key = compute_zob_key(p);
  p->mask[0] = compute_mask(p, 0);
  p->mask[1] = compute_mask(p, 1);
  
  return 0;  // everything is okay
}

// King orientations
// This code is duplicated from move_gen.c
static char *king_ori_to_rep[2][NUM_ORI] = {  { "NN", "EE", "SS", "WW" },
                                              { "nn", "ee", "ss", "ww" } };

// Pawn orientations
static char *pawn_ori_to_rep[2][NUM_ORI] = {  { "NW", "NE", "SE", "SW" },
                                              { "nw", "ne", "se", "sw" } };

// Translate a position struct into a fen string
// NOTE: When you use the test framework in search.c, you should modify this
// function to match your optimized board representation in move_gen.c
//
// Input:   (populated) position struct
//          empty string where FEN characters will be written
// Output:  null
int pos_to_fen(position_t *p, char *fen) {
  int pos = 0;
  int i;

  for (rnk_t r = BOARD_WIDTH - 1; r >=0 ; --r) {
    int empty_in_a_row = 0;
    for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
      square_t sq = square_of(f, r);

      if (ptype_of(p->board[sq]) == INVALID) {     // invalid square
        tbassert(false, "Bad news, yo.\n");        // This is bad!
      }

      if (ptype_of(p->board[sq]) == EMPTY) {       // empty square
        empty_in_a_row++;
        continue;
      } else {
        if (empty_in_a_row) fen[pos++] = '0' + empty_in_a_row;
        empty_in_a_row = 0;

        int ori = ori_of(p->board[sq]);  // orientation
        color_t c = color_of(p->board[sq]);

        if (ptype_of(p->board[sq]) == KING) {
          for (i = 0; i < 2; i++) fen[pos++] = king_ori_to_rep[c][ori][i];
          continue;
        }

        if (ptype_of(p->board[sq]) == PAWN) {
          for (i = 0; i < 2; i++) fen[pos++] = pawn_ori_to_rep[c][ori][i];
          continue;
        }
      }
    }
    // assert: for larger boards, we need more general solns
    tbassert(BOARD_WIDTH <= 10, "BOARD_WIDTH = %d\n", BOARD_WIDTH);
    if (empty_in_a_row == 10) {
      fen[pos++] = '1';
      fen[pos++] = '0';
    } else if (empty_in_a_row) {
      fen[pos++] = '0' + empty_in_a_row;
    }
    if (r) fen[pos++] = '/';
  }
  fen[pos++] = ' ';
  fen[pos++] = 'W';
  fen[pos++] = '\0';

  return pos;
}
