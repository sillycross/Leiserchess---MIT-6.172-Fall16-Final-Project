// Copyright (c) 2015 MIT License by 6.172 Staff

#include "./eval.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "./move_gen.h"
#include "./tbassert.h"

// -----------------------------------------------------------------------------
// Evaluation
// -----------------------------------------------------------------------------

typedef int32_t ev_score_t;  // Static evaluator uses "hi res" values

int RANDOMIZE;

int PCENTRAL;
int HATTACK;
int PBETWEEN;
int PCENTRAL;
int KFACE;
int KAGGRESSIVE;
int MOBILITY;
int PAWNPIN;

// Heuristics for static evaluation - described in the google doc
// mentioned in the handout.

ev_score_t pcentral_s[16][16];

// PCENTRAL heuristic: Bonus for Pawn near center of board
ev_score_t pcentral(fil_t f, rnk_t r) {
  if (pcentral_s[f][r])
    return pcentral_s[f][r];
  double df = BOARD_WIDTH/2 - f - 1;
  if (df < 0)  df = f - BOARD_WIDTH/2;
  double dr = BOARD_WIDTH/2 - r - 1;
  if (dr < 0) dr = r - BOARD_WIDTH/2;
  double bonus = 1 - sqrt(df * df + dr * dr) / (BOARD_WIDTH / sqrt(2));
  return pcentral_s[f][r] = PCENTRAL * bonus;
}


// returns true if c lies on or between a and b, which are not ordered
bool between(int c, int a, int b) {
  bool x = ((c >= a) && (c <= b)) || ((c <= a) && (c >= b));
  return x;
}

// PBETWEEN heuristic: Bonus for Pawn at (f, r) in rectangle defined by Kings at the corners
ev_score_t pbetween(position_t *p, fil_t f, rnk_t r) {
  bool is_between =
      between(f, fil_of(p->kloc[WHITE]), fil_of(p->kloc[BLACK])) &&
      between(r, rnk_of(p->kloc[WHITE]), rnk_of(p->kloc[BLACK]));
  return is_between ? PBETWEEN : 0;
}


// KFACE heuristic: bonus (or penalty) for King facing toward the other King
ev_score_t kface(position_t *p, fil_t f, rnk_t r) {
  square_t sq = square_of(f, r);
  piece_t x = p->board[sq];
  color_t c = color_of(x);
  square_t opp_sq = p->kloc[opp_color(c)];
  int delta_fil = fil_of(opp_sq) - f;
  int delta_rnk = rnk_of(opp_sq) - r;
  int bonus;

  switch (ori_of(x)) {
    case NN:
      bonus = delta_rnk;
      break;

    case EE:
      bonus = delta_fil;
      break;

    case SS:
      bonus = -delta_rnk;
      break;

    case WW:
      bonus = -delta_fil;
      break;

    default:
      bonus = 0;
      tbassert(false, "Illegal King orientation.\n");
  }

  return (bonus * KFACE) / (abs(delta_rnk) + abs(delta_fil));
}

// KAGGRESSIVE heuristic: bonus for King with more space to back
ev_score_t kaggressive(position_t *p, fil_t f, rnk_t r) {
  square_t sq = square_of(f, r);
  piece_t x = p->board[sq];
  color_t c = color_of(x);
  tbassert(ptype_of(x) == KING, "ptype_of(x) = %d\n", ptype_of(x));

  square_t opp_sq = p->kloc[opp_color(c)];
  fil_t of = fil_of(opp_sq);
  rnk_t _or = (rnk_t) rnk_of(opp_sq);

  int delta_fil = of - f;
  int delta_rnk = _or - r;

  int bonus = 0;

  if (delta_fil >= 0 && delta_rnk >= 0) {
    bonus = (f + 1) * (r + 1);
  } else if (delta_fil <= 0 && delta_rnk >= 0) {
    bonus = (BOARD_WIDTH - f) * (r + 1);
  } else if (delta_fil <= 0 && delta_rnk <= 0) {
    bonus = (BOARD_WIDTH - f) * (BOARD_WIDTH - r);
  } else if (delta_fil >= 0 && delta_rnk <= 0) {
    bonus = (f + 1) * (BOARD_WIDTH - r);
  }

  return (KAGGRESSIVE * bonus) / (BOARD_WIDTH * BOARD_WIDTH);
}

// Marks the path/line-of-sight of the laser until it hits a piece or goes off
// the board.
//
// p : Current board state.
// c : Color of king shooting laser.
// laser_map : End result will be stored here. Every square on the
//             path of the laser is marked with mark_mask.
// mark_mask : What each square is marked with.
void mark_laser_path(position_t *p, color_t c, char *laser_map,
                     char mark_mask) {
  square_t sq = p->kloc[c];
  int bdir = ori_of(p->board[sq]);

  tbassert(ptype_of(p->board[sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[sq]));
  laser_map[sq] |= mark_mask;

  while (true) {
    sq += beam_of(bdir);
    laser_map[sq] |= mark_mask;
    tbassert(sq < ARR_SIZE && sq >= 0, "sq: %d\n", sq);

    switch (ptype_of(p->board[sq])) {
      case EMPTY:  // empty square
        break;
      case PAWN:  // Pawn
        bdir = reflect_of(bdir, ori_of(p->board[sq]));
        if (bdir < 0) {  // Hit back of Pawn
          return;
        }
        break;
      case KING:  // King
        return;  // sorry, game over my friend!
        break;
      case INVALID:  // Ran off edge of board
        return;
        break;
      default:  // Shouldna happen, man!
        tbassert(false, "Not cool, man.  Not cool.\n");
        break;
    }
  }
}

// PAWNPIN Heuristic: count number of pawns that are not pinned by the
//   opposing king's laser --- and are thus mobile.

int pawnpin(position_t *p, color_t color) {
  color_t c = opp_color(color);
  char laser_map[ARR_SIZE];

  memset(laser_map, 4, sizeof laser_map);
  // for (int i = 0; i < ARR_SIZE; ++i) {
  //   laser_map[i] = 4;   // Invalid square
  // }

  for (fil_t f = 4 * 16; f < 4 * 16 + 8 * 16; f += 16) {
    for (rnk_t r = f + 4; r < f + 12; ++r) {
      laser_map[r] = 0;
    }
  }

  mark_laser_path(p, c, laser_map, 1);  // find path of laser given that you aren't moving



  int unpinned_pawns = 0;

  // Figure out which pawns are not pinned down by the laser.
  for (fil_t f = 4 * 16; f < 4 * 16 + 8 * 16; f += 16) {
    for (rnk_t r = f + 4; r < f + 12; ++r) {
      if (laser_map[r] == 0 &&
          color_of(p->board[r]) == color &&
          ptype_of(p->board[r]) == PAWN) {
        unpinned_pawns += 1;
      }
    }
  }
  // for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
  //   for (rnk_t r = 0; r < BOARD_WIDTH; ++r) {
  //     if (laser_map[square_of(f, r)] == 0 &&
  //         color_of(p->board[square_of(f, r)]) == color &&
  //         ptype_of(p->board[square_of(f, r)]) == PAWN) {
  //       unpinned_pawns += 1;
  //     }
  //   }
  // }

  return unpinned_pawns;
}

// MOBILITY heuristic: safe squares around king of given color.
int mobility(position_t *p, color_t color) {
  color_t c = opp_color(color);
  char laser_map[ARR_SIZE];

  memset(laser_map, 4, sizeof laser_map);
  // for (int i = 0; i < ARR_SIZE; ++i) {
  //   laser_map[i] = 4;   // Invalid square
  // }

  for (fil_t f = 4 * 16; f < 4 * 16 + 8 * 16; f += 16) {
    for (rnk_t r = f + 4; r < f + 12; ++r) {
      laser_map[r] = 0;
    }
  }

  mark_laser_path(p, c, laser_map, 1);  // find path of laser given that you aren't moving

  int mobility = 0;
  square_t king_sq = p->kloc[color];
  tbassert(ptype_of(p->board[king_sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[king_sq]));
  tbassert(color_of(p->board[king_sq]) == color,
           "color: %d\n", color_of(p->board[king_sq]));

  if (laser_map[king_sq] == 0) {
    mobility++;
  }
  for (int d = 0; d < 8; ++d) {
    square_t sq = king_sq + dir_of(d);
    if (laser_map[sq] == 0) {
      mobility++;
    }
  }
  return mobility;
}



// Harmonic-ish distance: 1/(|dx|+1) + 1/(|dy|+1)
float h_dist(square_t a, square_t b) {
  //  printf("a = %d, FIL(a) = %d, RNK(a) = %d\n", a, FIL(a), RNK(a));
  //  printf("b = %d, FIL(b) = %d, RNK(b) = %d\n", b, FIL(b), RNK(b));
  int delta_fil = abs(fil_of(a) - fil_of(b));
  int delta_rnk = abs(rnk_of(a) - rnk_of(b));
  float x = (1.0 / (delta_fil + 1)) + (1.0 / (delta_rnk + 1));
  //  printf("max_dist = %d\n\n", x);
  return x;
}

// H_SQUARES_ATTACKABLE heuristic: for shooting the enemy king
int h_squares_attackable(position_t *p, color_t c) {
  char laser_map[ARR_SIZE];

  memset(laser_map, 4, sizeof laser_map);
  // for (int i = 0; i < ARR_SIZE; ++i) {
  //   laser_map[i] = 4;   // Invalid square
  // }

  for (fil_t f = 4 * 16; f < 4 * 16 + 8 * 16; f += 16) {
    for (rnk_t r = f + 4; r < f + 12; ++r) {
      laser_map[r] = 0;
    }
  }

  mark_laser_path(p, c, laser_map, 1);  // 1 = path of laser with no moves

  square_t o_king_sq = p->kloc[opp_color(c)];
  tbassert(ptype_of(p->board[o_king_sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[o_king_sq]));
  tbassert(color_of(p->board[o_king_sq]) != c,
           "color: %d\n", color_of(p->board[o_king_sq]));

  float h_attackable = 0;
  for (fil_t f = 4 * 16; f < 4 * 16 + 8 * 16; f += 16) {
    for (rnk_t r = f + 4; r < f + 12; ++r) {
      if (laser_map[r] != 0) {
        h_attackable += h_dist(r, o_king_sq);
      }
    }
  }
  return h_attackable;
}

// Static evaluation.  Returns score
score_t eval(position_t *p, bool verbose) {
  // seed rand_r with a value of 1, as per
  // http://linux.die.net/man/3/rand_r
  static __thread unsigned int seed = 1;
  // verbose = true: print out components of score
  ev_score_t score[2] = { 0, 0 };
  //  int corner[2][2] = { {INF, INF}, {INF, INF} };
  ev_score_t bonus;
  // char buf[MAX_CHARS_IN_MOVE];

  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
      square_t sq = square_of(f, r);
      piece_t x = p->board[sq];
      color_t c = color_of(x);
      // if (verbose) {
      //   square_to_str(sq, buf, MAX_CHARS_IN_MOVE);
      // }

      switch (ptype_of(x)) {
        case EMPTY:
          break;
        case PAWN:
          // MATERIAL heuristic: Bonus for each Pawn
          bonus = PAWN_EV_VALUE;
          // if (verbose) {
          //   printf("MATERIAL bonus %d for %s Pawn on %s\n", bonus, color_to_str(c), buf);
          // }
          score[c] += bonus;

          // PBETWEEN heuristic
          bonus = pbetween(p, f, r);
          // if (verbose) {
          //   printf("PBETWEEN bonus %d for %s Pawn on %s\n", bonus, color_to_str(c), buf);
          // }
          score[c] += bonus;

          // PCENTRAL heuristic
          bonus = pcentral(f, r);
          // if (verbose) {
          //   printf("PCENTRAL bonus %d for %s Pawn on %s\n", bonus, color_to_str(c), buf);
          // }
          score[c] += bonus;
          break;

        case KING:
          // KFACE heuristic
          bonus = kface(p, f, r);
          // if (verbose) {
          //   printf("KFACE bonus %d for %s King on %s\n", bonus,
          //          color_to_str(c), buf);
          // }
          score[c] += bonus;

          // KAGGRESSIVE heuristic
          bonus = kaggressive(p, f, r);
          // if (verbose) {
          //   printf("KAGGRESSIVE bonus %d for %s King on %s\n", bonus, color_to_str(c), buf);
          // }
          score[c] += bonus;
          break;
        case INVALID:
          break;
        default:
          tbassert(false, "Jose says: no way!\n");   // No way, Jose!
      }
    }
  }

  // H_SQUARES_ATTACKABLE heuristic
  ev_score_t w_hattackable = HATTACK * h_squares_attackable(p, WHITE);
  score[WHITE] += w_hattackable;
  if (verbose) {
    printf("HATTACK bonus %d for White\n", w_hattackable);
  }
  ev_score_t b_hattackable = HATTACK * h_squares_attackable(p, BLACK);
  score[BLACK] += b_hattackable;
  if (verbose) {
    printf("HATTACK bonus %d for Black\n", b_hattackable);
  }

  // MOBILITY heuristic
  int w_mobility = MOBILITY * mobility(p, WHITE);
  score[WHITE] += w_mobility;
  if (verbose) {
    printf("MOBILITY bonus %d for White\n", w_mobility);
  }
  int b_mobility = MOBILITY * mobility(p, BLACK);
  score[BLACK] += b_mobility;
  if (verbose) {
    printf("MOBILITY bonus %d for Black\n", b_mobility);
  }

  // PAWNPIN heuristic --- is a pawn immobilized by the enemy laser.
  int w_pawnpin = PAWNPIN * pawnpin(p, WHITE);
  score[WHITE] += w_pawnpin;
  int b_pawnpin = PAWNPIN * pawnpin(p, BLACK);
  score[BLACK] += b_pawnpin;

  // score from WHITE point of view
  ev_score_t tot = score[WHITE] - score[BLACK];

  if (RANDOMIZE) {
    ev_score_t  z = rand_r(&seed) % (RANDOMIZE*2+1);
    tot = tot + z - RANDOMIZE;
  }

  if (color_to_move_of(p) == BLACK) {
    tot = -tot;
  }

  return tot / EV_SCORE_RATIO;
}
