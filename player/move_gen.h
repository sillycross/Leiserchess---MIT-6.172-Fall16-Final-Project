// Copyright (c) 2015 MIT License by 6.172 Staff

#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_NUM_MOVES 128      // real number = 7 x (8 + 3) + 1 x (8 + 4) = 89
#define MAX_PLY_IN_SEARCH 100  // up to 100 ply
#define MAX_PLY_IN_GAME 4096   // long game!  ;^)

// Used for debugging and display
#define MAX_CHARS_IN_MOVE 16  // Could be less
#define MAX_CHARS_IN_TOKEN 64

// -----------------------------------------------------------------------------
// Board
// -----------------------------------------------------------------------------

// The board (which is 8x8 or 10x10) is centered in a 16x16 array, with the
// excess height and width being used for sentinels.
#define ARR_WIDTH 10
#define ARR_SIZE (ARR_WIDTH * ARR_WIDTH)

// Board is 8 x 8 or 10 x 10
#define BOARD_WIDTH 8
#define LOG2(X) ((unsigned) (__builtin_ctzll((X))))

static const char laser_map_s[ARR_SIZE] = {
4,4,4,4,4,4,4,4,4,4,
4,0,0,0,0,0,0,0,0,4,
4,0,0,0,0,0,0,0,0,4,
4,0,0,0,0,0,0,0,0,4,
4,0,0,0,0,0,0,0,0,4,
4,0,0,0,0,0,0,0,0,4,
4,0,0,0,0,0,0,0,0,4,
4,0,0,0,0,0,0,0,0,4,
4,0,0,0,0,0,0,0,0,4,
4,4,4,4,4,4,4,4,4,4};

typedef uint8_t square_t;
typedef uint8_t rnk_t;
typedef uint8_t fil_t;

#define FIL_ORIGIN ((ARR_WIDTH - BOARD_WIDTH) / 2)
#define RNK_ORIGIN ((ARR_WIDTH - BOARD_WIDTH) / 2)

#define FIL_SHIFT 4
#define FIL_MASK 15
#define RNK_SHIFT 0
#define RNK_MASK 15

// -----------------------------------------------------------------------------
// Pieces
// -----------------------------------------------------------------------------

#define PIECE_SIZE 5  // Number of bits in (ptype, color, orientation)

typedef uint8_t piece_t;

// -----------------------------------------------------------------------------
// Piece types
// -----------------------------------------------------------------------------

#define PTYPE_SHIFT 2
#define PTYPE_MASK 3

typedef enum {
  EMPTY,
  PAWN,
  KING,
  INVALID
} ptype_t;

// -----------------------------------------------------------------------------
// Colors
// -----------------------------------------------------------------------------

#define COLOR_SHIFT 4
#define COLOR_MASK 1

typedef enum {
  WHITE = 0,
  BLACK
} color_t;

// -----------------------------------------------------------------------------
// Orientations
// -----------------------------------------------------------------------------

#define NUM_ORI 4
#define ORI_SHIFT 0
#define ORI_MASK (NUM_ORI - 1)

typedef enum {
  NN,
  EE,
  SS,
  WW
} king_ori_t;

typedef enum {
  NW,
  NE,
  SE,
  SW
} pawn_ori_t;

// -----------------------------------------------------------------------------
// Moves
// -----------------------------------------------------------------------------

// MOVE_MASK is 20 bits
#define MOVE_MASK 0xfffff


#define PTYPE_MV_SHIFT 18
#define PTYPE_MV_MASK 3
#define FROM_SHIFT 8
#define FROM_MASK 0xFF
#define TO_SHIFT 0
#define TO_MASK 0xFF
#define ROT_SHIFT 16
#define ROT_MASK 3

typedef uint32_t move_t;
typedef uint64_t sortable_move_t;  // extra bits used to store sort key

// Rotations
typedef enum {
  NONE,
  RIGHT,
  UTURN,
  LEFT
} rot_t;

// A single move can zap up to 13 pieces.
typedef struct victims_t {
  int8_t zapped_count;
  int8_t zapped_info;
} victims_t;

// returned by make move in illegal situation
#define KO_ZAPPED -1
// returned by make move in ko situation
#define ILLEGAL_ZAPPED -1

// -----------------------------------------------------------------------------
// Position
// -----------------------------------------------------------------------------

// Board representation is square-centric with sentinels.
//
// https://chessprogramming.wikispaces.com/Board+Representation
// https://chessprogramming.wikispaces.com/Mailbox
// https://chessprogramming.wikispaces.com/10x12+Board

typedef struct position {
  piece_t      board[ARR_SIZE];
  struct position  *history;     // history of position
  uint64_t     key;              // hash key
  int          ply;              // Even ply are White, odd are Black
  move_t       last_move;        // move that led to this position
  victims_t    victims;          // pieces destroyed by shooter
  square_t     kloc[2];          // location of kings
  uint64_t mask[2];
} position_t;

// -----------------------------------------------------------------------------
// Function prototypes
// -----------------------------------------------------------------------------

const char *color_to_str(color_t c);
#define color_to_move_of(p) ((p)->ply&1)
//color_t color_to_move_of(position_t *p);
#define color_of(x) ((color_t)(((x) >> COLOR_SHIFT) & COLOR_MASK))
//color_t color_of(piece_t x);
#define opp_color(c) ((c)^1)
//color_t opp_color(color_t c);
#define set_color(x,c) *(x) = (((c) & COLOR_MASK) << COLOR_SHIFT) | (*(x) & ~(COLOR_MASK << COLOR_SHIFT))
//void set_color(piece_t *x, color_t c);
#define ptype_of(x) ((ptype_t) (((x) >> PTYPE_SHIFT) & PTYPE_MASK))
//ptype_t ptype_of(piece_t x);
#define set_ptype(x, pt) *(x) = (((pt) & PTYPE_MASK) << PTYPE_SHIFT) | (*(x) & ~(PTYPE_MASK << PTYPE_SHIFT));
//void set_ptype(piece_t *x, ptype_t pt);
#define ori_of(x) (((x) >> ORI_SHIFT) & ORI_MASK)
//int ori_of(piece_t x);
#define set_ori(x, ori) *(x) = (((ori) & ORI_MASK) << ORI_SHIFT) | (*(x) & ~(ORI_MASK << ORI_SHIFT));
//void set_ori(piece_t *x, int ori);

void init_zob();
uint64_t compute_zob_key(position_t *p);
uint64_t compute_mask(position_t *p, color_t color);

#define square_of(f,r) (ARR_WIDTH * (FIL_ORIGIN + (f)) + RNK_ORIGIN + (r))
#define fil_of(sq) ((sq) / ARR_WIDTH - FIL_ORIGIN)
#define rnk_of(sq) ((sq) % ARR_WIDTH - RNK_ORIGIN)
/*
square_t square_of(fil_t f, rnk_t r);
fil_t fil_of(square_t sq);
rnk_t rnk_of(square_t sq);
*/
int square_to_str(square_t sq, char *buf, size_t bufsize);

int dir_of(int i);
int beam_of(int direction);
int reflect_of(int beam_dir, int pawn_ori);

#define ptype_mv_of(mv) ( (ptype_t) (((mv) >> PTYPE_MV_SHIFT) & PTYPE_MV_MASK))
//ptype_t ptype_mv_of(move_t mv);
#define from_square(mv) (((mv) >> FROM_SHIFT) & FROM_MASK)
//square_t from_square(move_t mv);
#define to_square(mv) ((mv >> TO_SHIFT) & TO_MASK)
//square_t to_square(move_t mv);
#define rot_of(mv) ((rot_t) ((mv >> ROT_SHIFT) & ROT_MASK))
//rot_t rot_of(move_t mv);
#define move_of(typ, rot, from_sq, to_sq) ((((typ) & PTYPE_MV_MASK) << PTYPE_MV_SHIFT) | \
      (((rot) & ROT_MASK) << ROT_SHIFT) | \
      (((from_sq) & FROM_MASK) << FROM_SHIFT) | \
      (((to_sq) & TO_MASK) << TO_SHIFT))
      
//move_t move_of(ptype_t typ, rot_t rot, square_t from_sq, square_t to_sq);
void move_to_str(move_t mv, char *buf, size_t bufsize);

int generate_all(position_t *p, sortable_move_t *sortable_move_list,
                 bool strict);
void do_perft(position_t *gme, int depth, int ply);
void low_level_make_move(position_t *old, position_t *p, move_t mv);
victims_t make_move(position_t *old, position_t *p, move_t mv);
void display(position_t *p);

#define KO() ((victims_t) {KO_ZAPPED, 0,})
//victims_t KO();
#define ILLEGAL() ((victims_t) {ILLEGAL_ZAPPED, 0})
//victims_t ILLEGAL();

#define is_ILLEGAL(victims) ((victims).zapped_count == ILLEGAL_ZAPPED)
//bool is_ILLEGAL(victims_t victims);
#define is_KO(victims) ((victims).zapped_count == KO_ZAPPED)
//bool is_KO(victims_t victims);
#define zero_victims(victims) ((victims).zapped_count == 0)
//bool zero_victims(victims_t victims);
#define victim_exists(victims) ((victims).zapped_count > 0)
//bool victim_exists(victims_t victims);

#endif  // MOVE_GEN_H
