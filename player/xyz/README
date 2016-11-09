This directory contains the codebase for the main game engine, leiserchess.

leiserchess.c:
    The main file that implements the UCI specification and invokes everything
    else. In UCI, when you type "go", a call is made to the search routine. To
    do so, a series of function calls happen: UciBeginSearch -> entry_point ->
    searchRoot in search.c

search_scout.c:
    Implements the low cost null-window scout search, which is what
    differentiates principal variation search from alpha-beta pruning.

search.c:
    Implements the remaining search routines for principal variation search.
    Includes functions searchRoot and searchPV (for PV-nodes). searchRoot first
    makes a call to scout_search in scout_search.c, followed by a call to
    searchPV.

eval.c:
    The static evaluator for board positions that implements different
    heuristics of the player.

move_gen.c:
    Implements board representation/hashing and move generation/execution.

search_common.c:
    Helper functions for the search routines, e.g. move evaluation/sorting,
    search pruning and extensions/reductions.

search_globals.c:
    Implements the killer move table (which keeps track of moves that triggered
    a beta-cutoff within a ply) and the best move history table (which keeps
    track of how often a move is determined to be the best, irrespective of
    position).

tt.c:
    Implements the transposition table (a hashtable storing positions seen by
    the player and some other relevant information for evaluating a position).

fen.c:
    The UCI uses FEN notation for board positions (see description of the FEN
    notation in doc/engine-interface.txt), so the program needs to translate a
    FEN string into the underlying board representation, and this file contains
    that logic.

util.c:
    Utility functions, such as random number generator, printing debugging
    messages, etc.