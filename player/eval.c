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
#define LOG2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X)) - 1))
#define MAX(x, y)  ((y) ^ (((x) ^ (y)) & -((x) > (y))))
#define MIN(x, y)  ((y) ^ (((x) ^ (y)) & -((x) < (y))))

static const ev_score_t pcentral_s[16][16] = {{125, 181, 220, 234, 234, 220, 181, 125, 58, -15, -92, -173, -255, -338, -422, -507},
{181, 249, 302, 323, 323, 302, 249, 181, 104, 24, -59, -143, -228, -314, -401, -488},
{220, 302, 375, 411, 411, 375, 302, 220, 135, 49, -37, -125, -212, -300, -388, -476},
{234, 323, 411, 500, 500, 411, 323, 234, 146, 58, -30, -118, -207, -295, -383, -472},
{234, 323, 411, 500, 500, 411, 323, 234, 146, 58, -30, -118, -207, -295, -383, -472},
{220, 302, 375, 411, 411, 375, 302, 220, 135, 49, -37, -125, -212, -300, -388, -476},
{181, 249, 302, 323, 323, 302, 249, 181, 104, 24, -59, -143, -228, -314, -401, -488},
{125, 181, 220, 234, 234, 220, 181, 125, 58, -15, -92, -173, -255, -338, -422, -507},
{58, 104, 135, 146, 146, 135, 104, 58, 0, -65, -137, -212, -290, -370, -451, -534},
{-15, 24, 49, 58, 58, 49, 24, -15, -65, -125, -190, -260, -333, -410, -488, -568},
{-92, -59, -37, -30, -30, -37, -59, -92, -137, -190, -250, -314, -383, -456, -530, -607},
{-173, -143, -125, -118, -118, -125, -143, -173, -212, -260, -314, -375, -439, -507, -578, -652},
{-255, -228, -212, -207, -207, -212, -228, -255, -290, -333, -383, -439, -500, -564, -631, -702},
{-338, -314, -300, -295, -295, -300, -314, -338, -370, -410, -456, -507, -564, -625, -689, -756},
{-422, -401, -388, -383, -383, -388, -401, -422, -451, -488, -530, -578, -631, -689, -750, -813},
{-507, -488, -476, -472, -472, -476, -488, -507, -534, -568, -607, -652, -702, -756, -813, -875}};

// PCENTRAL heuristic: Bonus for Pawn near center of board
inline ev_score_t pcentral(fil_t f, rnk_t r) {
  return pcentral_s[f][r];
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

// directions for laser: NN, EE, SS, WW
static int beam_64[NUM_ORI] = {1, 8, -1, -8};

uint64_t mark_laser_path_bit(position_t *p, color_t c) {
  square_t sq = p->kloc[c];
  int loc64 = fil_of(sq) * 8 + rnk_of(sq);
  uint64_t laser_map = 0;
  int bdir = ori_of(p->board[sq]);

  tbassert(ptype_of(p->board[sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[sq]));
  laser_map |= (1ULL << loc64);

  while (true) {
    sq += beam_of(bdir);
    if (ptype_of(p->board[sq]) == INVALID)
      return laser_map;
    tbassert(sq < ARR_SIZE && sq >= 0, "sq: %d\n", sq);
    loc64 += beam_64[bdir];
    laser_map |= (1ULL << loc64);

    switch (ptype_of(p->board[sq])) {
      case EMPTY:  // empty square
        break;
      case PAWN:  // Pawn
        bdir = reflect_of(bdir, ori_of(p->board[sq]));
        if (bdir < 0) {  // Hit back of Pawn
          return laser_map;
        }
        break;
      case KING:  // King
        return laser_map;  // sorry, game over my friend!
        break;
      // This is checked above
      // case INVALID:  // Ran off edge of board
      //   return laser_map;
      //   break;
      default:  // Shouldna happen, man!
        tbassert(false, "Not cool, man.  Not cool.\n");
        break;
    }
  }
  return laser_map;
}

uint64_t transform(char *laser_map) {
  uint64_t ans = 0;
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 8; j++)
      if (laser_map[square_of(i, j)])
        ans |= (1ULL << (i << 3) << j);
  return ans;
}

// PAWNPIN Heuristic: count number of pawns that are not pinned by the
//   opposing king's laser --- and are thus mobile.

int pawnpin(position_t *p, color_t color, uint64_t laser_map) {
  // color_t c = opp_color(color);
  // char laser_map[ARR_SIZE];

  // memcpy(laser_map, laser_map_s, sizeof laser_map);
  // mark_laser_path(p, c, laser_map, 1);  // find path of laser given that you aren't moving

  // uint64_t laser_map = mark_laser_path_bit(p, c);
  // if (transform(laser_map) != mark_laser_path_bit(p, c)) {
  //   printf("%llu %llu\n", transform(laser_map), mark_laser_path_bit(p, c));
  //   printf("ERROR\n");
  // }

  int unpinned_pawns = 0;

  // Figure out which pawns are not pinned down by the laser.
  for (int i = 0; i < 64; i++)
    if (!(laser_map & (1ULL << i))) {
      int r = ((i >> 3) + 4) * 16 + (i & 7) + 4;
      if (color_of(p->board[r]) == color &&
          ptype_of(p->board[r]) == PAWN) {
        unpinned_pawns += 1;
    }
  }
  // for (fil_t f = 4 * 16; f < 4 * 16 + 8 * 16; f += 16) {
  //   for (rnk_t r = f + 4; r < f + 12; ++r) {
  //     if (laser_map[r] == 0 &&
  //         color_of(p->board[r]) == color &&
  //         ptype_of(p->board[r]) == PAWN) {
  //       unpinned_pawns += 1;
  //     }
  //   }
  // }
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

int mobility(position_t *p, color_t color, uint64_t laser_map) {
  // color_t c = opp_color(color);
  // char laser_map[ARR_SIZE];

  // memcpy(laser_map, laser_map_s, sizeof laser_map);

  // mark_laser_path(p, c, laser_map, 1);  // find path of laser given that you aren't moving
  // uint64_t laser_map = mark_laser_path_bit(p, c);
  int mobility = 0;
  tbassert(ptype_of(p->board[king_sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[king_sq]));
  tbassert(color_of(p->board[king_sq]) == color,
           "color: %d\n", color_of(p->board[king_sq]));

  int kingx = fil_of(p->kloc[color]), kingy = rnk_of(p->kloc[color]);
  for (int i = MAX(0, kingx - 1); i < MIN(8, kingx + 2); i++)
    for (int j = MAX(0, kingy - 1); j < MIN(8, kingy + 2); j++)
      if (!(laser_map & (1ULL << (i << 3) << j)))
        mobility += 1;
  // if (laser_map[king_sq] == 0) {
  //   mobility++;
  // }
  // for (int d = 0; d < 8; ++d) {
  //   int sq = king_sq + dir_64[d];
  //   if (laser_map[sq] == 0) {
  //     mobility++;
  //   }
  // }
  return mobility;
}



static const float inv_s[16] = {1.0/1, 1.0/2, 1.0/3, 1.0/4, 1.0/5, 1.0/6, 1.0/7,
1.0/8, 1.0/9, 1.0/10, 1.0/11, 1.0/12, 1.0/13, 1.0/14, 1.0/15, 1.0/16};
// Harmonic-ish distance: 1/(|dx|+1) + 1/(|dy|+1)
// float h_dist(square_t a, square_t b) {
//   //  printf("a = %d, FIL(a) = %d, RNK(a) = %d\n", a, FIL(a), RNK(a));
//   //  printf("b = %d, FIL(b) = %d, RNK(b) = %d\n", b, FIL(b), RNK(b));
//   int delta_fil = abs(fil_of(a) - fil_of(b));
//   int delta_rnk = abs(rnk_of(a) - rnk_of(b));
//   float x = inv_s[delta_fil] + inv_s[delta_rnk];
//   //  printf("max_dist = %d\n\n", x);
//   return x;
// }

// H_SQUARES_ATTACKABLE heuristic: for shooting the enemy king
int h_squares_attackable(position_t *p, color_t c, uint64_t laser_map) {
  // char laser_map[ARR_SIZE];

  // memcpy(laser_map, laser_map_s, sizeof laser_map);
  // mark_laser_path(p, c, laser_map, 1);  // 1 = path of laser with no moves
  // uint64_t laser_map = mark_laser_path_bit(p, c);

  // if (transform(laser_map) != mark_laser_path_bit(p, c)) {
  //   printf("%llu %llu\n", transform(laser_map), mark_laser_path_bit(p, c));
  //   printf("ERROR\n");
  // }
  square_t o_king_sq = p->kloc[opp_color(c)];
  tbassert(ptype_of(p->board[o_king_sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[o_king_sq]));
  tbassert(color_of(p->board[o_king_sq]) != c,
           "color: %d\n", color_of(p->board[o_king_sq]));

  float h_attackable = 0;

  int king_sq_fil = fil_of(o_king_sq);
  int king_sq_rnk = rnk_of(o_king_sq);

  while (laser_map) {
    uint64_t y = laser_map & (-laser_map);
    int i = LOG2(y);
    h_attackable += inv_s[abs(king_sq_fil - (i >> 3))] + inv_s[abs(king_sq_rnk - (i & 7))];
    laser_map ^= y;
  }
  // for (int i = 0; i < 64; i++)
  //   if (laser_map & (1ULL << i))
  //     h_attackable += inv_s[abs(king_sq_fil - (i >> 3))] + inv_s[abs(king_sq_rnk - (i & 7))];
  // for (fil_t f = 4 * 16; f < 4 * 16 + 8 * 16; f += 16) {
  //   for (rnk_t r = f + 4; r < f + 12; ++r) {
  //     if (laser_map[r] != 0) {
  //       h_attackable += h_dist(r, o_king_sq);
  //     }
  //   }
  // }

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

  uint64_t laser_WHITE = mark_laser_path_bit(p, WHITE);
  uint64_t laser_BLACK = mark_laser_path_bit(p, BLACK);
  
  // H_SQUARES_ATTACKABLE heuristic
  ev_score_t w_hattackable = HATTACK * h_squares_attackable(p, WHITE, laser_WHITE);
  score[WHITE] += w_hattackable;
  if (verbose) {
    printf("HATTACK bonus %d for White\n", w_hattackable);
  }
  ev_score_t b_hattackable = HATTACK * h_squares_attackable(p, BLACK, laser_BLACK);
  score[BLACK] += b_hattackable;
  if (verbose) {
    printf("HATTACK bonus %d for Black\n", b_hattackable);
  }

  // MOBILITY heuristic
  int w_mobility = MOBILITY * mobility(p, WHITE, laser_BLACK);
  score[WHITE] += w_mobility;
  if (verbose) {
    printf("MOBILITY bonus %d for White\n", w_mobility);
  }
  int b_mobility = MOBILITY * mobility(p, BLACK, laser_WHITE);
  score[BLACK] += b_mobility;
  if (verbose) {
    printf("MOBILITY bonus %d for Black\n", b_mobility);
  }

  // PAWNPIN heuristic --- is a pawn immobilized by the enemy laser.
  int w_pawnpin = PAWNPIN * pawnpin(p, WHITE, laser_BLACK);
  score[WHITE] += w_pawnpin;
  int b_pawnpin = PAWNPIN * pawnpin(p, BLACK, laser_WHITE);
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
