// Copyright (c) 2015 MIT License by 6.172 Staff

#ifndef SEARCH_H
#define SEARCH_H

#include <stdio.h>
#include "./move_gen.h"

// score_t values
#define INF 32700
#define WIN 32000
#define PAWN_VALUE 100

// Enable all optimization tables.
#define ENABLE_TABLES true

// the maximum possible value for score_t type
#define MAX_SCORE_VAL INT16_MAX


/*//// Killer moves table and lookup function
//
#define __KMT_dim__ [MAX_PLY_IN_SEARCH][4]  // NOLINT(whitespace/braces)
#define KMT(ply, id) (4 * ply + id)
static move_t killer_reference __KMT_dim__;  // up to 4 killers

// Best move history table and lookup function
// Format: best_move_history[color_t][piece_t][square_t][orientation]
#define __BMH_dim__ [2][6][ARR_SIZE][NUM_ORI]  // NOLINT(whitespace/braces)
#define BMH(color, piece, square, ori)                             \
    (color * 6 * ARR_SIZE * NUM_ORI + piece * ARR_SIZE * NUM_ORI + \
     square * NUM_ORI + ori)

static int best_move_history_reference __BMH_dim__;
*/

typedef int16_t score_t;  // Search uses "low res" values

// Main search routines and helper functions
typedef enum searchType {  // different types of search
  SEARCH_ROOT,
  SEARCH_PV,
  SEARCH_SCOUT
} searchType_t;

typedef struct searchNode {
  struct searchNode* parent;
  searchType_t type;
  score_t orig_alpha;
  score_t alpha;
  score_t beta;
  int depth;
  int ply;
  int fake_color_to_move;
  int quiescence;
  int pov;
  int legal_move_count;
  bool abort;
  score_t best_score;
  int best_move_index;
  position_t position;
  move_t subpv[MAX_PLY_IN_SEARCH];
} searchNode;


void init_tics();
void init_abort_timer(double goal_time);
double elapsed_time();
bool should_abort();
void reset_abort();
void init_best_move_history();
move_t get_move(sortable_move_t sortable_mv);
score_t searchRoot(position_t *p, score_t alpha, score_t beta, int depth,
                   int ply, move_t *pv, uint64_t *node_count_serial,
                   FILE *OUT);


#endif  // SEARCH_H
