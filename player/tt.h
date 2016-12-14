// Copyright (c) 2015 MIT License by 6.172 Staff

// Transposition table

#ifndef TT_H
#define TT_H

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "./move_gen.h"
#include "./search.h"


typedef enum {
  UPPER,
  LOWER,
  EXACT
} ttBound_t;

// Just forward declarations
// The real definition is in tt.c
typedef struct ttRec ttRec_t;

// accessor methods for accessing move and score recorded in ttRec_t
static inline move_t tt_move_of(ttRec_t *tt);
static inline score_t tt_score_of(ttRec_t *tt);

static inline size_t tt_get_bytes_per_record();
static inline uint32_t tt_get_num_of_records();

// operations on the global hashtable
static inline void tt_make_hashtable(int sizeMeg);
static inline void tt_resize_hashtable(int sizeInMeg);
static inline void tt_free_hashtable();
static inline void tt_age_hashtable();

// putting / getting transposition data into / from hashtable
static inline void tt_hashtable_put(uint64_t key, int depth, score_t score,
                      int type, move_t move);
static inline ttRec_t *tt_hashtable_get(uint64_t key);

static inline score_t tt_adjust_score_from_hashtable(ttRec_t *rec, int ply);
static inline score_t tt_adjust_score_for_hashtable(score_t score, int ply);
static inline bool tt_is_usable(ttRec_t *tt, int depth, score_t beta);

#endif  // TT_H
