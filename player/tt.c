// Copyright (c) 2015 MIT License by 6.172 Staff

// Transposition table
//
// https://chessprogramming.wikispaces.com/Transposition+Table

#include "./tt.h"

#include <stdlib.h>
#include <stdio.h>
#include "./tbassert.h"

int HASH;     // hash table size in MBytes
int USE_TT;   // Use the transposition table.
// Turn off for deterministic behavior of the search.

// the actual record that holds the data for the transposition
// typedef to be ttRec_t in tt.h
struct ttRec {
  uint64_t  key;
  move_t    move;
  score_t   score;
  int       quality;
  ttBound_t bound;
  int       age;
};


// each set is a 4-way set-associative cache and contains 4 records
#define RECORDS_PER_SET 4
typedef struct {
  ttRec_t records[RECORDS_PER_SET];
} ttSet_t;


// struct def for the global transposition table
struct ttHashtable {
  uint64_t num_of_sets;    // how many sets in the hashtable
  uint64_t mask;           // a mask to map from key to set index
  unsigned age;
  ttSet_t *tt_set;         // array of sets that contains the transposition
} hashtable;  // name of the global transposition table


// getting the move out of the record
move_t tt_move_of(ttRec_t *rec) {
  return rec->move;
}

// getting the score out of the record
score_t tt_score_of(ttRec_t *rec) {
  return rec->score;
}

size_t tt_get_bytes_per_record() {
  return sizeof(struct ttRec);
}

uint32_t tt_get_num_of_records() {
  return hashtable.num_of_sets * RECORDS_PER_SET;
}

void tt_resize_hashtable(int size_in_meg) {
  uint64_t size_in_bytes = (uint64_t) size_in_meg * (1ULL << 20);
  // total number of sets we could have in the hashtable
  uint64_t num_of_sets = size_in_bytes / sizeof(ttSet_t);

  uint64_t pow = 1;
  num_of_sets--;
  while (pow <= num_of_sets) pow *= 2;
  num_of_sets = pow;

  hashtable.num_of_sets = num_of_sets;
  hashtable.mask = num_of_sets - 1;
  hashtable.age = 0;

  free(hashtable.tt_set);  // free the old ones
  hashtable.tt_set = (ttSet_t *) malloc(sizeof(ttSet_t) * num_of_sets);

  if (hashtable.tt_set == NULL) {
    fprintf(stderr,  "Hash table too big\n");
    exit(1);
  }

  // might as well clear the table while we are at it
  memset(hashtable.tt_set, 0, sizeof(ttSet_t) * hashtable.num_of_sets);
}

void tt_make_hashtable(int size_in_meg) {
  hashtable.tt_set = NULL;
  tt_resize_hashtable(size_in_meg);
}

void tt_free_hashtable() {
  free(hashtable.tt_set);
  hashtable.tt_set = NULL;
}

// age the hash table by incrementing global age
void tt_age_hashtable() {
  hashtable.age++;
}

void tt_clear_hashtable() {
  memset(hashtable.tt_set, 0, sizeof(ttSet_t) * hashtable.num_of_sets);
  hashtable.age = 0;
}


void tt_hashtable_put(uint64_t key, int depth, score_t score,
                      int bound_type, move_t move) {
  tbassert(abs(score) != INF, "Score was infinite.\n");

  uint64_t set_index = key & hashtable.mask;
  // current record that we are looking into
  ttRec_t *curr_rec = hashtable.tt_set[set_index].records;
  // best record to replace that we found so far
  ttRec_t *rec_to_replace = curr_rec;
  int replacemt_val = -99;            // value of doing the replacement

  move = move & MOVE_MASK;

  for (int i = 0; i < RECORDS_PER_SET; i++, curr_rec++) {
    int value = 0;  // points for sorting

    // always use entry if it's not used or has same key
    if (!curr_rec->key || key == curr_rec->key) {
      if (move == 0) {
        move = curr_rec->move;
      }
      curr_rec->key = key;
      curr_rec->quality = depth;
      curr_rec->move = move;
      curr_rec->age = hashtable.age;
      curr_rec->score = score;
      curr_rec->bound = (ttBound_t) bound_type;

      return;
    }

    // otherwise, potential candidate for replacement
    if (curr_rec->age == hashtable.age) {
      value -= 6;   // prefer not to replace if same age
    }
    if (curr_rec->quality < rec_to_replace->quality) {
      value += 1;   // prefer to replace if worse quality
    }
    if (value > replacemt_val) {
      replacemt_val = value;
      rec_to_replace = curr_rec;
    }
  }
  // update the record that we are replacing with this record
  rec_to_replace->key = key;
  rec_to_replace->quality = depth;
  rec_to_replace->move = move;
  rec_to_replace->age = hashtable.age;
  rec_to_replace->score = score;
  rec_to_replace->bound = (ttBound_t) bound_type;
}


inline ttRec_t *tt_hashtable_get(uint64_t key) {
  if (!USE_TT) {
    return NULL;  // done if we are not using the transposition table
  }

  uint64_t set_index = key & hashtable.mask;
  ttRec_t *rec = hashtable.tt_set[set_index].records;

  //Assume RECORDS_PER_SET= 1
  // if (rec -> key == key)
  //   return rec;
  // else
  //   return NULL;
  ttRec_t *found = NULL;
  for (int i = 0; i < RECORDS_PER_SET; i++, rec++) {
    if (rec->key == key) {  // found the record that we are looking for
      found = rec;
      return rec;
    }
  }
  return found;
}


score_t win_in(int ply)  {
  return  WIN - ply;
}

score_t lose_in(int ply) {
  return -WIN + ply;
}

// when we insert score for a position into the hashtable, it does keeps the
// pure score that does not account for which ply you are in the search tree;
// when you retrieve the score from the hashtable, however, you want to
// consider the value of the position based on where you are in the search tree
score_t tt_adjust_score_from_hashtable(ttRec_t *rec, int ply_in_search) {
  score_t score = rec->score;
  if (score >= win_in(MAX_PLY_IN_SEARCH)) {
    return score - ply_in_search;
  }
  if (score <= lose_in(MAX_PLY_IN_SEARCH)) {
    return score + ply_in_search;
  }
  return score;
}

// the inverse of tt_adjust_score_for_hashtable
score_t tt_adjust_score_for_hashtable(score_t score, int ply_in_search) {
  if (score >= win_in(MAX_PLY_IN_SEARCH)) {
    return score + ply_in_search;
  }
  if (score <= lose_in(MAX_PLY_IN_SEARCH)) {
    return score - ply_in_search;
  }
  return score;
}


// Whether we can use this record or not
bool tt_is_usable(ttRec_t *tt, int depth, score_t beta) {
  // can't use this record if we are searching at depth higher than the
  // depth of this record.
  if (tt->quality < depth) {
    return false;
  }
  // otherwise check whether the score falls within the bounds
  if ((tt->bound == LOWER) && tt->score >= beta) {
    return true;
  }
  if ((tt->bound == UPPER) && tt->score < beta) {
    return true;
  }

  return false;
}

