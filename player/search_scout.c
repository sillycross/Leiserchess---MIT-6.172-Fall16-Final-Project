// Copyright (c) 2015 MIT License by 6.172 Staff

// This file contains the scout search routine. Although this routine contains
//   some duplicated logic from the searchPV routine in search.c, it is
//   convenient to maintain it separately. This allows one to easily
//   parallelize scout search separately from searchPV.

#include "./tbassert.h"
#include "./simple_mutex.h"

// Checks whether a node's parent has aborted.
//   If this occurs, we should just stop and return 0 immediately.
bool parallel_parent_aborted(searchNode* node) {
  searchNode* pred = node->parent;
  while (pred != NULL) {
    if (pred->abort) {
      return true;
    }
    pred = pred->parent;
  }
  return false;
}

// Checks whether this node has aborted due to a cut-off.
//   If this occurs, we should actually return the score.
bool parallel_node_aborted(searchNode* node) {
  if (node->abort) {
    return true;
  }
  return false;
}

// Initialize a scout search node for a "Null Window" search.
//   https://chessprogramming.wikispaces.com/Scout
//   https://chessprogramming.wikispaces.com/Null+Window
static void initialize_scout_node(searchNode *node, int depth) {
  node->type = SEARCH_SCOUT;
  node->beta = -(node->parent->alpha);
  node->alpha = node->beta - 1;
  node->depth = depth;
  node->ply = node->parent->ply + 1;
  node->subpv[0] = 0;
  node->legal_move_count = 0;
  node->fake_color_to_move = color_to_move_of(&(node->position));
  // point of view = 1 for white, -1 for black
  node->pov = 1 - node->fake_color_to_move * 2;
  node->best_move_index = 0;  // index of best move found
  node->abort = false;
}

static const int range_tree_default[128] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
  16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
  32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
  48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
  64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
  80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
  96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
  112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127};

static score_t scout_search(searchNode *node, int depth,
                            uint64_t *node_count_serial) {
  // Initialize the search node.
  initialize_scout_node(node, depth);

  // check whether we should abort
  if (should_abort_check() || parallel_parent_aborted(node)) {
    return 0;
  }

  // Pre-evaluate this position.
  leafEvalResult pre_evaluation_result = evaluate_as_leaf(node, SEARCH_SCOUT);

  // If we decide to stop searching, return the pre-evaluation score.
  if (pre_evaluation_result.type == MOVE_EVALUATED) {
    return pre_evaluation_result.score;
  }

  // Populate some of the fields of this search node, using some
  //  of the information provided by the pre-evaluation.
  int hash_table_move = pre_evaluation_result.hash_table_move;
  node->best_score = pre_evaluation_result.score;
  node->quiescence = pre_evaluation_result.should_enter_quiescence;

  // Grab the killer-moves for later use.
  move_t killer_a = killer[KMT(node->ply, 0)];
  move_t killer_b = killer[KMT(node->ply, 1)];

  // Store the sorted move list on the stack.
  //   MAX_NUM_MOVES is all that we need.
  
  sortable_move_t move_list[MAX_NUM_MOVES];
  sortable_move_t sorted_move_list[MAX_NUM_MOVES];
  int range_tree[MAX_NUM_MOVES * 2];
  memcpy(range_tree + MAX_NUM_MOVES, range_tree_default, sizeof range_tree_default);

  // Obtain the sorted move list.
  memset(move_list, 0, sizeof move_list);
  int num_of_moves = get_sortable_move_list(node, move_list, hash_table_move);

  int number_of_moves_evaluated = 0;


  // A simple mutex. See simple_mutex.h for implementation details.
  simple_mutex_t node_mutex;
  init_simple_mutex(&node_mutex);

  // Sort the move list.
  // sort_incremental(move_list, num_of_moves, number_of_moves_evaluated);

  // Using branchless if here does not increase speed.
  for (int i = MAX_NUM_MOVES - 1; i; i--)
    if (move_list[range_tree[i << 1]] >= move_list[range_tree[(i << 1) ^ 1]])
      range_tree[i] = range_tree[i << 1];
    else
      range_tree[i] = range_tree[(i << 1) ^ 1];

  for (int mv_index = 0; mv_index < num_of_moves; mv_index++) {
    // Get the next move from the move list.
    int local_index = number_of_moves_evaluated++;
    move_t mv = get_move(move_list[range_tree[1]]);
    sorted_move_list[mv_index] = move_list[range_tree[1]];
    // printf("?? %d\n", local_index + MAX_NUM_MOVES);
    int i = range_tree[1], j;
    // printf("%d\n", i);
    move_list[i] = 0;
    i += MAX_NUM_MOVES;
    j = i>>1;
    if (move_list[range_tree[i]] >= move_list[range_tree[i^1]])
      range_tree[j] = range_tree[i];
    else
      range_tree[j] = range_tree[i ^ 1];
    i = j;
    j = i>>1;
    if (move_list[range_tree[i]] >= move_list[range_tree[i^1]])
      range_tree[j] = range_tree[i];
    else
      range_tree[j] = range_tree[i ^ 1];
    i = j;
    j = i>>1;
    if (move_list[range_tree[i]] >= move_list[range_tree[i^1]])
      range_tree[j] = range_tree[i];
    else
      range_tree[j] = range_tree[i ^ 1];
    i = j;j = i>>1;
    if (move_list[range_tree[i]] >= move_list[range_tree[i^1]])
      range_tree[j] = range_tree[i];
    else
      range_tree[j] = range_tree[i ^ 1];
    i = j;
    j = i>>1;
    if (move_list[range_tree[i]] >= move_list[range_tree[i^1]])
      range_tree[j] = range_tree[i];
    else
      range_tree[j] = range_tree[i ^ 1];
    i = j;
    j = i>>1;
    if (move_list[range_tree[i]] >= move_list[range_tree[i^1]])
      range_tree[j] = range_tree[i];
    else
      range_tree[j] = range_tree[i ^ 1];
    i = j;
    j = i>>1;
    if (move_list[range_tree[i]] >= move_list[range_tree[i^1]])
      range_tree[j] = range_tree[i];
    else
      range_tree[j] = range_tree[i ^ 1];
    i = j;

    if (TRACE_MOVES) {
      print_move_info(mv, node->ply);
    }

    // increase node count
    // __sync_fetch_and_add(node_count_serial, 1);
    (*node_count_serial)++;

    moveEvaluationResult result = evaluateMove(node, mv, killer_a, killer_b,
                                               SEARCH_SCOUT,
                                               node_count_serial);

    if (result.type == MOVE_ILLEGAL || result.type == MOVE_IGNORE
        || abortf || parallel_parent_aborted(node)) {
      continue;
    }

    // A legal move is a move that's not KO, but when we are in quiescence
    // we only want to count moves that has a capture.
    if (result.type == MOVE_EVALUATED) {
      node->legal_move_count++;
    }

    // process the score. Note that this mutates fields in node.
    bool cutoff = search_process_score(node, mv, local_index, &result, SEARCH_SCOUT);

    if (cutoff) {
      node->abort = true;
      break;
    }
  }

  if (parallel_parent_aborted(node)) {
    return 0;
  }

  if (node->quiescence == false) {
    update_best_move_history(&(node->position), node->best_move_index,
                             sorted_move_list, number_of_moves_evaluated);
  }

  tbassert(abs(node->best_score) != -INF, "best_score = %d\n",
           node->best_score);

  // Reads node->position.key, node->depth, node->best_score, and node->ply
  update_transposition_table(node);

  return node->best_score;
}


