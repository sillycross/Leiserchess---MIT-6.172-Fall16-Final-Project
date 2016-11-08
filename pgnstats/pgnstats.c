// Copyright (c) 2015 MIT License by 6.172 Staff

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>
// #include <windows.h>

#define LAST_MOVE_NUMBER 80

typedef struct {
  char name[64];
  int  games;
  int64_t  tt;
  double  ts;       // time in seconds PER MOVE
  double  nm;       // nodes in millions
  int  depth;       // depth achieved
  int64_t moves;  // total moves
  int64_t nodes;  // total nodes
} player_t;

player_t  players[1024];
int       pc = 0;

#define Xisdigit(x) ((x) >= 48 && (x) <= 57)


int ParseString(char *ps, char *fld[]) {
  int   field_count = 0;
  char  *tok;
  int   i;
  char  *saveptr;

  field_count = 0;
  for (i = 0, tok = strtok_r(ps, " \t\n\r", &saveptr);
       tok != NULL;
       tok = strtok_r(NULL, " \t\n\r", &saveptr), i++) {
     fld[i] = tok;
     field_count++;
  }

  fld[ field_count ] = NULL;
  return field_count;
}


int main(int argc, char *argv[]) {
  int             i;
  int             j;
  int             ctm = 0;
  FILE            *f;
  char            s[4096];
  char            *x;
  int             tc;
  char            *tok[1024];
  player_t        *who[2];
  char            ref[64];   // reference player if not specified
  int             rix = 0;   // reference index
  int             mn = 0;    // move number of game being parsed
  int64_t   tq[32];
  int64_t   nq[32];
  int             dq[32];
  int             q = 0;
  char            *saveptr;

  printf("\n");
  f = fopen(argv[1], "r");

  snprintf(ref, sizeof(ref), "");
  if (argc > 2) {
    snprintf(ref, sizeof(ref), "%s", argv[2]);
  }

  int ln = 0;
  while (fgets(s, 4096, f) != NULL) {
    ln++;
    if (strncmp(s+1, "White ", 6) == 0) {
      x = strtok_r(s+1, "\"", &saveptr);
      x = strtok_r(NULL, "\"", &saveptr);
      for (i = 0; i < pc; i++) if (strcmp(players[i].name, x) == 0) break;

      if (i == pc) {  // player not found,  create a new entry
        snprintf(players[pc].name, sizeof(players[pc].name), "%s", x);
        players[pc].tt = 0ull;
        players[pc].games = 0;
        players[pc].depth = 0;
        players[pc].moves = 0;
        pc++;
      }
      who[0] = players + i;
      who[0]->games++;
      continue;
    }

    if (strncmp(s+1, "Black ", 6) == 0) {
      x = strtok_r(s+1, "\"", &saveptr);
      x = strtok_r(NULL, "\"", &saveptr);
      for (i = 0; i < pc; i++) if (strcmp(players[i].name, x) == 0) break;

      if (i == pc) {  // player not found, create a new entry
        snprintf(players[pc].name, sizeof(players[pc].name), "%s", x);
        players[pc].tt = 0ull;
        players[pc].games = 0;
        players[pc].depth = 0;
        players[pc].moves = 0;
        pc++;
      }
      who[1] = players + i;
      who[1]->games++;
      continue;
    }

    tc = ParseString(s, tok);

    for (i = 0; i < tc; i++) {
      if (Xisdigit(tok[i][0])
          && tok[i][1] != '}'
          && tok[i][2] != '}'
          && tok[i][3] != '}'
          && tok[i][4] != '}') {
        mn = strtol(tok[i], (char **)NULL, 10);
        ctm = 0;  // always white to move after a move number in PGN file
        continue;
      }

      if (mn >= LAST_MOVE_NUMBER) continue;

      if (tok[i][0] == '{') {
        tq[q & 31] = strtoll(tok[i] + 1, (char **)NULL, 10);
        dq[q & 31] = strtol(tok[i+1], (char **)NULL, 10);
        nq[q & 31] = strtoll(tok[i+2], (char **)NULL, 10);
        q++;
        i += 2;  // we consumed 2 additional tokens

        if (mn < 14) {
          ctm = 1;
          continue;
        }

        who[ctm]->tt += tq[(q-17) & 31];
        who[ctm]->depth += dq[(q-17) & 31];
        who[ctm]->nodes += nq[(q-17) & 31];
        who[ctm]->moves++;

        /*
        if (dq[ (q-17) & 31 ] != 10) {
          printf("Line number: %d move number = %d\n", ln, mn);
          exit(0);
        }
        */

        ctm = 1;
      }
    }
  }

  for (i = 0; i < pc; i++) {
    double se = players[i].tt / 1000000000.0;  // convert to seconds
    players[i].ts = se / (double) players[i].moves;
    players[i].nm = players[i].nodes / 1000000.0 / (double) players[i].moves;
  }

  for (i = 0; i < pc-1; i++)
    for (j = i+1; j < pc; j++)
      if (players[j].ts < players[i].ts) {
        player_t  tmp = players[i];
        players[i] = players[j];
        players[j] = tmp;
      }

  rix= 0;

  int  biggest = 0;
  for (i = 0; i < pc; i++) {
    int  cc = strlen(players[i].name);
    if (cc > biggest) biggest = cc;
    if (0 == strcmp(players[i].name, ref))
      rix = i;
  }

  char dsh[52] = "--------------------------------------------------";

  if (biggest > 50) biggest = 50;
  dsh[biggest] = 0;

  printf("      TIME       RATIO    log(r)     NODES    log(r)  ave DEPTH    GAMES   PLAYER\n");
  printf(" ---------  ----------  --------  --------  --------  ---------  -------   %s\n", dsh);

  for (i = 0; i < pc; i++) {
    printf("%10.4f  %10.3f  %8.3f  %8.3f  %8.3f  %9.4f  %7d   %s\n",
           players[i].ts,
           players[i].ts / players[rix].ts,
           log(players[i].ts / players[rix].ts),
           players[i].nm,
           log(players[i].nm / players[rix].nm),
           players[i].depth  / (double) players[i].moves,
           players[i].games,
           players[i].name);
  }

  printf("\n");
  return 0;
}
