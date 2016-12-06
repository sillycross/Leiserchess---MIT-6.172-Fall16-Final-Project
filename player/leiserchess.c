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
  { "hash",                   &HASH,   16,                    1,              MAX_HASH   },
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
  score_t sc = 0;
  static score_t prev_cp = 0;

  for (int d = 1; d <= depth; d++) {  // Iterative deepening
    reset_abort();

    sc = searchRoot(p, -INF, INF, d, 0, &subpv, &node_count_serial,
                OUT, prev_cp);

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

  prev_cp = sc;

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
  entry_point(&args);

  char bms[MAX_CHARS_IN_MOVE];
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
