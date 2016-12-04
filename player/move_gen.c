// Copyright (c) 2015 MIT License by 6.172 Staff

#include "./move_gen.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "./eval.h"
#include "./fen.h"
#include "./search.h"
#include "./tbassert.h"
#include "./util.h"

static const uint64_t sq_to_board_bit[100] = {
0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL,
0ULL, 1ULL<<0, 1ULL<<1, 1ULL<<2, 1ULL<<3, 1ULL<<4, 1ULL<<5, 1ULL<<6, 1ULL<<7, 0ULL,
0ULL, 1ULL<<8, 1ULL<<9, 1ULL<<10, 1ULL<<11, 1ULL<<12, 1ULL<<13, 1ULL<<14, 1ULL<<15, 0ULL,
0ULL, 1ULL<<16, 1ULL<<17, 1ULL<<18, 1ULL<<19, 1ULL<<20, 1ULL<<21, 1ULL<<22, 1ULL<<23, 0ULL,
0ULL, 1ULL<<24, 1ULL<<25, 1ULL<<26, 1ULL<<27, 1ULL<<28, 1ULL<<29, 1ULL<<30, 1ULL<<31, 0ULL,
0ULL, 1ULL<<32, 1ULL<<33, 1ULL<<34, 1ULL<<35, 1ULL<<36, 1ULL<<37, 1ULL<<38, 1ULL<<39, 0ULL,
0ULL, 1ULL<<40, 1ULL<<41, 1ULL<<42, 1ULL<<43, 1ULL<<44, 1ULL<<45, 1ULL<<46, 1ULL<<47, 0ULL,
0ULL, 1ULL<<48, 1ULL<<49, 1ULL<<50, 1ULL<<51, 1ULL<<52, 1ULL<<53, 1ULL<<54, 1ULL<<55, 0ULL,
0ULL, 1ULL<<56, 1ULL<<57, 1ULL<<58, 1ULL<<59, 1ULL<<60, 1ULL<<61, 1ULL<<62, 1ULL<<63, 0ULL,
0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL};

int USE_KO;  // Respect the Ko rule

static const char *color_strs[2] = {"White", "Black"};

const char *color_to_str(color_t c) {
  return color_strs[c];
}

// -----------------------------------------------------------------------------
// Piece getters and setters (including color, ptype, orientation)
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Piece orientation strings
// -----------------------------------------------------------------------------

// King orientations
static const char *king_ori_to_rep[2][NUM_ORI] = { { "NN", "EE", "SS", "WW" },
                                      { "nn", "ee", "ss", "ww" } };

// Pawn orientations
static const char *pawn_ori_to_rep[2][NUM_ORI] = { { "NW", "NE", "SE", "SW" },
                                      { "nw", "ne", "se", "sw" } };

//static const char *nesw_to_str[NUM_ORI] = {"north", "east", "south", "west"};

// -----------------------------------------------------------------------------
// Board hashing
// -----------------------------------------------------------------------------

// Zobrist hashing
//
// https://chessprogramming.wikispaces.com/Zobrist+Hashing
//
// NOTE: Zobrist hashing uses piece_t as an integer index into to the zob table.
// So if you change your piece representation, you'll need to recompute what the
// old piece representation is when indexing into the zob table to get the same
// node counts.
static uint64_t   zob[ARR_SIZE][1<<PIECE_SIZE];
static uint64_t   zob_color;
uint64_t myrand();

uint64_t compute_zob_key(position_t *p) {
  uint64_t key = 0;
  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
      square_t sq = square_of(f, r);
      key ^= zob[sq][p->board[sq]];
    }
  }
  if (color_to_move_of(p) == BLACK)
    key ^= zob_color;

  return key;
}

uint64_t compute_mask(position_t *p, color_t color) {
  uint64_t key = 0;
  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
      square_t sq = square_of(f, r);
      if (p->board[sq] && color_of(p->board[sq]) == color)
        key |= (1ULL << (f << 3) << r);
    }
  }
  return key;
}

void init_zob() {
  for (int i = 0; i < ARR_SIZE; i++) {
    for (int j = 0; j < (1 << PIECE_SIZE); j++) {
      zob[i][j] = myrand();
    }
  }
  zob_color = myrand();
}

// -----------------------------------------------------------------------------
// Squares
// -----------------------------------------------------------------------------

// converts a square to string notation, returns number of characters printed
inline int square_to_str(square_t sq, char *buf, size_t bufsize) {
  fil_t f = fil_of(sq);
  rnk_t r = rnk_of(sq);
  if (f >= 0) {
    return snprintf(buf, bufsize, "%c%d", 'a'+ f, r);
  } else  {
    return snprintf(buf, bufsize, "%c%d", 'z' + f + 1, r);
  }
}

// -----------------------------------------------------------------------------
// Move getters and setters
// -----------------------------------------------------------------------------


// converts a move to string notation for FEN
void move_to_str(move_t mv, char *buf, size_t bufsize) {
  square_t f = from_square(mv);  // from-square
  square_t t = to_square(mv);    // to-square
  rot_t r = rot_of(mv);          // rotation
  const char *orig_buf = buf;

  buf += square_to_str(f, buf, bufsize);
  if (f != t) {
    buf += square_to_str(t, buf, bufsize - (buf - orig_buf));
  } else {
    switch (r) {
      case NONE:
        buf += square_to_str(t, buf, bufsize - (buf - orig_buf));
        break;
      case RIGHT:
        buf += snprintf(buf, bufsize - (buf - orig_buf), "R");
        break;
      case UTURN:
        buf += snprintf(buf, bufsize - (buf - orig_buf), "U");
        break;
      case LEFT:
        buf += snprintf(buf, bufsize - (buf - orig_buf), "L");
        break;
      default:
        tbassert(false, "Whoa, now.  Whoa, I say.\n");  // Bad, bad, bad
        break;
    }
  }
}

// -----------------------------------------------------------------------------
// Move generation
// -----------------------------------------------------------------------------

// Generate all moves from position p.  Returns number of moves.
// strict currently ignored
//
// https://chessprogramming.wikispaces.com/Move+Generation

int generate_all(position_t *p, sortable_move_t *sortable_move_list,
                 bool strict) {
  tbassert(p->mask[0] == compute_mask(p, 0),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[0], compute_mask(p, 0));
  tbassert(p->mask[1] == compute_mask(p, 1),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[1], compute_mask(p, 1));
  color_t color_to_move = color_to_move_of(p);
  // Make sure that the enemy_laser map is marked
  // char laser_map[ARR_SIZE];
  
  // memcpy(laser_map, laser_map_s, sizeof laser_map);
  
  // 1 = path of laser with no moves
  // mark_laser_path(p, opp_color(color_to_move), laser_map, 1);
  uint64_t laser_map = mark_laser_path_bit(p, opp_color(color_to_move));
  int move_count = 0;
  uint64_t mask = p -> mask[color_to_move] & ~laser_map;
  while (mask) {
    uint64_t y = mask & (-mask);
    mask ^= y;
    int i = LOG2(y);
    fil_t f = (i >> 3);
    rnk_t r = i & 7;

    square_t sq = (f + FIL_ORIGIN) * ARR_WIDTH + r + RNK_ORIGIN;
  // }
  
  // for (fil_t f = 0; f < BOARD_WIDTH; f++) {
  //   for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
  //     square_t  sq = square_of(f, r);
      piece_t x = p->board[sq];

      ptype_t typ = ptype_of(x);
      // color_t color = color_of(x);

      if (typ == PAWN || typ == KING) {
        // case EMPTY:
        //   break;
        // case PAWN:
          // if (laser_map[sq] == 1) continue;  // Piece is pinned down by laser.
        // case KING:
          // if (color != color_to_move) {  // Wrong color
          //   break;
          // }
          // directions
          for (int d = 0; d < 8; d++) {
            int dest = sq + dir_of(d);
            // Skip moves into invalid squares
            if (ptype_of(p->board[dest]) == INVALID) {
              continue;    // illegal square
            }

            WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
            WHEN_DEBUG_VERBOSE({
                move_to_str(move_of(typ, (rot_t) 0, sq, dest), buf, MAX_CHARS_IN_MOVE);
                DEBUG_LOG(1, "Before: %s ", buf);
              });
            tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
            sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, dest);

            WHEN_DEBUG_VERBOSE({
                move_to_str(get_move(sortable_move_list[move_count-1]), buf, MAX_CHARS_IN_MOVE);
                DEBUG_LOG(1, "After: %s\n", buf);
              });
          }

          // rotations - three directions possible
          for (int rot = 1; rot < 4; ++rot) {
            tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
            sortable_move_list[move_count++] = move_of(typ, (rot_t) rot, sq, sq);
          }
          if (typ == KING) {  // Also generate null move
            tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
            sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, sq);
          }
          // break;
        // case INVALID:
        // default:;
        //   tbassert(false, "Bogus, man.\n");  // Couldn't BE more bogus!
      }
    }
  // }

  WHEN_DEBUG_VERBOSE({
      DEBUG_LOG(1, "\nGenerated moves: ");
      for (int i = 0; i < move_count; ++i) {
        char buf[MAX_CHARS_IN_MOVE];
        move_to_str(get_move(sortable_move_list[i]), buf, MAX_CHARS_IN_MOVE);
        DEBUG_LOG(1, "%s ", buf);
      }
      DEBUG_LOG(1, "\n");
    });

  return move_count;
}

// -----------------------------------------------------------------------------
// Move execution
// -----------------------------------------------------------------------------

// Returns the square of piece that would be zapped by the laser if fired once,
// or 0 if no such piece exists.
//
// p : Current board state.
// c : Color of king shooting laser.
square_t fire_laser(position_t *p, color_t c) {
  tbassert(p->mask[0] == compute_mask(p, 0),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[0], compute_mask(p, 0));
  tbassert(p->mask[1] == compute_mask(p, 1),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[1], compute_mask(p, 1));
  // color_t fake_color_to_move = (color_to_move_of(p) == WHITE) ? BLACK : WHITE;0
  square_t sq = p->kloc[c];
  int bdir = ori_of(p->board[sq]);

  tbassert(ptype_of(p->board[sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[sq]));

  while (true) {
    sq += beam_of(bdir);
    tbassert(sq < ARR_SIZE && sq >= 0, "sq: %d\n", sq);

    switch (ptype_of(p->board[sq])) {
      case EMPTY:  // empty square
        break;
      case PAWN:  // Pawn
        bdir = reflect_of(bdir, ori_of(p->board[sq]));
        if (bdir < 0) {  // Hit back of Pawn
          return sq;
        }
        break;
      case KING:  // King
        return sq;  // sorry, game over my friend!
        break;
      case INVALID:  // Ran off edge of board
        return 0;
        break;
      // default:  // Shouldna happen, man!
      //   // tbassert(false, "Like porkchops and whipped cream.\n");
      //   break;
    }
  }
}

void low_level_make_move(position_t *old, position_t *p, move_t mv) {
  tbassert(mv != 0, "mv was zero.\n");

  WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
  WHEN_DEBUG_VERBOSE({
      move_to_str(mv, buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "low_level_make_move: %s\n", buf);
    });

  tbassert(old->key == compute_zob_key(old),
           "old->key: %"PRIu64", zob-key: %"PRIu64"\n",
           old->key, compute_zob_key(old));
  tbassert(old->mask[0] == compute_mask(old, 0),
           "old->key: %"PRIu64", zob-key: %"PRIu64"\n",
           old->mask[0], compute_mask(old, 0));
  tbassert(old->mask[1] == compute_mask(old, 1),
           "old->key: %"PRIu64", zob-key: %"PRIu64"\n",
           old->mask[1], compute_mask(old, 1));


  WHEN_DEBUG_VERBOSE({
      fprintf(stderr, "Before:\n");
      display(old);
    });

  square_t from_sq = from_square(mv);
  square_t to_sq = to_square(mv);
  rot_t rot = rot_of(mv);

  WHEN_DEBUG_VERBOSE({
      DEBUG_LOG(1, "low_level_make_move 2:\n");
      square_to_str(from_sq, buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "from_sq: %s\n", buf);
      square_to_str(to_sq, buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "to_sq: %s\n", buf);
      switch (rot) {
        case NONE:
          DEBUG_LOG(1, "rot: none\n");
          break;
        case RIGHT:
          DEBUG_LOG(1, "rot: R\n");
          break;
        case UTURN:
          DEBUG_LOG(1, "rot: U\n");
          break;
        case LEFT:
          DEBUG_LOG(1, "rot: L\n");
          break;
        default:
          tbassert(false, "Not like a boss at all.\n");  // Bad, bad, bad
          break;
      }
    });

  *p = *old;

  p->history = old;
  p->last_move = mv;

  tbassert(from_sq < ARR_SIZE && from_sq > 0, "from_sq: %d\n", from_sq);
  tbassert(p->board[from_sq] < (1 << PIECE_SIZE) && p->board[from_sq] >= 0,
           "p->board[from_sq]: %d\n", p->board[from_sq]);
  tbassert(to_sq < ARR_SIZE && to_sq > 0, "to_sq: %d\n", to_sq);
  tbassert(p->board[to_sq] < (1 << PIECE_SIZE) && p->board[to_sq] >= 0,
           "p->board[to_sq]: %d\n", p->board[to_sq]);
  tbassert(p->mask[0] == compute_mask(p, 0),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[0], compute_mask(p, 0));
  tbassert(p->mask[1] == compute_mask(p, 1),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[1], compute_mask(p, 1));

  p->key ^= zob_color;   // swap color to move

  piece_t from_piece = p->board[from_sq];
  piece_t to_piece = p->board[to_sq];

  

  if (to_sq != from_sq) {  // move, not rotation
    // Hash key updates
    p->key ^= zob[from_sq][from_piece];  // remove from_piece from from_sq
    p->key ^= zob[to_sq][to_piece];  // remove to_piece from to_sq

    p->board[to_sq] = from_piece;  // swap from_piece and to_piece on board
    p->board[from_sq] = to_piece;

    p->key ^= zob[to_sq][from_piece];  // place from_piece in to_sq
    p->key ^= zob[from_sq][to_piece];  // place to_piece in from_sq

    // if (!to_piece) {

    uint64_t tmp = sq_to_board_bit[from_sq] ^ sq_to_board_bit[to_sq];
    p->mask[color_of(from_piece)] ^= tmp;
    if (to_piece)
      p->mask[color_of(to_piece)] ^= tmp;
    // }
    // if ()
    // Update King locations if necessary
    if (ptype_of(from_piece) == KING) {
      p->kloc[color_of(from_piece)] = to_sq;
    }
    if (ptype_of(to_piece) == KING) {
      p->kloc[color_of(to_piece)] = from_sq;
    }

  } else {  // rotation
    // remove from_piece from from_sq in hash
    p->key ^= zob[from_sq][from_piece];
    set_ori(&from_piece, rot + ori_of(from_piece));  // rotate from_piece
    p->board[from_sq] = from_piece;  // place rotated piece on board
    p->key ^= zob[from_sq][from_piece];              // ... and in hash
  }

  // Increment ply
  p->ply++;

  tbassert(p->key == compute_zob_key(p),
           "p->key: %"PRIu64", zob-key: %"PRIu64"\n",
           p->key, compute_zob_key(p));

  tbassert(p->mask[0] == compute_mask(p, 0),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[0], compute_mask(p, 0));
  tbassert(p->mask[1] == compute_mask(p, 1),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[1], compute_mask(p, 1));

  WHEN_DEBUG_VERBOSE({
      fprintf(stderr, "After:\n");
      display(p);
    });
}

// return victim pieces or KO
victims_t make_move(position_t *old, position_t *p, move_t mv) {
  tbassert(mv != 0, "mv was zero.\n");

  WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);

  // move phase 1 - moving a piece
  low_level_make_move(old, p, mv);

  // move phase 2 - shooting the laser
  square_t victim_sq = 0;
  p->victims = 0;
  
  while ((victim_sq = fire_laser(p, color_to_move_of(old)))) {
    WHEN_DEBUG_VERBOSE({
        square_to_str(victim_sq, buf, MAX_CHARS_IN_MOVE);
        DEBUG_LOG(1, "Zapping piece on %s\n", buf);
      });

    // we definitely hit something with laser, remove it from board
    piece_t victim_piece = p->board[victim_sq];
    p->victims ++;
    p->victims |= 16 << color_of(victim_piece);
    p->key ^= zob[victim_sq][victim_piece];
    p->board[victim_sq] = 0;
    p->key ^= zob[victim_sq][0];
    p->mask[color_of(victim_piece)] ^= sq_to_board_bit[victim_sq];

    tbassert(p->key == compute_zob_key(p),
             "p->key: %"PRIu64", zob-key: %"PRIu64"\n",
             p->key, compute_zob_key(p));
    tbassert(p->mask[0] == compute_mask(p, 0),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[0], compute_mask(p, 0));
    tbassert(p->mask[1] == compute_mask(p, 1),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[1], compute_mask(p, 1));

    WHEN_DEBUG_VERBOSE({
        square_to_str(victim_sq, buf, MAX_CHARS_IN_MOVE);
        DEBUG_LOG(1, "Zapped piece on %s\n", buf);
      });

    // laser halts on king
    if (ptype_of(victim_piece) == KING) {
      p->victims |= 128;
      if (color_of(victim_piece))
        p->victims |= 64;
      break;
    }
  }

  if (USE_KO) {  // Ko rule
    if (p->key == (old->key ^ zob_color) && p->mask[0] == old->mask[0] && p->mask[1] == old->mask[1]) {
      bool match = true;


      for (fil_t f = FIL_ORIGIN * ARR_WIDTH; f < FIL_ORIGIN * ARR_WIDTH + BOARD_WIDTH * ARR_WIDTH; f += ARR_WIDTH) {
        for (rnk_t r = f + RNK_ORIGIN; r < f + RNK_ORIGIN + BOARD_WIDTH; ++r) {
          if (p -> board[r] != old -> board[r])
            match = false;
        }
      }

      if (match) return KO();
    }

    if (p->key == old->history->key && p->mask[0] == old->history->mask[0] && p->mask[1] == old->history->mask[1]) {
      bool match = true;

      for (fil_t f = FIL_ORIGIN * ARR_WIDTH; f < FIL_ORIGIN * ARR_WIDTH + BOARD_WIDTH * ARR_WIDTH; f += ARR_WIDTH) {
        for (rnk_t r = f + RNK_ORIGIN; r < f + RNK_ORIGIN + BOARD_WIDTH; ++r) {
          if (p -> board[r] != old -> history -> board[r])
            match = false;
        }
      }

      if (match) return KO();
    }
  }

  return p->victims;
}

// -----------------------------------------------------------------------------
// Move path enumeration (perft)
// -----------------------------------------------------------------------------

// Helper function for do_perft() (ply starting with 0).
//
// NOTE: This function reimplements some of the logic for make_move().
static uint64_t perft_search(position_t *p, int depth, int ply) {
  uint64_t node_count = 0;
  position_t np;
  sortable_move_t lst[MAX_NUM_MOVES];
  int num_moves;
  int i;

  if (depth == 0) {
    return 1;
  }

  num_moves = generate_all(p, lst, true);

  if (depth == 1) {
    return num_moves;
  }

  for (i = 0; i < num_moves; i++) {
    move_t mv = get_move(lst[i]);

    low_level_make_move(p, &np, mv);  // make the move baby!

    square_t victim_sq = 0;  // the guys to disappear
    np.victims = 0;
    
    while ((victim_sq = fire_laser(&np, color_to_move_of(p)))) {  // hit a piece
      piece_t victim_piece = np.board[victim_sq];
      tbassert((ptype_of(victim_piece) != EMPTY) &&
               (ptype_of(victim_piece) != INVALID),
               "type: %d\n", ptype_of(victim_piece));

      np.victims++;
      np.victims |= 16 << color_of(victim_piece);
      np.key ^= zob[victim_sq][victim_piece];   // remove from board
      np.board[victim_sq] = 0;
      np.key ^= zob[victim_sq][0];
      np.mask[color_of(victim_piece)] ^= sq_to_board_bit[victim_sq];

      if (ptype_of(victim_piece) == KING) {
        np.victims |= 128;
        if (color_of(victim_piece))
          np.victims |= 64;
        break;
      }
    }

    if (np.victims & 128) {
      // do not expand further: hit a King
      node_count++;
      continue;
    }

    uint64_t partialcount = perft_search(&np, depth-1, ply+1);
    node_count += partialcount;
  }

  return node_count;
}

// Debugging function to help verify that the move generator is working
// correctly.
//
// https://chessprogramming.wikispaces.com/Perft
void do_perft(position_t *gme, int depth, int ply) {
  fen_to_pos(gme, "");

  for (int d = 1; d <= depth; d++) {
    printf("perft %2d ", d);
    uint64_t j = perft_search(gme, d, 0);
    printf("%" PRIu64 "\n", j);
  }
}

// -----------------------------------------------------------------------------
// Position display
// -----------------------------------------------------------------------------

void display(position_t *p) {
  char buf[MAX_CHARS_IN_MOVE];

  printf("\ninfo Ply: %d\n", p->ply);
  printf("info Color to move: %s\n", color_to_str(color_to_move_of(p)));

  square_to_str(p->kloc[WHITE], buf, MAX_CHARS_IN_MOVE);
  printf("info White King: %s, ", buf);
  square_to_str(p->kloc[BLACK], buf, MAX_CHARS_IN_MOVE);
  printf("info Black King: %s\n", buf);

  if (p->last_move != 0) {
    move_to_str(p->last_move, buf, MAX_CHARS_IN_MOVE);
    printf("info Last move: %s\n", buf);
  } else {
    printf("info Last move: NULL\n");
  }

  for (rnk_t r = BOARD_WIDTH - 1; r >=0 ; --r) {
    printf("\ninfo %1d  ", r);
    for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
      square_t sq = square_of(f, r);

      tbassert(ptype_of(p->board[sq]) != INVALID,
               "ptype_of(p->board[sq]): %d\n", ptype_of(p->board[sq]));
      /*if (p->blocked[sq]) {
        printf(" xx");
        continue;
      }*/
      if (ptype_of(p->board[sq]) == EMPTY) {       // empty square
        printf(" --");
        continue;
      }

      int ori = ori_of(p->board[sq]);  // orientation
      color_t c = color_of(p->board[sq]);

      if (ptype_of(p->board[sq]) == KING) {
        printf(" %2s", king_ori_to_rep[c][ori]);
        continue;
      }

      if (ptype_of(p->board[sq]) == PAWN) {
        printf(" %2s", pawn_ori_to_rep[c][ori]);
        continue;
      }
    }
  }

  printf("\n\ninfo    ");
  for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
    printf(" %c ", 'a'+f);
  }
  printf("\n\n");
}
