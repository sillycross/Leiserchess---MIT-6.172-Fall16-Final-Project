//
//
// Copyright (c) 2015 MIT License by 6.172 Staff

#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#if PARALLEL
#include <cilk/cilk.h>
#include <cilk/reducer.h>
#endif

#include "./eval.h"
#include "./fen.h"
#include "./move_gen.h"
#include "./search.h"
#include "./tbassert.h"
#include "./tt.h"
#include "./util.h"
#include "./openbook.h"

#define get_move(mv) ((mv) & MOVE_MASK)

char  VERSION[] = "1038";

#define MAX_HASH 4096       // 4 GB
#define INF_TIME 99999999999.0
#define INF_DEPTH 999       // if user does not specify a depth, use 999

// if the time remain is less than this fraction, dont start the next search iteration
#define RATIO_FOR_TIMEOUT 0.5

// -----------------------------------------------------------------------------
// file I/O
// -----------------------------------------------------------------------------

static FILE *OUT;
static FILE *IN;

// Options for UCI interface

// defined in search.c
extern int DRAW;
extern int LMR_R1;
extern int LMR_R2;
extern int HMB;
extern int USE_NMM;
extern int FUT_DEPTH;
extern int TRACE_MOVES;
extern int DETECT_DRAWS;

// defined in eval.c
extern int RANDOMIZE;
extern int HATTACK;
extern int PBETWEEN;
extern int PCENTRAL;
extern int KFACE;
extern int KAGGRESSIVE;
extern int MOBILITY;
extern int PAWNPIN;

// defined in move_gen.c
extern int USE_KO;

// defined in tt.c
extern int USE_TT;
extern int HASH;

// struct for manipulating options below
typedef struct {
  char      name[MAX_CHARS_IN_TOKEN];   // name of options
  int       *var;       // pointer to an int variable holding its value
  int       dfault;     // default value
  int       min;        // lower bound on what we want it to be
  int       max;        // upper bound
} int_options;

// Configurable options for passing via UCI interface.
// These options are used to tune the AI and decide whether or not
// your AI will use some of the builtin techniques we implemented.
// Refer to the Google Doc mentioned in the handout for understanding
// the terminology.

static int_options iopts[] = {
  // name                  variable    default                lower bound     upper bound
  // -----------------------------------------------------------------------------------------
  { "hattack",             &HATTACK,   0.09 * PAWN_EV_VALUE,  0,              PAWN_EV_VALUE },
  { "mobility",           &MOBILITY,   0.04 * PAWN_EV_VALUE,  0,              PAWN_EV_VALUE },
  { "kaggressive",     &KAGGRESSIVE,   2.6 * PAWN_EV_VALUE,   0,              3.0 * PAWN_EV_VALUE },
  { "kface",                 &KFACE,   0.5 * PAWN_EV_VALUE,   0,              PAWN_EV_VALUE },
  { "pawnpin",             &PAWNPIN,   0.4 * PAWN_EV_VALUE,   0,              PAWN_EV_VALUE },
  { "pbetween",           &PBETWEEN,   0.2 * PAWN_EV_VALUE,   -PAWN_EV_VALUE, PAWN_EV_VALUE },
  { "pcentral",           &PCENTRAL,   0.05 * PAWN_EV_VALUE,  -PAWN_EV_VALUE, PAWN_EV_VALUE },
  { "hash",                   &HASH,   1024,                    1,              MAX_HASH   },
  { "draw",                   &DRAW,   -0.07 * PAWN_VALUE,    -PAWN_VALUE,    PAWN_VALUE    },
  { "randomize",         &RANDOMIZE,   0,                     0,              PAWN_EV_VALUE },
  { "lmr_r1",               &LMR_R1,   5,                     1,              MAX_NUM_MOVES },
  { "lmr_r2",               &LMR_R2,   20,                    1,              MAX_NUM_MOVES },
  { "hmb",                     &HMB,   0.03 * PAWN_VALUE,     0,              PAWN_VALUE    },
  { "fut_depth",         &FUT_DEPTH,   3,                     0,              5             },
  // debug options
  { "use_nmm",             &USE_NMM,   1,                     0,              1             },
  { "detect_draws",   &DETECT_DRAWS,   1,                     0,              1             },
  { "use_tt",               &USE_TT,   1,                     0,              1             },
  { "use_ko",               &USE_KO,   1,                     0,              1             },
  { "trace_moves",     &TRACE_MOVES,   0,                     0,              1             },
  { "",                        NULL,   0,                     0,              0             }
};

// -----------------------------------------------------------------------------
// Printing helpers
// -----------------------------------------------------------------------------


void lower_case(char *s) {
  int i;
  int c = strlen(s);

  for (i = 0; i < c; i++) {
    s[i] = tolower(s[i]);
  }

  return;
}

// Returns victims or NO_VICTIMS if no victims or -1 if illegal move
// makes the move described by 'mvstring'
victims_t make_from_string(position_t *old, position_t *p,
                           const char *mvstring) {
  sortable_move_t lst[MAX_NUM_MOVES];
  move_t mv = 0;
  // make copy so that mvstring can be a constant
  char string[MAX_CHARS_IN_MOVE];
  int move_count = generate_all(old, lst, true);

  snprintf(string, MAX_CHARS_IN_MOVE, "%s", mvstring);
  lower_case(string);

  for (int i = 0; i < move_count; i++) {
    char buf[MAX_CHARS_IN_MOVE];
    move_to_str(get_move(lst[i]), buf, MAX_CHARS_IN_MOVE);
    lower_case(buf);

    if (strcmp(buf, string) == 0) {
      mv = get_move(lst[i]);
      break;
    }
  }

  return (mv == 0) ? ILLEGAL() : make_move(old, p, mv);
}

typedef enum {
  NONWHITESPACE_STARTS,  // next nonwhitespace starts token
  WHITESPACE_ENDS,       // next whitespace ends token
  QUOTE_ENDS             // next double-quote ends token
} parse_state_t;


// -----------------------------------------------------------------------------
// UCI search (top level scout search call)
// -----------------------------------------------------------------------------

static move_t bestMoveSoFar;
static char theMove[MAX_CHARS_IN_MOVE];

static pthread_mutex_t entry_mutex;
static uint64_t node_count_serial;

typedef struct {
  position_t *p;
  int depth;
  double tme;
} entry_point_args;

void *entry_point(void *arg) {
  move_t subpv;

  entry_point_args *real_arg = (entry_point_args *) arg;
  int depth = real_arg->depth;
  position_t *p = real_arg->p;
  double tme = real_arg->tme;

  double et = 0.0;

  // start time of search
  init_abort_timer(tme);

  init_best_move_history();
  tt_age_hashtable();

  init_tics();

  for (int d = 1; d <= depth; d++) {  // Iterative deepening
    reset_abort();

    searchRoot(p, -INF, INF, d, 0, &subpv, &node_count_serial,
                OUT);

    et = elapsed_time();
    bestMoveSoFar = subpv;

    if (!should_abort()) {
      // print something?
    } else {
      break;
    }

    // don't start iteration that you cannot complete
    if (et > tme * RATIO_FOR_TIMEOUT) break;
  }


  // This unlock will allow the main thread lock/unlock in UCIBeginSearch to
  // proceed
  pthread_mutex_unlock(&entry_mutex);

  return NULL;
}

// Makes call to entry_point -> make call to searchRoot -> searchRoot in search.c
void UciBeginSearch(position_t *p, int depth, double tme) {
  pthread_mutex_lock(&entry_mutex);  // setup for the barrier

  entry_point_args args;
  args.depth = depth;
  args.p = p;
  args.tme = tme;
  node_count_serial = 0;

  if (check_is_in_openbook(p, OUT)) {
    pthread_mutex_unlock(&entry_mutex);
    return;
  }
  char bms[MAX_CHARS_IN_MOVE];
  entry_point(&args);
  move_to_str(bestMoveSoFar, bms, MAX_CHARS_IN_MOVE);
  snprintf(theMove, MAX_CHARS_IN_MOVE, "%s", bms);
  fprintf(OUT, "bestmove %s\n", bms);
  return;
}

// -----------------------------------------------------------------------------
// argparse help
// -----------------------------------------------------------------------------

// print help messages in uci
void help()  {
  printf("eval      - Evaluate current position.\n");
  printf("display   - Display current board state.\n");
  printf("generate  - Generate all possible moves.\n");
  printf("go        - Search from current state.  Possible arguments are:\n");
  printf("            depth <depth>:     search until depth <depth>\n");
  printf("            time <time_limit>: search assume you have <time> amount of time\n");
  printf("                               for the whole game.\n");
  printf("            inc <time_inc>:    set the fischer time increment for the search\n");
  printf("            Both time arguments are specified in milliseconds.\n");
  printf("            Sample usage: \n");
  printf("                go depth 4: search until depth 4\n");
  printf("help      - Display help (this info).\n");
  printf("isready   - Ask if the UCI engine is ready, if so it echoes \"readyok\".\n");
  printf("            This is mainly used to synchronize the engine with the GUI.\n");
  printf("move      - Make a move for current player.\n");
  printf("            Sample usage: \n");
  printf("                move j0j1: move a piece from j0 to j1\n");
  printf("perft     - Output the number of possible moves upto a given depth.\n");
  printf("            Used to verify move the generator.\n");
  printf("            Sample usage: \n");
  printf("                depth 3: generate all possible moves for depth 1--3\n");
  printf("position  - Set up the board using the fenstring given.  Possible arguments are:\n");
  printf("            startpos:     set up the board with default starting position.\n");
  printf("            endgame:      set up the board with endgame configuration.\n");
  printf("            fen <string>: set up the board using the given fenstring <string>.\n");
  printf("                          See doc/engine-interface.txt for more info on fen notation.\n");
  printf("            Sample usage: \n");
  printf("                position endgame: set up the board so that only kings remain\n");
  printf("quit      - Quit this program\n");
  printf("setoption - Set configuration options used in the engine, the format is: \n");
  printf("            setoption name <name> value <val>.\n");
  printf("            Use the comment \"uci\" to see possible options and their current values\n");
  printf("            Sample usage: \n");
  printf("                setoption name fut_depth value 4: set fut_depth to 4\n");
  printf("uci       - Display UCI version and options\n");
  printf("\n");
}

// Get next token in s[] and put into token[]. Strips quotes.
// Side effects modify ps[].
int parse_string_q(char *s, char *token[]) {
  int token_count = 0;
  parse_state_t state = NONWHITESPACE_STARTS;

  while (*s != '\0') {
    switch (state) {
      case NONWHITESPACE_STARTS:
        switch (*s) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
          case '#':
            *s = '\0';
            break;
          case '"':
            state = QUOTE_ENDS;
            *s = '\0';
            if (*(s+1) == '\0') {
              fprintf(stderr, "Input parse error: no end of quoted string\n");
              return 0;  // Parse error
            }
            token[token_count++] = s+1;
            break;
          default:  // nonwhitespace, nonquote
            state = WHITESPACE_ENDS;
            token[token_count++] = s;
        }
        break;

      case WHITESPACE_ENDS:
        switch (*s) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
          case '#':
            state = NONWHITESPACE_STARTS;
            *s = '\0';
            break;
          case '"':
            fprintf(stderr, "Input parse error: misplaced quote\n");
            return 0;  // Parse error
            break;
          default:     // nonwhitespace, nonquote
            break;
        }
        break;

      case QUOTE_ENDS:
        switch (*s) {
          case ' ':
          case '\t':
          case '\n':
          case '\r':
          case '#':
            break;
          case '"':
            state = NONWHITESPACE_STARTS;
            *s = '\0';
            if (*(s+1) != '\0' && *(s+1) != ' ' &&
                *(s+1) != '\t' && *(s+1) != '\n' && *(s+1) != '\r') {
              fprintf(stderr, "Input parse error: quoted string must be followed by white space\n");
              fprintf(stderr, "ASCII char: %d\n", (int) *(s+1));
              return 0;  // Parse error
            }
            break;
          default:  // nonwhitespace, nonquote
            break;
        }
        break;
    }
    s++;
  }
  if (state == QUOTE_ENDS) {
    fprintf(stderr, "Input parse error: no end quote on quoted string\n");
    return 0;  // Parse error
  }

  return token_count;
}


void init_options() {
  for (int j = 0; iopts[j].name[0] != 0; j++) {
    tbassert(iopts[j].min <= iopts[j].dfault,
             "min: %d, dfault: %d\n", iopts[j].min, iopts[j].dfault);
    tbassert(iopts[j].max >= iopts[j].dfault,
             "max: %d, dfault: %d\n", iopts[j].max, iopts[j].dfault);
    *iopts[j].var = iopts[j].dfault;
  }
}

void print_options() {
  for (int j = 0; iopts[j].name[0] != 0; j++) {
    printf("option name %s type spin value %d default %d min %d max %d\n",
           iopts[j].name,
           *iopts[j].var,
           iopts[j].dfault,
           iopts[j].min,
           iopts[j].max);
  }
  return;
}

// -----------------------------------------------------------------------------
// main - implements to UCI protocol. The command line interface you use
// described in doc/engine-interface.txt
// -----------------------------------------------------------------------------

int main(int argc, char *argv[]) {
  position_t *gme = (position_t *) malloc(sizeof(position_t) * MAX_PLY_IN_GAME);

  setbuf(stdout, NULL);
  setbuf(stdin, NULL);

  OUT = stdout;

  if (argc > 1) {
    IN = fopen(argv[1], "r");
  } else {
    IN = stdin;
  }

  init_options();
  init_zob();

  char **tok = (char **) malloc(sizeof(char *) * MAX_CHARS_IN_TOKEN * MAX_PLY_IN_GAME);
  int   ix = 0;  // index of which position we are operating on

  // input string - last message from UCI interface
  // big enough to support 4000 moves
  char *istr = (char *) malloc(sizeof(char) * 24000);


  tt_make_hashtable(HASH);   // initial hash table
  fen_to_pos(&gme[ix], "");  // initialize with an actual position

  //  Check to make sure we don't loop infinitely if we don't get input.
  bool saw_input = false;
  double start_time = milliseconds();

  while (true) {
    int n;

    if (fgets(istr, 20478, IN) != NULL) {
      int token_count = parse_string_q(istr, tok);

      if (token_count == 0) {  // no input
        if (!saw_input && milliseconds() - start_time > 1000*5) {
          fprintf(stdout, "Received no commands after 5 seconds, terminating program\n");
          break;
        }
        continue;
      } else {
        saw_input = true;
      }

      if (strcmp(tok[0], "quit") == 0) {
        break;
      }

      if (strcmp(tok[0], "position") == 0) {
        n = 0;
        if (token_count < 2) {  // no input
          fprintf(OUT, "Second argument required.  Use 'help' to see valid commands.\n");
          continue;
        }

        if (strcmp(tok[1], "startpos") == 0) {
          ix = 0;
          fen_to_pos(&gme[ix], "");
          n = 2;
        } else if (strcmp(tok[1], "endgame") == 0) {
          ix = 0;
          if (BOARD_WIDTH == 10)
            fen_to_pos(&gme[ix], "ss9/10/10/10/10/10/10/10/10/9NN W");
          else if (BOARD_WIDTH == 8)
            fen_to_pos(&gme[ix], "ss7/8/8/8/8/8/8/7NN W");
          n = 2;
        } else if (strcmp(tok[1], "fen") == 0) {
          if (token_count < 3) {  // no input
            fprintf(OUT, "Third argument (the fen string) required.\n");
            continue;
          }
          ix = 0;
          n = 3;
          char fen_tok[MAX_CHARS_IN_TOKEN];
          strncpy(fen_tok, tok[2], MAX_CHARS_IN_TOKEN);
          if (token_count >= 4 && (
                  strcmp(tok[3], "B") == 0 || strcmp(tok[3], "W") == 0 ||
                  strcmp(tok[3], "b") == 0 || strcmp(tok[3], "w") == 0)) {
            n++;
            strncat(fen_tok, " ", MAX_CHARS_IN_TOKEN - strlen(fen_tok) - 1);
            strncat(fen_tok, tok[3], MAX_CHARS_IN_TOKEN - strlen(fen_tok) - 1);
          }
          fen_to_pos(&gme[ix], fen_tok);
        }

        int save_ix = ix;
        if (token_count > n+1) {
          for (int j = n + 1; j < token_count; j++) {
            victims_t victims = make_from_string(&gme[ix], &gme[ix+1], tok[j]);
            if (is_ILLEGAL(victims) || is_KO(victims)) {
              fprintf(OUT, "info string Move %s is illegal\n", tok[j]);
              ix = save_ix;
              // breaks multiple loops.
              goto next_command;
            } else {
              ix++;
            }
          }
        }

     next_command:
        continue;
      }

      if (strcmp(tok[0], "move") == 0) {
        victims_t victims = make_from_string(&gme[ix], &gme[ix+1], tok[1]);
        if (token_count < 2) {  // no input
          fprintf(OUT, "Second argument (move positon) required.\n");
          continue;
        }
        if (is_ILLEGAL(victims) || is_KO(victims)) {
          fprintf(OUT, "Illegal move %s\n", tok[1]);
        } else {
          ix++;
          display(&gme[ix]);
        }
        continue;
      }

      if (strcmp(tok[0], "uci") == 0) {
        // TODO(you): Change the name & version once you start modifying the code!
        printf("id name %s version %s\n", "Leiserchess", VERSION);
        printf("id author %s\n",
               "Don Dailey, Charles E. Leiserson, and the staff of MIT 6.172");
        print_options();
        printf("uciok\n");
        continue;
      }

      if (strcmp(tok[0], "isready") == 0) {
        printf("readyok\n");
        continue;
      }

      if (strcmp(tok[0], "setoption") == 0) {
        int sostate = 0;
        char  name[MAX_CHARS_IN_TOKEN];
        char  value[MAX_CHARS_IN_TOKEN];

        strncpy(name, "", MAX_CHARS_IN_TOKEN);
        strncpy(value, "", MAX_CHARS_IN_TOKEN);

        for (int i = 1; i < token_count; i++) {
          if (strcmp(tok[i], "name") == 0) {
            sostate = 1;
            continue;
          }
          if (strcmp(tok[i], "value") == 0) {
            sostate = 2;
            continue;
          }
          if (sostate == 1) {
            // we subtract 1 from the length to account for the
            // additional terminating '\0' that strncat appends
            strncat(name, " ", MAX_CHARS_IN_TOKEN - strlen(name) - 1);
            strncat(name, tok[i], MAX_CHARS_IN_TOKEN - strlen(name) - 1);
            continue;
          }

          if (sostate == 2) {
            strncat(value, " ", MAX_CHARS_IN_TOKEN - strlen(value) - 1);
            strncat(value, tok[i], MAX_CHARS_IN_TOKEN - strlen(value) - 1);
            if (i+1 < token_count) {
              strncat(value, " ", MAX_CHARS_IN_TOKEN - strlen(value) - 1);
              strncat(value, tok[i+1], MAX_CHARS_IN_TOKEN - strlen(value) - 1);
              i++;
            }
            continue;
          }
        }

        lower_case(name);
        lower_case(value);

        // see if option is in the configurable integer parameters
        {
          bool recognized = false;
          for (int j = 0; iopts[j].name[0] != 0; j++) {
            char loc[MAX_CHARS_IN_TOKEN];

            snprintf(loc, MAX_CHARS_IN_TOKEN, "%s", iopts[j].name);
            lower_case(loc);
            if (strcmp(name+1, loc) == 0) {
              recognized = true;
              int v = strtol(value + 1, (char **)NULL, 10);
              if (v < iopts[j].min) {
                v = iopts[j].min;
              }
              if (v > iopts[j].max) {
                v = iopts[j].max;
              }
              printf("info setting %s to %d\n", iopts[j].name, v);
              *(iopts[j].var) = v;

              if (strcmp(name+1, "hash") == 0) {
                tt_resize_hashtable(HASH);
                printf("info string Hash table set to %d records of "
                       "%zu bytes each\n",
                       tt_get_num_of_records(), tt_get_bytes_per_record());
                printf("info string Total hash table size: %zu bytes\n",
                       tt_get_num_of_records() * tt_get_bytes_per_record());
              }
              break;
            }
          }
          if (!recognized) {
            fprintf(OUT, "info string %s not recognized\n", name+1);
          }
          continue;
        }
      }

      if (strcmp(tok[0], "help") == 0) {
        help();
        continue;
      }

      if (strcmp(tok[0], "display") == 0) {
        display(&gme[ix]);
        continue;
      }

      sortable_move_t  lst[MAX_NUM_MOVES];
      if (strcmp(tok[0], "generate") == 0) {
        int num_moves = generate_all(&gme[ix], lst, true);
        for (int i = 0; i < num_moves; ++i) {
          char buf[MAX_CHARS_IN_MOVE];
          move_to_str(get_move(lst[i]), buf, MAX_CHARS_IN_MOVE);
          printf("%s ", buf);
        }
        printf("\n");
        continue;
      }

      if (strcmp(tok[0], "eval") == 0) {
        if (token_count == 1) {  // evaluate current position
          score_t score = eval(&gme[ix], true);
          fprintf(OUT, "info score cp %d\n", score);
        } else {  // get and evaluate move
          victims_t victims = make_from_string(&gme[ix], &gme[ix+1], tok[1]);
          if (is_ILLEGAL(victims) || is_KO(victims)) {
            printf("Illegal move\n");
          } else {
            // evaluated from opponent's pov
            score_t score = - eval(&gme[ix+1], true);
            fprintf(OUT, "info score cp %d\n", score);
          }
        }
        continue;
      }

      if (strcmp(tok[0], "go") == 0) {
        double tme = 0.0;
        double inc = 0.0;
        int    depth = INF_DEPTH;
        double goal = INF_TIME;

        // process various tokens here
        for (int n = 1; n < token_count; n++) {
          if (strcmp(tok[n], "depth") == 0) {
            n++;
            depth = strtol(tok[n], (char **)NULL, 10);
            continue;
          }
          if (strcmp(tok[n], "time") == 0) {
            n++;
            tme = strtod(tok[n], (char **)NULL);
            continue;
          }
          if (strcmp(tok[n], "inc") == 0) {
            n++;
            inc = strtod(tok[n], (char **)NULL);
            continue;
          }
        }

        if (depth < INF_DEPTH) {
          UciBeginSearch(&gme[ix], depth, INF_TIME);
        } else {
          goal = tme * 0.02;   // use about 1/50 of main time
          goal += inc * 0.80;  // use most of increment
          // sanity check,  make sure that we don't run ourselves too low
          if (goal*10 > tme) goal = tme / 10.0;
          UciBeginSearch(&gme[ix], INF_DEPTH, goal);
        }
        continue;
      }

      if (strcmp(tok[0], "perft") == 0) {  // Test move generator
        // Correct output to depth 4
        // perft  1 78
        // perft  2 6084
        // perft  3 473126
        // perft  4 36767050


        int depth = 4;
        if (token_count >= 2) {  // Takes a depth argument to test deeper
          depth = strtol(tok[1], (char **)NULL, 10);
        }
        do_perft(gme, depth, 0);
        continue;
      }

      printf("Illegal command.  Use 'help' to see possible options.\n");
      continue;
    }
  }
  tt_free_hashtable();

  return 0;
}
//
//
// Copyright (c) 2015 MIT License by 6.172 Staff

#include "./search.h"

/*
The search module contains the alpha-beta search utilities for Leiserchess,
consisting of three main functions:

  1. searchRoot() is called from outside the module.
  2. scout_search() performs a null-window search.
  3. searchPV() performs the normal alpha-beta search.

searchRoot() calls scout_search() to order the moves.  searchRoot() then
calls searchPV() to perform the full search.  A sample parallel
implementation has been provided for you.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <cilk/cilk.h>
#include <cilk/reducer.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "./eval.h"
#include "./tt.h"
#include "./util.h"
#include "./fen.h"
#include "./tbassert.h"


// -----------------------------------------------------------------------------
// Preprocessor
// -----------------------------------------------------------------------------

#define ABORT_CHECK_PERIOD 0xfff

// -----------------------------------------------------------------------------
// READ ONLY settings (see iopt in leiserchess.c)
// -----------------------------------------------------------------------------

// eval score of a board state that is draw for both players.
int DRAW;

// POSITIONAL WEIGHTS evaluation terms
int HMB;  // having the move bonus

// Late-move reduction
int LMR_R1;    // Look at this number of moves full width before reducing 1 ply
int LMR_R2;    // After this number of moves reduce 2 ply

int USE_NMM;       // Null move margin
int TRACE_MOVES;   // Print moves
int DETECT_DRAWS;  // Detect draws by repetition

// do not set more than 5 ply
int FUT_DEPTH;     // set to zero for no futilty


// Declare the two main search functions.
static score_t searchPV(searchNode *node, int depth,
                        uint64_t *node_count_serial);
static score_t scout_search(searchNode *node, int depth,
                            uint64_t *node_count_serial);

// Include common search functions
#include "./search_globals.c"
#include "./search_common.c"
#include "./search_scout.c"

// Initializes a PV (principle variation node)
//   https://chessprogramming.wikispaces.com/Node+Types#PV
static void initialize_pv_node(searchNode* node, int depth) {
  node->type = SEARCH_PV;
  node->alpha = -node->parent->beta;
  node->orig_alpha = node->alpha;  // Save original alpha.
  node->beta = -node->parent->alpha;
  node->subpv = 0;
  node->depth = depth;
  node->legal_move_count = 0;
  node->ply = node->parent->ply + 1;
  node->fake_color_to_move = color_to_move_of(&(node->position));
  // point of view = 1 for white, -1 for black
  node->pov = 1 - node->fake_color_to_move * 2;
  node->quiescence = (depth <= 0);
  node->best_move_index = 0;
  node->best_score = -INF;
  node->abort = false;
}

// Perform a Principle Variation Search
//
// https://chessprogramming.wikispaces.com/Principal+Variation+Search
static score_t searchPV(searchNode *node, int depth, uint64_t *node_count_serial) {
  
  // Initialize the searchNode data structure.
  initialize_pv_node(node, depth);

  // Pre-evaluate the node to determine if we need to search further.
  leafEvalResult pre_evaluation_result = evaluate_as_leaf(node, SEARCH_PV);

  // use some information from the pre-evaluation
  int hash_table_move = pre_evaluation_result.hash_table_move;

  if (pre_evaluation_result.type == MOVE_EVALUATED) {
    return pre_evaluation_result.score;
  }
  if (pre_evaluation_result.score > node->best_score) {
    node->best_score = pre_evaluation_result.score;
    if (node->best_score > node->alpha) {
      node->alpha = node->best_score;
    }
  }

  // Get the killer moves at this node.
  move_t killer_a = killer[KMT(node->ply, 0)];
  move_t killer_b = killer[KMT(node->ply, 1)];


  // sortable_move_t move_list
  //
  // Contains a list of possible moves at this node. These moves are "sortable"
  //   and can be compared as integers. This is accomplished by using high-order
  //   bits to store a sort key.
  //
  // Keep track of the number of moves that we have considered at this node.
  //   After we finish searching moves at this node the move_list array will
  //   be organized in the following way:
  //
  //   m0, m1, ... , m_k-1, m_k, ... , m_N-1
  //
  //  where k = num_moves_tried, and N = num_of_moves
  //
  //  This will allow us to update the best_move_history table easily by
  //  scanning move_list from index 0 to k such that we update the table
  //  only for moves that we actually considered at this node.
  sortable_move_t move_list[MAX_NUM_MOVES];
  int num_of_moves = get_sortable_move_list(node, move_list, hash_table_move);
  int num_moves_tried = 0;

  sort_incremental(move_list, num_of_moves);
  // Start searching moves.

  for (int mv_index = 0; mv_index < num_of_moves; mv_index++) {
    // Incrementally sort the move list.

    move_t mv = get_move(move_list[mv_index]);

    num_moves_tried++;
    (*node_count_serial)++;

    
    moveEvaluationResult result = evaluateMove(node, mv, killer_a, killer_b,
                                               SEARCH_PV,
                                               node_count_serial);

    if (result.type == MOVE_ILLEGAL || result.type == MOVE_IGNORE) {
      continue;
    }

    // A legal move is a move that's not KO, but when we are in quiescence
    // we only want to count moves that has a capture.
    if (result.type == MOVE_EVALUATED) {
      node->legal_move_count++;
    }

    // Check if we should abort due to time control.
    if (abortf) {
      return 0;
    }

    
    bool cutoff = search_process_score(node, mv, mv_index, &result, SEARCH_PV);
    if (cutoff) {
      break;
    }

  }

  if (node->quiescence == false) {
    update_best_move_history(&(node->position), node->best_move_index,
                             move_list, num_moves_tried);
  }

  tbassert(abs(node->best_score) != -INF, "best_score = %d\n",
           node->best_score);

  // Update the transposition table.
  //
  // Note: This function reads node->best_score, node->orig_alpha,
  //   node->position.key, node->depth, node->ply, node->beta,
  //   node->alpha, node->subpv
  update_transposition_table(node);

  return node->best_score;
}

// -----------------------------------------------------------------------------
// searchRoot
//
// This handles scout search logic for the first level of the search tree
// -----------------------------------------------------------------------------
static void initialize_root_node(searchNode *node, score_t alpha, score_t beta, int depth,
                            int ply, position_t* p) {
  node->type = SEARCH_ROOT;
  node->alpha = alpha;
  node->beta = beta;
  node->depth = depth;
  node->ply = ply;
  node->position = *p;
  node->fake_color_to_move = color_to_move_of(&(node->position));
  node->best_score = -INF;
  node->pov = 1 - node->fake_color_to_move * 2;  // pov = 1 for White, -1 for Black
  node->abort = false;
}

score_t searchRoot(position_t *p, score_t alpha, score_t beta, int depth,
                   int ply, move_t *pv, uint64_t *node_count_serial,
                   FILE *OUT) {
  static int num_of_moves = 0;  // number of moves in list
  // hopefully, more than we will need
  static sortable_move_t move_list[MAX_NUM_MOVES];

  if (depth == 1) {
    // we are at depth 1; generate all possible moves
    num_of_moves = generate_all(p, move_list, false);
    // shuffle the list of moves
    // for (int i = 0; i < num_of_moves; i++) {
    //   int r = myrand() % (i + 1);
    //   sortable_move_t tmp = move_list[i];
    //   move_list[i] = move_list[r];
    //   move_list[r] = tmp;
    // }
    sort_incremental(move_list, num_of_moves);
  }

  // printf("?? %d %d\n", depth, num_of_moves);
  searchNode rootNode;
  rootNode.parent = NULL;
  initialize_root_node(&rootNode, alpha, beta, depth, ply, p);


  assert(rootNode.best_score == alpha);  // initial conditions

  searchNode next_node;
  next_node.subpv = 0;
  next_node.parent = &rootNode;

  score_t score;
  for (int mv_index = 0; mv_index < num_of_moves; mv_index++) {
    move_t mv = get_move(move_list[mv_index]);

    if (TRACE_MOVES) {
      print_move_info(mv, ply);
    }

    (*node_count_serial)++;

    // make the move.
    
    victims_t x = make_move(&(rootNode.position), &(next_node.position), mv);
    
    if (is_KO(x)) {
      continue;  // not a legal move
    }

    if (is_game_over(x, rootNode.pov, rootNode.ply)) {
      score = get_game_over_score(x, rootNode.pov, rootNode.ply);
      next_node.subpv = 0;
      goto scored;
    }

    if (is_repeated(&(next_node.position), rootNode.ply)) {
      score = get_draw_score(&(next_node.position), rootNode.ply);
      next_node.subpv = 0;
      goto scored;
    }
    if (mv_index == 0 || rootNode.depth == 1) {
      // We guess that the first move is the principle variation
      score = -searchPV(&next_node, rootNode.depth-1, node_count_serial);
      
      // Check if we should abort due to time control.
      if (abortf) {
        return 0;
      }
    } else {
      score = -scout_search(&next_node, rootNode.depth-1, node_count_serial);
      // Check if we should abort due to time control.
      if (abortf) {
        return 0;
      }

      // If its score exceeds the current best score,
      if (score > rootNode.alpha) {
        score = -searchPV(&next_node, rootNode.depth-1, node_count_serial);
        // Check if we should abort due to time control.
        if (abortf) {
          return 0;
        }
      }
    }

  scored:
    // only valid for the root node:
    tbassert((score > rootNode.best_score) == (score > rootNode.alpha),
             "score = %d, best = %d, alpha = %d\n", score, rootNode.best_score, rootNode.alpha);

    if (score > rootNode.best_score) {
      tbassert(score > rootNode.alpha, "score: %d, alpha: %d\n", score, rootNode.alpha);

      rootNode.best_score = score;
      *pv = mv;
      // memcpy(pv+1, next_node.subpv, sizeof(move_t) * (MAX_PLY_IN_SEARCH - 1));
      // pv[MAX_PLY_IN_SEARCH - 1] = 0;

      // Print out based on UCI (universal chess interface)
      double et = elapsed_time();
      char   pvbuf[MAX_CHARS_IN_MOVE];
      getPV(*pv, pvbuf, MAX_CHARS_IN_MOVE);
      if (et < 0.00001) {
        et = 0.00001;  // hack so that we don't divide by 0
      }

      uint64_t nps = 1000 * *node_count_serial / et;
      fprintf(OUT, "info depth %d move_no %d time (microsec) %d nodes %" PRIu64
              " nps %" PRIu64 "\n",
              depth, mv_index + 1, (int) (et * 1000), *node_count_serial, nps);
      fprintf(OUT, "info score cp %d pv %s\n", score, pvbuf);

      // Slide this move to the front of the move list
      for (int j = mv_index; j > 0; j--) {
        move_list[j] = move_list[j - 1];
      }
      move_list[0] = mv;
    }

    // Normal alpha-beta logic: if the current score is better than what the
    // maximizer has been able to get so far, take that new value.  Likewise,
    // score >= beta is the beta cutoff condition
    if (score > rootNode.alpha) {
      rootNode.alpha = score;
    }
    if (score >= rootNode.beta) {
      tbassert(0, "score: %d, beta: %d\n", score, rootNode.beta);
      break;
    }
  }

  return rootNode.best_score;
}
//
//
// Copyright (c) 2015 MIT License by 6.172 Staff

#include "./eval.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "./move_gen.h"
#include "./tbassert.h"
#include "./closebook.h"

// -----------------------------------------------------------------------------
// Evaluation
// -----------------------------------------------------------------------------

#define WINNING_SCORE 30000
typedef int32_t ev_score_t;  // Static evaluator uses "hi res" values

int RANDOMIZE;

int PCENTRAL;
int HATTACK;
int PBETWEEN;
int KFACE;
int KAGGRESSIVE;
int MOBILITY;
int PAWNPIN;

// Heuristics for static evaluation - described in the google doc
// mentioned in the handout.

static const ev_score_t pcentral_s[64] = {
125, 181, 220, 234, 234, 220, 181, 125,
181, 249, 302, 323, 323, 302, 249, 181,
220, 302, 375, 411, 411, 375, 302, 220,
234, 323, 411, 500, 500, 411, 323, 234,
234, 323, 411, 500, 500, 411, 323, 234,
220, 302, 375, 411, 411, 375, 302, 220,
181, 249, 302, 323, 323, 302, 249, 181,
125, 181, 220, 234, 234, 220, 181, 125};

static const uint64_t three_by_three_mask[100] = {
  0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 
  0ULL, 771ULL, 1799ULL, 3598ULL, 7196ULL, 14392ULL, 28784ULL, 57568ULL, 49344ULL, 0ULL, 
  0ULL, 197379ULL, 460551ULL, 921102ULL, 1842204ULL, 3684408ULL, 7368816ULL, 14737632ULL, 12632256ULL, 0ULL, 
  0ULL, 50529024ULL, 117901056ULL, 235802112ULL, 471604224ULL, 943208448ULL, 1886416896ULL, 3772833792ULL, 3233857536ULL, 0ULL, 
  0ULL, 12935430144ULL, 30182670336ULL, 60365340672ULL, 120730681344ULL, 241461362688ULL, 482922725376ULL, 965845450752ULL, 827867529216ULL, 0ULL, 
  0ULL, 3311470116864ULL, 7726763606016ULL, 15453527212032ULL, 30907054424064ULL, 61814108848128ULL, 123628217696256ULL, 247256435392512ULL, 211934087479296ULL, 0ULL, 
  0ULL, 847736349917184ULL, 1978051483140096ULL, 3956102966280192ULL, 7912205932560384ULL, 15824411865120768ULL, 31648823730241536ULL, 63297647460483072ULL, 54255126394699776ULL, 0ULL, 
  0ULL, 217020505578799104ULL, 506381179683864576ULL, 1012762359367729152ULL, 2025524718735458304ULL, 4051049437470916608ULL, 8102098874941833216ULL, 16204197749883666432ULL, 13889312357043142656ULL, 0ULL, 
  0ULL, 217017207043915776ULL, 506373483102470144ULL, 1012746966204940288ULL, 2025493932409880576ULL, 4050987864819761152ULL, 8101975729639522304ULL, 16203951459279044608ULL, 13889101250810609664ULL, 0ULL, 
  0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL};

static const double inv_s[16] = {1.0/1, 1.0/2, 1.0/3, 1.0/4, 1.0/5, 1.0/6, 1.0/7,
1.0/8, 1.0/9, 1.0/10, 1.0/11, 1.0/12, 1.0/13, 1.0/14, 1.0/15, 1.0/16};

// PCENTRAL heuristic: Bonus for Pawn near center of board
#define pcentral(x) pcentral_s[x]

// returns true if c lies on or between a and b, which are not ordered
static inline  bool between(int c, int a, int b) {
  bool x = ((c >= a) && (c <= b)) || ((c <= a) && (c >= b));
  return x;
}

// PBETWEEN heuristic: Bonus for Pawn at (f, r) in rectangle defined by Kings at the corners
// static inline  ev_score_t pbetween(position_t *p, fil_t f, rnk_t r) {
//   bool is_between =
//       between(f, fil_of(p->kloc[WHITE]), fil_of(p->kloc[BLACK])) &&
//       between(r, rnk_of(p->kloc[WHITE]), rnk_of(p->kloc[BLACK]));
//   return is_between ? PBETWEEN : 0;
// }


// KFACE heuristic: bonus (or penalty) for King facing toward the other King
static inline  ev_score_t kface(position_t *p, fil_t f, rnk_t r) {
  square_t sq = square_of(f, r);
  piece_t x = p->board[sq];
  color_t c = color_of(x);
  square_t opp_sq = p->kloc[opp_color(c)];
  int delta_fil = fil_of(opp_sq) - f;
  int delta_rnk = rnk_of(opp_sq) - r;
  int bonus;

  switch (ori_of(x)) {
    case NN:
      bonus = delta_rnk;
      break;

    case EE:
      bonus = delta_fil;
      break;

    case SS:
      bonus = -delta_rnk;
      break;

    case WW:
      bonus = -delta_fil;
      break;

    default:
      bonus = 0;
      tbassert(false, "Illegal King orientation.\n");
  }
  return (bonus * KFACE) * inv_s[abs(delta_rnk) + abs(delta_fil)-1];
}

// KAGGRESSIVE heuristic: bonus for King with more space to back
static inline  ev_score_t kaggressive(position_t *p, fil_t f, rnk_t r) {
  square_t sq = square_of(f, r);
  piece_t x = p->board[sq];
  color_t c = color_of(x);
  tbassert(ptype_of(x) == KING, "ptype_of(x) = %d\n", ptype_of(x));

  square_t opp_sq = p->kloc[opp_color(c)];
  fil_t of = fil_of(opp_sq);
  rnk_t _or = (rnk_t) rnk_of(opp_sq);

  int delta_fil = of - f;
  int delta_rnk = _or - r;

  int bonus = 0;

  if (delta_fil >= 0 && delta_rnk >= 0) {
    bonus = (f + 1) * (r + 1);
  } else if (delta_fil <= 0 && delta_rnk >= 0) {
    bonus = (BOARD_WIDTH - f) * (r + 1);
  } else if (delta_fil <= 0 && delta_rnk <= 0) {
    bonus = (BOARD_WIDTH - f) * (BOARD_WIDTH - r);
  } else if (delta_fil >= 0 && delta_rnk <= 0) {
    bonus = (f + 1) * (BOARD_WIDTH - r);
  }
  return (KAGGRESSIVE * bonus) / (BOARD_WIDTH * BOARD_WIDTH);
}

// Marks the path/line-of-sight of the laser until it hits a piece or goes off
// the board.
//
// p : Current board state.
// c : Color of king shooting laser.
// laser_map : End result will be stored here. Every square on the
//             path of the laser is marked with mark_mask.
// mark_mask : What each square is marked with.

// directions for laser: NN, EE, SS, WW
static const int beam_64[NUM_ORI] = {1, 8, -1, -8};

uint64_t mark_laser_path_bit(position_t *p, color_t c) {
  // static int count = 0;
  // count += 1;
  // if (count % 10000 == 0)
  //   printf("%d\n", count);
  square_t sq = p->kloc[c];
  int loc64 = fil_of(sq) * 8 + rnk_of(sq);
  uint64_t laser_map = 0;
  int bdir = ori_of(p->board[sq]);
  p->kill_d[c] = false;

  tbassert(ptype_of(p->board[sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[sq]));
  laser_map |= (1ULL << loc64);
  
  while (true) {
    sq += beam_of(bdir);
    int x = ptype_of(p->board[sq]);
    if (x == INVALID)
      return laser_map;
    // tbassert(sq < ARR_SIZE && sq >= 0, "sq: %d\n", sq);
    loc64 += beam_64[bdir];
    laser_map |= (1ULL << loc64);
    if (x == KING) {
      p->kill_d[c] = true;
      return laser_map;
    }
    if (x == PAWN) {
      bdir = reflect_of(bdir, ori_of(p->board[sq]));
      if (bdir < 0) {  // Hit back of Pawn
        p->kill_d[c] = true;
        return laser_map;
      }
    }
  }
  return laser_map;
}

// PAWNPIN Heuristic: count number of pawns that are not pinned by the
//   opposing king's laser --- and are thus mobile.

static inline int pawnpin(position_t *p, color_t color, uint64_t laser_map) {
  tbassert(p->mask[0] == compute_mask(p, 0),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[0], compute_mask(p, 0));
  tbassert(p->mask[1] == compute_mask(p, 1),
           "p->mask: %"PRIu64", mask: %"PRIu64"\n",
           p->mask[1], compute_mask(p, 1));

  uint64_t mask = (~laser_map) & p -> mask[color];
  return __builtin_popcountl(mask) - ((1ULL << (fil_of(p -> kloc[color]) * 8 + rnk_of(p -> kloc[color])) & mask) != 0);

}

// MOBILITY heuristic: safe squares around king of given color.

static inline int mobility(position_t *p, color_t color, uint64_t laser_map) {
  return __builtin_popcountl((~laser_map) & three_by_three_mask[p -> kloc[color]]);
}


// H_SQUARES_ATTACKABLE heuristic: for shooting the enemy king
static inline int h_squares_attackable(position_t *p, color_t c, uint64_t laser_map) {

  square_t o_king_sq = p->kloc[opp_color(c)];
  tbassert(ptype_of(p->board[o_king_sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[o_king_sq]));
  tbassert(color_of(p->board[o_king_sq]) != c,
           "color: %d\n", color_of(p->board[o_king_sq]));

  float h_attackable = 0;

  int king_sq_fil = fil_of(o_king_sq);
  int king_sq_rnk = rnk_of(o_king_sq);

  while (laser_map) {
    uint64_t y = laser_map & (-laser_map);
    int i = LOG2(y);
    h_attackable += inv_s[abs(king_sq_fil - (i >> 3))] + inv_s[abs(king_sq_rnk - (i & 7))];
    laser_map ^= y;
  }

  return h_attackable;
}

// Static evaluation.  Returns score
score_t eval(position_t *p, bool verbose) {
  // seed rand_r with a value of 1, as per
  // http://linux.die.net/man/3/rand_r

  static __thread unsigned int seed = 1;
  
  int t = checkEndGame(p);
  if (t==1 || t==2)
  {
	  if (t==1) return WINNING_SCORE; else return -WINNING_SCORE;
  }
  

  ev_score_t score = 0;
  
  fil_t f0 = fil_of(p -> kloc[0]);
  rnk_t r0 = rnk_of(p -> kloc[0]);
  fil_t f1 = fil_of(p -> kloc[1]);
  rnk_t r1 = rnk_of(p -> kloc[1]);
  score += kface(p, f0, r0) + kaggressive(p, f0, r0);
  score -= pcentral(f0 * 8 + r0);

  score -= kface(p, f1, r1) + kaggressive(p, f1, r1);
  score += pcentral(f1 * 8 + r1);


  uint64_t mask = p -> mask[0];
  while (mask) {
    uint64_t y = mask & (-mask);
    mask ^= y;
    uint8_t i = LOG2(y);
    fil_t f = (i >> 3);
    rnk_t r = i & 7;
    score += PAWN_EV_VALUE;
    // score += pbetween(p, f, r);
    score += (between(f, f0, f1) && between(r, r0, r1)) ? PBETWEEN : 0;
    // score += pcentral(f, r);
    score += pcentral(i);
  }
  mask = p -> mask[1];
  while (mask) {
    uint64_t y = mask & (-mask);
    mask ^= y;
    uint8_t i = LOG2(y);
    fil_t f = (i >> 3);
    rnk_t r = i & 7;
    score -= PAWN_EV_VALUE;
    // score -= pbetween(p, f, r);
    score -= (between(f, f0, f1) && between(r, r0, r1)) ? PBETWEEN : 0;
    score -= pcentral(i);
  }

  uint64_t laser_WHITE = p -> laser[0];
  uint64_t laser_BLACK = p -> laser[1];
  // if (laser_WHITE != p -> laser[0])
  //   printf("error\n");
  
  // H_SQUARES_ATTACKABLE heuristic
  ev_score_t w_hattackable = HATTACK * h_squares_attackable(p, WHITE, laser_WHITE);
  score += w_hattackable;
  // if (verbose) {
  //   printf("HATTACK bonus %d for White\n", w_hattackable);
  // }
  ev_score_t b_hattackable = HATTACK * h_squares_attackable(p, BLACK, laser_BLACK);
  score -= b_hattackable;
  // if (verbose) {
  //   printf("HATTACK bonus %d for Black\n", b_hattackable);
  // }

  // MOBILITY heuristic
  int w_mobility = MOBILITY * mobility(p, WHITE, laser_BLACK);
  score += w_mobility;
  // if (verbose) {
  //   printf("MOBILITY bonus %d for White\n", w_mobility);
  // }
  int b_mobility = MOBILITY * mobility(p, BLACK, laser_WHITE);
  score -= b_mobility;
  // if (verbose) {
  //   printf("MOBILITY bonus %d for Black\n", b_mobility);
  // }

  // PAWNPIN heuristic --- is a pawn immobilized by the enemy laser.
  int w_pawnpin = PAWNPIN * pawnpin(p, WHITE, laser_BLACK);
  score += w_pawnpin;
  int b_pawnpin = PAWNPIN * pawnpin(p, BLACK, laser_WHITE);
  score -= b_pawnpin;

  // score from WHITE point of view
  // ev_score_t tot = score[WHITE] - score[BLACK];

  if (RANDOMIZE) {
    ev_score_t  z = rand_r(&seed) % (RANDOMIZE*2+1);
    score = score + z - RANDOMIZE;
  }

  if (color_to_move_of(p) == BLACK) {
    score = -score;
  }

  return score / EV_SCORE_RATIO;
}
  
//
//
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

// static const uint64_t sq_to_board_bit[100] = {
// 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL,
// 0ULL, 1ULL<<0, 1ULL<<1, 1ULL<<2, 1ULL<<3, 1ULL<<4, 1ULL<<5, 1ULL<<6, 1ULL<<7, 0ULL,
// 0ULL, 1ULL<<8, 1ULL<<9, 1ULL<<10, 1ULL<<11, 1ULL<<12, 1ULL<<13, 1ULL<<14, 1ULL<<15, 0ULL,
// 0ULL, 1ULL<<16, 1ULL<<17, 1ULL<<18, 1ULL<<19, 1ULL<<20, 1ULL<<21, 1ULL<<22, 1ULL<<23, 0ULL,
// 0ULL, 1ULL<<24, 1ULL<<25, 1ULL<<26, 1ULL<<27, 1ULL<<28, 1ULL<<29, 1ULL<<30, 1ULL<<31, 0ULL,
// 0ULL, 1ULL<<32, 1ULL<<33, 1ULL<<34, 1ULL<<35, 1ULL<<36, 1ULL<<37, 1ULL<<38, 1ULL<<39, 0ULL,
// 0ULL, 1ULL<<40, 1ULL<<41, 1ULL<<42, 1ULL<<43, 1ULL<<44, 1ULL<<45, 1ULL<<46, 1ULL<<47, 0ULL,
// 0ULL, 1ULL<<48, 1ULL<<49, 1ULL<<50, 1ULL<<51, 1ULL<<52, 1ULL<<53, 1ULL<<54, 1ULL<<55, 0ULL,
// 0ULL, 1ULL<<56, 1ULL<<57, 1ULL<<58, 1ULL<<59, 1ULL<<60, 1ULL<<61, 1ULL<<62, 1ULL<<63, 0ULL,
// 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL, 0ULL};

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
  // uint64_t laser_map = mark_laser_path_bit(p, opp_color(color_to_move));
  int move_count = 0;
  uint64_t mask = p -> mask[color_to_move] & ~p -> laser[opp_color(color_to_move)];
  while (mask) {
    uint64_t y = mask & (-mask);
    mask ^= y;
    int i = LOG2(y);
    fil_t f = (i >> 3);
    rnk_t r = i & 7;

    square_t sq = (f + FIL_ORIGIN) * ARR_WIDTH + r + RNK_ORIGIN;
    piece_t x = p->board[sq];

    ptype_t typ = ptype_of(x);
    // color_t color = color_of(x);

    if (typ == PAWN || typ == KING) {
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
    }
  }

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
static inline square_t fire_laser(position_t *p, color_t c) {
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

// bool check_zero_victims(position_t *old, move_t mv) {
//   if (old -> laser[0] != mark_laser_path_bit(old, 0))
//     printf("error\n");
//   if (old -> laser[1] != mark_laser_path_bit(old, 1))
//     printf("error\n");
//   return false;
// }

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

    
  // printf("?? %d %d\n", from_sq, to_sq);

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

  // p->laser[0] = mark_laser_path_bit(p, 0);
  // p->laser[1] = mark_laser_path_bit(p, 1);

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
  
  // static int count = 0, cnt = 0;
  // count += 1;

  
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
  

  // square_t from_sq = from_square(mv);
  // square_t to_sq = to_square(mv);

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
  // printf("in\n");
  if (!(p->victims & 128)) {
    p -> laser[0] = mark_laser_path_bit(p, 0);
    p -> laser[1] = mark_laser_path_bit(p, 1);
  }
  // printf("out\n");
  return p->victims;
}

victims_t make_move2(position_t *old, position_t *p, move_t mv) {

  tbassert(mv != 0, "mv was zero.\n");

  WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);

  // move phase 1 - moving a piece
  low_level_make_move(old, p, mv);
  
  //================================================

  // move phase 2 - shooting the laser
  square_t victim_sq = 0;
  p->victims = 0;
  
  // static int count = 0, cnt = 0;
  // count += 1;

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

  // square_t from_sq = from_square(mv);
  // square_t to_sq = to_square(mv);

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

  np.laser[0] = mark_laser_path_bit(&np, 0);
  np.laser[1] = mark_laser_path_bit(&np, 1);
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
//
//
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

//
//
// Copyright (c) 2015 MIT License by 6.172 Staff

#include "./fen.h"

#include <stdbool.h>
#include <stdio.h>

#include "./move_gen.h"
#include "./eval.h"
#include "./tbassert.h"

static void fen_error(char *fen, int c_count, char *msg) {
  fprintf(stderr, "\nError in FEN string:\n");
  fprintf(stderr, "   %s\n  ", fen);  // Indent 3 spaces
  for (int i = 0; i < c_count; ++i) {
    fprintf(stderr, " ");
  }
  fprintf(stderr, "^\n");  // graphical pointer to error character in string
  fprintf(stderr, "%s\n", msg);
}

// parse_fen_board
// Input:   board representation as a fen string
//          unpopulated board position struct
// Output:  index of where board description ends or 0 if parsing error
//          (populated) board position struct
static int parse_fen_board(position_t *p, char *fen) {
  // Invariant: square (f, r) is last square filled.
  // Fill from last rank to first rank, from first file to last file
  fil_t f = -1;
  rnk_t r = BOARD_WIDTH - 1;

  // Current and next characters from input FEN description
  char c, next_c;

  // Invariant: fen[c_count] is next character to be read
  int c_count = 0;

  // Loop also breaks internally if (f, r) == (BOARD_WIDTH-1, 0)
  while ((c = fen[c_count++]) != '\0') {
    int ori;
    ptype_t typ;

    switch (c) {
      // ignore whitespace until the end
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        if ((f == BOARD_WIDTH - 1) && (r == 0)) {  // our job is done
          return c_count;
        }
        break;

        // digits
      case '1':
        if (fen[c_count] == '0') {
          c_count++;
          c += 9;
        }
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        while (c > '0') {
          if (++f >= BOARD_WIDTH) {
            fen_error(fen, c_count, "Too many squares in rank.\n");
            return 0;
          }
          set_ptype(&p->board[square_of(f, r)], EMPTY);
          c--;
        }
        break;

        // pieces
      case 'N':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'N') {  // White King facing North
          ori = NN;
          typ = KING;
        } else if (next_c == 'W') {  // White Pawn facing NW
          ori = NW;
          typ = PAWN;
        } else if (next_c == 'E') {  // White Pawn facing NE
          ori = NE;
          typ = PAWN;
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        set_ptype(&p->board[square_of(f, r)], typ);
        set_color(&p->board[square_of(f, r)], WHITE);
        set_ori(&p->board[square_of(f, r)], ori);
        break;

      case 'n':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'n') {  // Black King facing North
          ori = NN;
          typ = KING;
        } else if (next_c == 'w') {  // Black Pawn facing NW
          ori = NW;
          typ = PAWN;
        } else if (next_c == 'e') {  // Black Pawn facing NE
          ori = NE;
          typ = PAWN;
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        set_ptype(&p->board[square_of(f, r)], typ);
        set_color(&p->board[square_of(f, r)], BLACK);
        set_ori(&p->board[square_of(f, r)], ori);
        break;

      case 'S':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'S') {  // White King facing SOUTH
          ori = SS;
          typ = KING;
        } else if (next_c == 'W') {  // White Pawn facing SW
          ori = SW;
          typ = PAWN;
        } else if (next_c == 'E') {  // White Pawn facing SE
          ori = SE;
          typ = PAWN;
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        set_ptype(&p->board[square_of(f, r)], typ);
        set_color(&p->board[square_of(f, r)], WHITE);
        set_ori(&p->board[square_of(f, r)], ori);
        break;

      case 's':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 's') {  // Black King facing South
          ori = SS;
          typ = KING;
        } else if (next_c == 'w') {  // Black Pawn facing SW
          ori = SW;
          typ = PAWN;
        } else if (next_c == 'e') {  // Black Pawn facing SE
          ori = SE;
          typ = PAWN;
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        set_ptype(&p->board[square_of(f, r)], typ);
        set_color(&p->board[square_of(f, r)], BLACK);
        set_ori(&p->board[square_of(f, r)], ori);
        break;

      case 'E':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'E') {  // White King facing East
          set_ptype(&p->board[square_of(f, r)], KING);
          set_color(&p->board[square_of(f, r)], WHITE);
          set_ori(&p->board[square_of(f, r)], EE);
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        break;

      case 'W':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'W') {  // White King facing West
          set_ptype(&p->board[square_of(f, r)], KING);
          set_color(&p->board[square_of(f, r)], WHITE);
          set_ori(&p->board[square_of(f, r)], WW);
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        break;

      case 'e':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'e') {  // Black King facing East
          set_ptype(&p->board[square_of(f, r)], KING);
          set_color(&p->board[square_of(f, r)], BLACK);
          set_ori(&p->board[square_of(f, r)], EE);
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        break;

      case 'w':
        if (++f >= BOARD_WIDTH) {
          fen_error(fen, c_count, "Too many squares in rank");
          return 0;
        }
        next_c = fen[c_count++];

        if (next_c == 'w') {  // Black King facing West
          set_ptype(&p->board[square_of(f, r)], KING);
          set_color(&p->board[square_of(f, r)], BLACK);
          set_ori(&p->board[square_of(f, r)], WW);
        } else {
          fen_error(fen, c_count+1, "Syntax error");
          return 0;
        }
        break;

        // end of rank
      case '/':
        if (f == BOARD_WIDTH - 1) {
          f = -1;
          if (--r < 0) {
            fen_error(fen, c_count, "Too many ranks");
            return 0;
          }
        } else {
          fen_error(fen, c_count, "Too few squares in rank");
          return 0;
        }
        break;

      default:
        fen_error(fen, c_count, "Syntax error");
        return 0;
        break;
    }  // end switch
  }  // end while

  if ((f == BOARD_WIDTH - 1) && (r == 0)) {
    return c_count;
  } else {
    fen_error(fen, c_count, "Too few squares specified");
    return 0;
  }
}

// returns 0 if no error
static int get_sq_from_str(char *fen, int *c_count, int *sq) {
  char c, next_c;

  while ((c = fen[*c_count++]) != '\0') {
    // skip whitespace
    if ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r')) {
      continue;
    } else {
      break;  // found nonwhite character
    }
  }

  if (c == '\0') {
    *sq = 0;
    return 0;
  }

  // get file and rank
  if ((c - 'a' < 0) || (c - 'a') > BOARD_WIDTH) {
    fen_error(fen, *c_count, "Illegal specification of last move");
    return 1;
  }
  next_c = fen[*c_count++];
  if (next_c == '\0') {
    fen_error(fen, *c_count, "FEN ended before last move fully specified");

    if ((next_c - '0' < 0) || (next_c - '0') > BOARD_WIDTH) {
      fen_error(fen, *c_count, "Illegal specification of last move");
      return 1;
    }

    *sq = square_of(c - 'a', next_c - '0');
    return 0;
  }
  return 0;
}

// Translate a fen string into a board position struct
//
int fen_to_pos(position_t *p, char *fen) {
  static  position_t dmy1, dmy2;

  // these sentinels simplify checking previous
  // states without stepping past null pointers.
  dmy1.key = 0;
  dmy1.victims = 1;
  dmy1.history = NULL;

  dmy2.key = 0;
  dmy2.victims = 1;
  dmy2.history = &dmy1;


  p->key = 0;          // hash key
  p->victims = 0;
  p->history = &dmy2;  // history


  if (fen[0] == '\0') {  // Empty FEN => use starting position
    fen = "ss3nw3/3nw4/2nw1nw3/1nw3SE1SE/nw1nw3SE1/3SE1SE2/4SE3/3SE3NN W";
  }

  int c_count = 0;  // Invariant: fen[c_count] is next char to be read

  for (int i = 0; i < ARR_SIZE; ++i) {
    p->board[i] = 0;
    set_ptype(&p->board[i], INVALID);  // squares are invalid until filled
  }

  c_count = parse_fen_board(p, fen);
  if (!c_count) {
    return 1;  // parse error of board
  }

  // King check

  int Kings[2] = {0, 0};
  for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
    for (rnk_t r = 0; r < BOARD_WIDTH; ++r) {
      square_t sq = square_of(f, r);
      piece_t x = p->board[sq];
      ptype_t typ = ptype_of(x);
      if (typ == KING) {
        Kings[color_of(x)]++;
        p->kloc[color_of(x)] = sq;
      }
    }
  }

  if (Kings[WHITE] == 0) {
    fen_error(fen, c_count, "No White Kings");
    return 1;
  } else if (Kings[WHITE] > 1) {
    fen_error(fen, c_count, "Too many White Kings");
    return 1;
  } else if (Kings[BLACK] == 0) {
    fen_error(fen, c_count, "No Black Kings");
    return 1;
  } else if (Kings[BLACK] > 1) {
    fen_error(fen, c_count, "Too many Black Kings");
    return 1;
  }

  char c;
  bool done = false;
  // Look for color to move and set ply accordingly
  while (!done && (c = fen[c_count++]) != '\0') {
    switch (c) {
      // ignore whitespace until the end
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        break;

        // White to move
      case 'W':
      case 'w':
        p->ply = 0;
        done = true;
        break;

        // Black to move
      case 'B':
      case 'b':
        p->ply = 1;
        done = true;
        break;

      default:
        fen_error(fen, c_count, "Must specify White (W) or Black (B) to move");
        return 1;
        break;
    }
  }

  // Look for last move, if it exists
  int lm_from_sq, lm_to_sq, lm_rot;
  if (get_sq_from_str(fen, &c_count, &lm_from_sq) != 0) {  // parse error
    return 1;
  }
  if (lm_from_sq == 0) {   // from-square of last move
    p->last_move = 0;  // no last move specified
    p->key = compute_zob_key(p);
    p->mask[0] = compute_mask(p, 0);
    p->mask[1] = compute_mask(p, 1);
    p->laser[0] = mark_laser_path_bit(p, 0);
    p->laser[1] = mark_laser_path_bit(p, 1);
    return 0;
  }

  c = fen[c_count];

  switch (c) {
    case 'R':
    case 'r':
      lm_to_sq = lm_from_sq;
      lm_rot = RIGHT;
      break;
    case 'U':
    case 'u':
      lm_to_sq = lm_from_sq;
      lm_rot = UTURN;
      break;
    case 'L':
    case 'l':
      lm_to_sq = lm_from_sq;
      lm_rot = LEFT;
      break;

    default:  // Not a rotation
      lm_rot = NONE;
      if (get_sq_from_str(fen, &c_count, &lm_to_sq) != 0) {
        return 1;
      }
      break;
  }
  p->last_move = move_of(EMPTY, lm_rot, lm_from_sq, lm_to_sq);
  p->key = compute_zob_key(p);
  p->mask[0] = compute_mask(p, 0);
  p->mask[1] = compute_mask(p, 1);
  
  return 0;  // everything is okay
}

// King orientations
// This code is duplicated from move_gen.c

// Translate a position struct into a fen string
// NOTE: When you use the test framework in search.c, you should modify this
// function to match your optimized board representation in move_gen.c
//
// Input:   (populated) position struct
//          empty string where FEN characters will be written
// Output:  null
int pos_to_fen(position_t *p, char *fen) {
  int pos = 0;
  int i;

  for (rnk_t r = BOARD_WIDTH - 1; r >=0 ; --r) {
    int empty_in_a_row = 0;
    for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
      square_t sq = square_of(f, r);

      if (ptype_of(p->board[sq]) == INVALID) {     // invalid square
        tbassert(false, "Bad news, yo.\n");        // This is bad!
      }

      if (ptype_of(p->board[sq]) == EMPTY) {       // empty square
        empty_in_a_row++;
        continue;
      } else {
        if (empty_in_a_row) fen[pos++] = '0' + empty_in_a_row;
        empty_in_a_row = 0;

        int ori = ori_of(p->board[sq]);  // orientation
        color_t c = color_of(p->board[sq]);

        if (ptype_of(p->board[sq]) == KING) {
          for (i = 0; i < 2; i++) fen[pos++] = king_ori_to_rep[c][ori][i];
          continue;
        }

        if (ptype_of(p->board[sq]) == PAWN) {
          for (i = 0; i < 2; i++) fen[pos++] = pawn_ori_to_rep[c][ori][i];
          continue;
        }
      }
    }
    // assert: for larger boards, we need more general solns
    tbassert(BOARD_WIDTH <= 10, "BOARD_WIDTH = %d\n", BOARD_WIDTH);
    if (empty_in_a_row == 10) {
      fen[pos++] = '1';
      fen[pos++] = '0';
    } else if (empty_in_a_row) {
      fen[pos++] = '0' + empty_in_a_row;
    }
    if (r) fen[pos++] = '/';
  }
  fen[pos++] = ' ';
  fen[pos++] = 'W';
  fen[pos++] = '\0';

  return pos;
}
//
//
// Copyright (c) 2015 MIT License by 6.172 Staff

#include "./util.h"

#include <stdarg.h>
#include <stdio.h>

#ifndef MACPORT
#include <sys/time.h>
#endif

#include <time.h>

#include <stdint.h>
#include <stdlib.h>

void debug_log(int log_level, const char *errstr, ...) {
  if (log_level >= DEBUG_LOG_THRESH) {
    va_list arg_list;
    va_start(arg_list, errstr);
    vfprintf(stderr, errstr, arg_list);
    va_end(arg_list);
    fprintf(stderr, "\n");
  }
}

double milliseconds() {
#if MACPORT
  static mach_timebase_info_data_t timebase;
  int r __attribute__((unused));
  r = mach_timebase_info(&timebase);
  fasttime_t t = gettime();
  double ns = (double)t * timebase.numer / timebase.denom;
  return ns*1e-6;
#else
  struct timespec mtime;
  clock_gettime(CLOCK_MONOTONIC, &mtime);
  double result = 1000.0 * mtime.tv_sec;
  result += mtime.tv_nsec / 1000000.0;
  return result;
#endif
}

// Public domain code for JLKISS64 RNG - long period KISS RNG producing
// 64-bit results
uint64_t myrand() {
  static int first_time = 1;
  // Seed variables
  static uint64_t x = 123456789123ULL, y = 987654321987ULL;
  static unsigned int z1 = 43219876, c1 = 6543217, z2 = 21987643,
      c2 = 1732654;  // Seed variables
  static uint64_t t;

  if (first_time) {
    int  i;
    FILE *f = fopen("/dev/urandom", "r");
    for (i = 0; i < 64; i += 8) {
      x = x ^ getc(f) << i;
      y = y ^ getc(f) << i;
    }

    fclose(f);
    first_time = 0;
  }

  x = 1490024343005336237ULL * x + 123456789;

  y ^= y << 21;
  y ^= y >> 17;
  y ^= y << 30;  // Do not set y=0!

  t = 4294584393ULL * z1 + c1;
  c1 = t >> 32;
  z1 = t;

  t = 4246477509ULL * z2 + c2;
  c2 = t >> 32;
  z2 = t;

  return x + y + z1 + ((uint64_t)z2 << 32);  // Return 64-bit result
}
