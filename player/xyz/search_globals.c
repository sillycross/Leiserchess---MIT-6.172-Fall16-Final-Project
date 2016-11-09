// Copyright (c) 2015 MIT License by 6.172 Staff

// Killer move table
//
// https://chessprogramming.wikispaces.com/Killer+Move
// https://chessprogramming.wikispaces.com/Killer+Heuristic
//
// FORMAT: killer[ply][id]
#define __KMT_dim__ [MAX_PLY_IN_SEARCH*4]  // NOLINT(whitespace/braces)
#define KMT(ply, id) (4 * ply + id)
static move_t killer __KMT_dim__;  // up to 4 killers

// Best move history table and lookup function
//
// https://chessprogramming.wikispaces.com/History+Heuristic
//
// FORMAT: best_move_history[color_t][piece_t][square_t][orientation]
#define __BMH_dim__ [2*6*ARR_SIZE*NUM_ORI]  // NOLINT(whitespace/braces)
#define BMH(color, piece, square, ori)                             \
    (color * 6 * ARR_SIZE * NUM_ORI + piece * ARR_SIZE * NUM_ORI + \
     square * NUM_ORI + ori)

static int best_move_history __BMH_dim__;

void init_best_move_history() {
  memset(best_move_history, 0, sizeof(best_move_history));
}

static void update_best_move_history(position_t *p, int index_of_best,
                                     sortable_move_t* lst, int count) {
  tbassert(ENABLE_TABLES, "Tables weren't enabled.\n");

  int color_to_move = color_to_move_of(p);

  for (int i = 0; i < count; i++) {
    move_t   mv  = get_move(lst[i]);
    ptype_t  pce = ptype_mv_of(mv);
    rot_t    ro  = rot_of(mv);  // rotation
    square_t fs  = from_square(mv);
    int      ot  = ORI_MASK & (ori_of(p->board[fs]) + ro);
    square_t ts  = to_square(mv);

    int  s = best_move_history[BMH(color_to_move, pce, ts, ot)];

    if (index_of_best == i) {
      s = s + 11200;  // number will never exceed 1017
    }
    s = s * 0.90;  // decay score over time

    tbassert(s < 102000, "s = %d\n", s);  // or else sorting will fail

    best_move_history[BMH(color_to_move, pce, ts, ot)] = s;
  }
}

static void update_transposition_table(searchNode* node) {
  if (node->type == SEARCH_SCOUT) {
    if (node->best_score < node->beta) {
      tt_hashtable_put(node->position.key, node->depth,
                       tt_adjust_score_for_hashtable(node->best_score, node->ply),
                       UPPER, 0);
    } else {
      tt_hashtable_put(node->position.key, node->depth,
                       tt_adjust_score_for_hashtable(node->best_score, node->ply),
                       LOWER, node->subpv[0]);
    }
  } else if (node->type == SEARCH_PV) {
    if (node->best_score <= node->orig_alpha) {
      tt_hashtable_put(node->position.key, node->depth,
          tt_adjust_score_for_hashtable(node->best_score, node->ply), UPPER, 0);
    } else if (node->best_score >= node->beta) {
      tt_hashtable_put(node->position.key, node->depth,
          tt_adjust_score_for_hashtable(node->best_score, node->ply), LOWER, node->subpv[0]);
    } else {
      tt_hashtable_put(node->position.key, node->depth,
          tt_adjust_score_for_hashtable(node->best_score, node->ply), EXACT, node->subpv[0]);
    }
  }
}
