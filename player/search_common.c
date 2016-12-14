// Copyright (c) 2015 MIT License by 6.172 Staff

// tic counter for how often we should check for abort
static int     tics = 0;
static double  sstart;    // start time of a search in milliseconds
static double  timeout;   // time elapsed before abort
static bool    abortf = false;  // abort flag for search

static const score_t fmarg[10] = {
  0, PAWN_VALUE / 2, PAWN_VALUE, (PAWN_VALUE * 5) / 2, (PAWN_VALUE * 9) / 2,
  PAWN_VALUE * 7, PAWN_VALUE * 10, PAWN_VALUE * 15, PAWN_VALUE * 20,
  PAWN_VALUE * 30
};

typedef enum {
    MOVE_EVALUATED,
    MOVE_ILLEGAL,
    MOVE_IGNORE,
    MOVE_GAMEOVER
} moveEvaluationResult_t;

typedef struct moveEvaluationResult {
  score_t score;
  moveEvaluationResult_t type;
  searchNode next_node;
} moveEvaluationResult;

typedef struct leafEvalResult {
  score_t score;
  moveEvaluationResult_t type;
  bool should_enter_quiescence;
  int hash_table_move;
} leafEvalResult;


typedef uint32_t sort_key_t;
static const uint64_t SORT_MASK = (1ULL << 32) - 1;
static const int SORT_SHIFT = 32;

/*
static sort_key_t sort_key(sortable_move_t mv) {
  return (sort_key_t) ((mv >> SORT_SHIFT) & SORT_MASK);
}
*/

inline static void set_sort_key(sortable_move_t *mv, sort_key_t key) {
  // sort keys must not exceed SORT_MASK
  assert ((0 <= key) && (key <= SORT_MASK));
  assert ((0 <= *mv) && (*mv <= SORT_MASK));
  *mv |= (uint64_t) key << SORT_SHIFT;
  return;
}

static inline void init_abort_timer(double goal_time) {
  sstart = milliseconds();
  // don't go over any more than 3 times the goal
  timeout = sstart + goal_time * 3.0;
}

static inline double elapsed_time() {
  return milliseconds() - sstart;
}

static inline bool should_abort() {
  return abortf;
}

static inline void reset_abort() {
  abortf = false;
}

static inline void init_tics() {
  tics = 0;
}

// move_t get_move(sortable_move_t sortable_mv) {
//   return (move_t) (sortable_mv & MOVE_MASK);
// }

static inline  score_t get_draw_score(position_t *p, int ply) {
  position_t *x = p->history;
  uint64_t cur = p->key;
  score_t score;
  while (true) {
    if (!zero_victims(x->victims)) {
      break;  // cannot be a repetition
    }
    x = x->history;
    if (!zero_victims(x->victims)) {
      break;  // cannot be a repetition
    }
    if (x->key == cur) {  // is a repetition
      if (ply & 1) {
        score = -DRAW;
      } else {
        score = DRAW;
      }
      return score;
    }
    x = x->history;
  }
  assert(false);  // This should not occur.
  return (score_t) 0;
}



// Detect move repetition
static inline  bool is_repeated(position_t *p, int ply) {
  if (!DETECT_DRAWS) {
    return false;  // no draw detected
  }

  position_t *x = p->history;
  uint64_t cur = p->key;

  while (true) {
    if (!zero_victims(x->victims)) {
      break;  // cannot be a repetition
    }
    x = x->history;
    if (!zero_victims(x->victims)) {
      break;  // cannot be a repetition
    }
    if (x->key == cur) {  // is a repetition
      return true;
    }
    x = x->history;
  }
  return false;
}



// check the victim pieces returned by the move to determine if it's a
// game-over situation.  If so, also calculate the score depending on
// the pov (which player's point of view)
static inline  bool is_game_over(victims_t victims, int pov, int ply) {
  if (victims & 128) {
    return true;
  }
  return false;
}

static inline  score_t get_game_over_score(victims_t victims, int pov, int ply) {
  score_t score;
  if (victims & 64) {
    score = WIN * pov;
  } else {
    score = -WIN * pov;
  }
  if (score < 0) {
    score += ply;
  } else {
    score -= ply;
  }
  return score;
}

static inline  void getPV(move_t pv, char *buf, size_t bufsize) {
  buf[0] = 0;
  if (pv) {
  // for (int i = 0; i < 1 && pv[i] != 0; i++) {
    char a[MAX_CHARS_IN_MOVE];
    move_to_str(pv, a, MAX_CHARS_IN_MOVE);
    // if (i != 0) {
    //   strncat(buf, " ", bufsize - strlen(buf) - 1);  // - 1, for the terminating '\0'
    // }
    strncat(buf, a, bufsize - strlen(buf) - 1);  // - 1, for the terminating '\0'
  }
}

static inline  void print_move_info(move_t mv, int ply) {
  char buf[MAX_CHARS_IN_MOVE];
  move_to_str(mv, buf, MAX_CHARS_IN_MOVE);
  printf("info");
  for (int i = 0; i < ply; ++i) {
    printf(" ----");
  }
  printf(" %s\n", buf);
}


// Evaluates the node before performing a full search.
//   does a few things differently if in scout search.
static inline leafEvalResult evaluate_as_leaf(searchNode *node, searchType_t type) {
  leafEvalResult result;
  result.type = MOVE_IGNORE;
  result.score = -INF;
  result.should_enter_quiescence = false;
  result.hash_table_move = 0;

  // get transposition table record if available.
  //
  // https://chessprogramming.wikispaces.com/Transposition+Table
  ttRec_t *rec = tt_hashtable_get(node->position.key);
  if (rec) {
    if (type == SEARCH_SCOUT && tt_is_usable(rec, node->depth, node->beta)) {
      result.type = MOVE_EVALUATED;
      result.score = tt_adjust_score_from_hashtable(rec, node->ply);
      return result;
    }
    result.hash_table_move = tt_move_of(rec);
  }

  // stand pat (having-the-move) bonus
  //
  // https://chessprogramming.wikispaces.com/Quiescence+Search#StandPat
  score_t sps = eval(&(node->position), false) + HMB;
  bool quiescence = (node->depth <= 0);  // are we in quiescence?
  result.should_enter_quiescence = quiescence;
  if (quiescence) {
    result.score = sps;
    if (result.score >= node->beta) {
      result.type = MOVE_EVALUATED;
      return result;
    }
  }

  // margin based forward pruning
  if (type == SEARCH_SCOUT && USE_NMM) {
    if (node->depth <= 2) {
      if (node->depth == 1 && sps >= node->beta + 3 * PAWN_VALUE) {
        result.type = MOVE_EVALUATED;
        result.score = node->beta;
        return result;
      }
      if (node->depth == 2 && sps >= node->beta + 5 * PAWN_VALUE) {
        result.type = MOVE_EVALUATED;
        result.score = node->beta;
        return result;
      }
    }
  }

  // extended futility pruning
  //
  // https://chessprogramming.wikispaces.com/Futility+Pruning#Extendedfutilitypruning
  if (type == SEARCH_SCOUT && node->depth <= FUT_DEPTH && node->depth > 0) {
    if (sps + fmarg[node->depth] < node->beta) {
      // treat this ply as a quiescence ply, look only at captures
      result.should_enter_quiescence = true;
      result.score = sps;
    }
  }
  return result;
}





static inline bool check_zero_victims(position_t *old, move_t mv) {
  square_t from_sq = from_square(mv);
  square_t to_sq = to_square(mv);

  if (old -> kill_d[color_to_move_of(old)]
    || (sq_to_board_bit[from_sq] & old -> laser[color_to_move_of(old)])
    || (sq_to_board_bit[to_sq] & old -> laser[color_to_move_of(old)]))
  return false;
    else
  return true;

  // if (old -> laser[0] != mark_laser_path_bit(old, 0))
  //   printf("error\n");
  // if (old -> laser[1] != mark_laser_path_bit(old, 1))
  //   printf("error\n");
  // return false;
}


// Evaluate the move by performing a search.
static inline moveEvaluationResult evaluateMove(searchNode *node, move_t mv, move_t killer_a,
                                  move_t killer_b, searchType_t type,
                                  uint64_t *node_count_serial) {

  int ext = 0;  // extensions
  bool blunder = false;  // shoot our own piece
  moveEvaluationResult result;
  result.next_node.subpv = 0;
  result.next_node.parent = node;

  // Make the move, and get any victim pieces.
  victims_t victims = make_move2(&(node->position), &(result.next_node.position),
                                mv);


  // Check whether this move changes the board state (moves that don't are
  // illegal).
  if (is_KO(victims)) {
    result.type = MOVE_ILLEGAL;
    return result;
  }


  // Check whether the game is over.
  if (is_game_over(victims, node->pov, node->ply)) {
    // Compute the end-game score.
    result.type = MOVE_GAMEOVER;
    result.score = get_game_over_score(victims, node->pov, node->ply);
    return result;
  }

  // Ignore noncapture moves when in quiescence.
  if (zero_victims(victims) && node->quiescence) {
    result.type = MOVE_IGNORE;
    return result;
  }


  // Check whether the board state has been repeated, this results in a draw.
  if (is_repeated(&(result.next_node.position), node->ply)) {
    result.type = MOVE_GAMEOVER;
    result.score = get_draw_score(&(result.next_node.position), node->ply);
    return result;
  }

  // Check whether we blundered (caused only our own pieces to be zapped).
  blunder = ((victims >> 4) == (1 << node->fake_color_to_move));


  // Do not consider moves that are blunders while in quiescence.
  if (node->quiescence && blunder) {
    result.type = MOVE_IGNORE;
    return result;
  }

  
  result.next_node.position.laser[0] = mark_laser_path_bit(&(result.next_node.position), 0);
  result.next_node.position.laser[1] = mark_laser_path_bit(&(result.next_node.position), 1);
  
  // Extend the search-depth by 1 if we captured a piece, since that means the
  // move was interesting.
  //
  // https://chessprogramming.wikispaces.com/Capture+Extensions


  if (victim_exists(victims) && !blunder) {
    ext = 1;
  }

  // Late move reductions - or LMR. Only done in scout search.
  //
  // https://chessprogramming.wikispaces.com/Late+Move+Reductions

  int next_reduction = 0;
  if (type == SEARCH_SCOUT && node->legal_move_count + 1 >= LMR_R1 && node->depth > 2 &&
      zero_victims(victims) && mv != killer_a && mv != killer_b) {
    if (node->legal_move_count + 1 >= LMR_R2) {
      next_reduction = 2;
    } else {
      next_reduction = 1;
    }
  }

  result.type = MOVE_EVALUATED;
  int search_depth = ext + node->depth - 1;

  // Check if we need to perform a reduced-depth search.
  //
  // After a reduced-depth search, a full-depth search will be performed if the
  //  reduced-depth search did not trigger a cut-off.
  if (next_reduction > 0) {
    search_depth -= next_reduction;
    int reduced_depth_score = -scout_search(&(result.next_node), search_depth,
                                            node_count_serial);
    if (reduced_depth_score < node->beta) {
      result.score = reduced_depth_score;
      return result;
    }
    search_depth += next_reduction;
  }

  // Check if we should abort due to time control.
  if (abortf) {
    result.score = 0;
    result.type = MOVE_IGNORE;
    return result;
  }


  if (type == SEARCH_SCOUT) {
    result.score = -scout_search(&(result.next_node), search_depth,
                                 node_count_serial);
  } else {
    if (node->legal_move_count == 0 || node->quiescence) {
      result.score = -searchPV(&(result.next_node), search_depth, node_count_serial);
    } else {
      result.score = -scout_search(&(result.next_node), search_depth,
                            node_count_serial);
      if (result.score > node->alpha) {
        result.score = -searchPV(&(result.next_node), node->depth + ext - 1, node_count_serial);
      }
    }
  }

  return result;
}

// Incremental sort of the move list.
static inline void sort_incremental(sortable_move_t *move_list, int num_of_moves) {
  for (int j = 0; j < num_of_moves; j++) {
    sortable_move_t insert = move_list[j];
    int hole = j;
    while (hole > 0 && insert > move_list[hole-1]) {
      move_list[hole] = move_list[hole-1];
      hole--;
    }
    move_list[hole] = insert;
  }
}

// Returns true if a cutoff was triggered, false otherwise.
static inline bool search_process_score(searchNode *node, move_t mv, int mv_index,
                                moveEvaluationResult *result, searchType_t type) {
  if (result->score > node->best_score) {
    node->best_score = result->score;
    node->best_move_index = mv_index;
    node->subpv = mv;

    // write best move into right position in PV buffer.
    // memcpy(node->subpv + 1, result->next_node.subpv,
    //        sizeof(move_t) * (MAX_PLY_IN_SEARCH - 1));
    // node->subpv[MAX_PLY_IN_SEARCH - 1] = 0;

    if (type != SEARCH_SCOUT && result->score > node->alpha) {
      node->alpha = result->score;
    }

    if (result->score >= node->beta) {
      if (mv != killer[KMT(node->ply, 0)] && ENABLE_TABLES) {
        killer[KMT(node->ply, 1)] = killer[KMT(node->ply, 0)];
        killer[KMT(node->ply, 0)] = mv;
      }
      return true;
    }
  }
  return false;
}

// Check if we should abort.
static inline bool should_abort_check() {
  tics++;
  if ((tics & ABORT_CHECK_PERIOD) == 0) {
    if (milliseconds() >= timeout) {
      abortf = true;
      return true;
    }
  }
  return false;
}

// Obtain a sorted move list.
//
// https://chessprogramming.wikispaces.com/Move+Ordering
static inline  int get_sortable_move_list(searchNode *node, sortable_move_t * move_list,
                         int hash_table_move) {
  // number of moves in list
  int num_of_moves = generate_all(&(node->position), move_list, false);

  color_t fake_color_to_move = color_to_move_of(&(node->position));

  move_t killer_a = killer[KMT(node->ply, 0)];
  move_t killer_b = killer[KMT(node->ply, 1)];

  // sort special moves to the front
  for (int mv_index = 0; mv_index < num_of_moves; mv_index++) {
    move_t mv = get_move(move_list[mv_index]);
    if (mv == hash_table_move) {
      set_sort_key(&move_list[mv_index], SORT_MASK);
    } else if (mv == killer_a) {
      set_sort_key(&move_list[mv_index], SORT_MASK - 1);
    } else if (mv == killer_b) {
      set_sort_key(&move_list[mv_index], SORT_MASK - 2);
    } else {
      ptype_t  pce = ptype_mv_of(mv);
      rot_t    ro  = rot_of(mv);   // rotation
      square_t fs  = from_square(mv);
      int      ot  = ORI_MASK & (ori_of(node->position.board[fs]) + ro);
      square_t ts  = to_square(mv);
      set_sort_key(&move_list[mv_index],
                   best_move_history[BMH(fake_color_to_move, pce, ts, ot)]);
    }
  }
  return num_of_moves;
}


static inline int get_sortable_move_list2(searchNode *node, sortable_move_t * move_list,
                         int hash_table_move, int killer_a, int killer_b) {
  // number of moves in list
  int num_of_moves = generate_all(&(node->position), move_list, false);

  color_t fake_color_to_move = color_to_move_of(&(node->position));

  // move_t killer_a = killer[KMT(node->ply, 0)];
  // move_t killer_b = killer[KMT(node->ply, 1)];

  // sort special moves to the front
  for (int mv_index = 0; mv_index < num_of_moves; mv_index++) {
    move_t mv = get_move(move_list[mv_index]);
    if (mv == hash_table_move) {
      set_sort_key(&move_list[mv_index], SORT_MASK);
    } else if (mv == killer_a) {
      set_sort_key(&move_list[mv_index], SORT_MASK - 1);
    } else if (mv == killer_b) {
      set_sort_key(&move_list[mv_index], SORT_MASK - 2);
    } else {
      if (node -> quiescence && check_zero_victims(&(node -> position), mv)) {
        move_list[mv_index] = move_list[num_of_moves - 1];
        num_of_moves -= 1;
        mv_index -= 1;
        continue;
      }
      ptype_t  pce = ptype_mv_of(mv);
      rot_t    ro  = rot_of(mv);   // rotation
      square_t fs  = from_square(mv);
      int      ot  = ORI_MASK & (ori_of(node->position.board[fs]) + ro);
      square_t ts  = to_square(mv);
      set_sort_key(&move_list[mv_index],
                   best_move_history[BMH(fake_color_to_move, pce, ts, ot)]);
    }
  }
  return num_of_moves;
}

