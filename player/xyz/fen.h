// Copyright (c) 2015 MIT License by 6.172 Staff

#ifndef FEN_H
#define FEN_H

struct position;

// Assuming BOARD_WIDTH is at most 99, MAX_FEN_CHARS is
//   BOARD_WIDTH * BOARD_WIDTH * 2  (for a piece in every square)
//   + BOARD_WIDTH - 1  (for slashes delineating ranks)
//   + 2  (for final space and player turn)
#define MAX_FEN_CHARS 128

int fen_to_pos(struct position *p, char *fen);
int pos_to_fen(struct position *p, char *fen);

#endif  // FEN_H
