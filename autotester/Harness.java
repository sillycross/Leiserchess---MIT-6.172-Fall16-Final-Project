/**
 * Copyright (c) 2012--2014 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

import  java.io.*;
import  java.util.*;
import  java.util.regex.Pattern;
import  java.util.regex.Matcher;


public class Harness {

    static String version = "lauto version 1.0";
    static String  base;      // base name of test file
    static HashMap<String, Player>  pl = new HashMap<String, Player>();
    static String opening_book = "";
    static int game_rounds = 0;
    static int adjudicate = 400;             // default number of moves to adjudicate as a draw
    static int totalPlayers = 0;
    static String[] sname = new String[512]; // map id to player name
    static int[] count = new int[512];       // how many games as white for each player?
    static int gn = 0;
    static String pgnfile;
    public static BufferedWriter pgnwrite;
    private static long start_time;
    private static int  cycle = 0;
    private static int total_games = 0;
    private static final int DEFAULT_GAME_ROUNDS = 1000;

    static void parsePgn()
    {
        String          s = new String();
        Pattern         wp = Pattern.compile("^\\[White \"(.*)\"");
        Pattern         bp = Pattern.compile("^\\[Black \"(.*)\"");
        Pattern         rp = Pattern.compile("^\\[Result \"(.*)\"");
        Matcher         wm = wp.matcher("");
        Matcher         bm = bp.matcher("");
        Matcher         rm = rp.matcher("");
        String          w = "";
        String          b = "";
        //        String          r = "";

        try {
            FileReader fr = new FileReader( pgnfile );
            BufferedReader f = new BufferedReader(fr);

            while ( (s = f.readLine()) != null ) {
                wm.reset(s);
                if (wm.find()) {
                    w = wm.group(1);
                    continue;
                }
                bm.reset(s);
                if (bm.find()) {
                    b = bm.group(1);
                    continue;
                }
                rm.reset(s);
                if (rm.find()) {
                    // r = rm.group(1);
                    if (pl.containsKey(w) && pl.containsKey(b)) {
                        int idw = pl.get(w).id;
                        int idb = pl.get(b).id;
                        pl.get(w).count[idb] += 1;  // how many games against a given player
                        pl.get(b).count[idw] += 1;  // ditto
                        gn++;
                    }
                    continue;
                }
            }

            f.close();
        } catch (IOException e) {
            // assume this file does not exist and keep going
            System.out.println("PGN file " + pgnfile + " not found.  Keep going.\n");
        }
    }


    // ---------------------------------------------------------
    // Parse the pgn file, removing all games where one or both
    // players are no longer configured
    // ---------------------------------------------------------
    static void cleanPgn()
    {
        File            temp;
        String          s = new String();
        Pattern         ep = Pattern.compile("^\\[Event ");
        Pattern         wp = Pattern.compile("^\\[White \"(.*)\"");
        Pattern         bp = Pattern.compile("^\\[Black \"(.*)\"");
        Matcher         wm = wp.matcher("");
        Matcher         bm = bp.matcher("");
        Matcher         em = ep.matcher("");
        String          w = "";
        String          b = "";
        //	String          r = "";
        String          rec = new String();
        boolean         ok = false;
        BufferedWriter  fout;

        // FileReader cf = new FileReader( cfgFile );
        // BufferedReader f = new BufferedReader(cf);

        try {
            temp = File.createTempFile(pgnfile, ".tmp");
            temp.deleteOnExit();
            fout = new BufferedWriter(new FileWriter(temp));

            FileReader fr = new FileReader( pgnfile );
            BufferedReader f = new BufferedReader(fr);

            while ( (s = f.readLine()) != null ) {

                em.reset(s);
                if (em.find()) {
                    if (ok) {
                        fout.write(rec);
                    }
                    rec = "";
                    ok = true;   // until proved otherwise
                }

                rec = rec + s + "\n";

                wm.reset(s);
                if (wm.find()) {
                    w = wm.group(1);
                    if (!pl.containsKey(w))  {
                        ok = false;
                    }
                    continue;
                }
                bm.reset(s);
                if (bm.find()) {
                    b = bm.group(1);
                    if (!pl.containsKey(b))
                        ok = false;
                    continue;
                }
            }

            if (ok) {
                fout.write(rec);
            }

            fout.close();
            f.close();

            File orig = new File(pgnfile);
            ok = temp.renameTo(orig);

            if (ok) {
                System.out.printf("File \"%s\" was cleaned.\n", pgnfile );
            } else {
                System.out.printf("File \"%s\" was NOT cleaned: Failed to rename file \"%s\".\n", pgnfile, temp );
            }

        } catch (IOException e) {
            System.out.println(e);
            // assume this file does not exist and keep going
        }
    }


    // this is an ugly hack, return a pair of players in a single integer.
    static int getNextMatch()
    {
        int c = 999999999;
        int p0 = 0;
        int p1 = 0;

        // find pairing with least number of games played
        for (int a=0; a<totalPlayers-1; a++) {
            // System.out.printf(" ------------ player %d = %s %d\n",
            //                  a, pl.get(sname[a]).name, pl.get(sname[a]).id );
            Player pl_a = pl.get(sname[a]);
            for (int b=a+1; b<totalPlayers; b++) {
                // System.out.printf(" ------------ player %d = %s %d\n",
                //                  b, pl.get(sname[b]).name, pl.get(sname[b]).id );
                Player pl_b = pl.get(sname[b]);
                if (!pl_a.fam.equals(pl_b.fam)) {
                    if (pl_a.count[b] < c) {
                        c = pl_a.count[b];
                        if (pl_a.name.compareTo(pl_b.name) > 0)  {
                            p0 = pl.get(sname[a]).id;
                            p1 = pl.get(sname[b]).id;
                        } else {
                            p1 = pl.get(sname[a]).id;
                            p0 = pl.get(sname[b]).id;
                        }
                    }
                }
            }
        }

        if ((pl.get(sname[p0]).count[p1] & 1) == 1)
            return( (p1 << 16) | p0 );
        else
            return( (p0 << 16) | p1 );
    }


    public static int lcd(long x, long  y)
    {
        long max = x;
        if (y < x) max = y;
        for (int i = 2; i <= max; i++) {
            if ((x %i)==(y % i)) {
                return i;
            }
        }
        return 0;
    }


    // ------------------------------------------------------------
    // setSkip deterministically computes a "skip" factor which
    // enables up to choose openings that appear random but never
    // repeat.
    // ------------------------------------------------------------
    public static int setSkip( String w, String b )
    {
        long bc = Book.count;

        if (bc < 3) return 1;

        long sk = 0xffffffffL & (w + "|" + b).hashCode();

        while (true) {
            sk = sk % Book.count;
            if (sk == 0) sk++;

            long m = (long) bc % sk;
            long y = lcd(m, sk);

            if (m != 0) {
                if (y == 0) break;
            }
            sk++;
        }
        return (int) sk;
    }


    public static int setOfst(String w, String b)
    {
        long ofst = 0xffffffffL & (b + "<->" + w).hashCode();
        return (int) (ofst % Book.count);
    }


    public static void usage()
    {
        //     System.out.printf("\nUsage examples:\n\n");
        //     System.out.printf("java -jar lauto.jar test.txt\n");
        //     System.out.printf("java -jar lauto.jar test\n");
        //     System.out.printf("java -jar lauto.jar test clean\n");
        //     System.out.printf("\n");
        //     System.out.printf("Note: Actual configuration file must end with '.txt' extension but\n");
        //     System.out.printf("      that need not be specified.\n");
        //     System.out.printf("\n      Game results are stored in 'test.pgn' file\n");
        //     System.out.printf("\n");
        String progname = "lauto.jar";
        System.out.printf("Usage:\n");
        System.out.printf("java -jar %s perft <depth>\n", progname);
        System.out.printf("\tOutput the number of possible moves up to <depth>.\n", progname);
        System.out.printf("java -jar %s display\n", progname);
        System.out.printf("\tDisplay the starting board configuration.\n");
        System.out.printf("java -jar %s <test>[.txt] [clean]\n", progname);
        System.out.printf("\tRun autotester on configuration file <test>.txt\n");
        System.out.printf("\tResults are output to <test>.pgn\n");
        System.out.printf("\tclean: Clean <test>.pgn before running.\n");
    }

    public static void main(String[] args)
    {
        String          s = new String();
        String          cfgFile;
        String          title = new String();
        String          who = null;
        Integer         cpus = 1;   // by default

        File killfile = new File("killme.now");
        killfile.delete();

        title = "Autotest";     // default title

        System.out.printf("%s\n", version );

        if (args.length == 0) {
            usage();
            System.exit(1);
        }

        if (args[0].equals("-h") || args[0].equals("--help")) {
            usage();
            System.exit(0);
        }
        if (args[0].equals("perft")) {
            Leiserchess lch = new Leiserchess();
            Integer depth = Integer.parseInt(args[1]);
            for (int i=1; i<=depth; i++) {
                long x = lch.perft(i);
                System.out.printf("perft %d = %d\n", i, x );
            }
            System.exit(0);
        }
        if (args[0].equals("display")) {
            Leiserchess lch = new Leiserchess();
            System.out.printf("%s\n", lch.getBoard());
            System.exit(0);
        }


        // Configuration files always end with "txt" extension.
        // User can specify this with or without the extension,
        //   but in either case we need to extract the base name.
        // ------------------------------------------------------
        base = args[0];
        if (base.endsWith(".txt") == true) {
            base = base.substring(0, base.length() - 4);
        }

        cfgFile = base + ".txt";
        pgnfile = base + ".pgn";

        // read the configuration file
        // first open the file if possible
        // --------------------------------
        try {
            FileReader cf = new FileReader( cfgFile );
            BufferedReader f = new BufferedReader(cf);

            while ( (s = f.readLine()) != null ) {

                if (s.trim().length() < 3) continue;
                if (s.charAt(0) == '#') continue;

                String[] kv =  s.split("=");

                if (kv.length != 2) {
                    System.out.printf("Illegal line in configuration file:\n");
                    System.out.printf("%s\n", s);
                    System.exit(1);
                }

                if (kv.length == 2) {
                    String k = kv[0].trim();
                    String v = kv[1].trim();

                    if (k.equals("title")) {
                        title = v;
                    } else if (k.equals("player")) {
                        who = new String(v);  // should always point to a separate instance
                        if (pl.containsKey(who)) {
                            System.err.println("Configuration file error. Player \"" + who + "\" defined twice");
                            System.exit(1);
                        }
                        Player joe = new Player();
                        joe.name = new String(who);
                        // by default family is same as player
                        joe.fam = new String(who);
                        joe.depth = 0;
                        joe.nodedepth = 0;
                        joe.fisMain = 0L;
                        joe.fisInc =  0L;
                        joe.tcMvs[0] = 0L;
                        joe.tcMvs[1] = 0L;
                        joe.tcTme[0] = 0L;
                        joe.tcTme[1] = 0L;
                        joe.desc = "N/A";
                        joe.id = totalPlayers;
                        joe.invoke = "";
                        pl.put(who, joe);
                        // find the name of a player by his index in list
                        sname[totalPlayers] = who;
                        totalPlayers += 1;
                    } else if (k.equals("cpus")) {
                        cpus = Integer.parseInt(v);
                    } else if (k.equals("adjudicate")) {
                        adjudicate = Integer.parseInt(v);
                        // need some kind of limit on this
                        if (adjudicate > 4000) adjudicate = 4000;
                        if (adjudicate < 2) adjudicate = 2;
                    } else if (k.equals("book")) {
                        opening_book = new String(v);
                    } else if (k.equals("game_rounds")) {
                        game_rounds = Integer.parseInt(v);  // pairs of games
                    } else if (k.equals("desc")) {
                        // PlayGame.desc = v;
                    } else if (k.equals("family")) {
                        Player cur = pl.get(who);
                        cur.fam = new String(v);
                    } else if (k.equals("invoke")) {
                        Player cur = pl.get(who);
                        Pattach  z;

                        System.out.printf("Testing player: %s\n", cur.name );
                        try {
                            z = new Pattach();
                            cur.invoke = v;
                            z.setInvoke( v );
                            z.start();
                            z.snd("uci\n");
                            cur.opts = z.getOpts();
                            z.snd("quit\n");
                            z.cleanup();
                        } catch (Exception e) {
                            System.out.printf("\nError running program %s\n", cur.name);
                            System.out.printf("Please check syntax in configuration file.\n\n");
                            System.exit(1);
                        }

                        // pl.put(who, new Player(cur));
                    } else if (k.equals("depth")) {
                        Player cur = pl.get(who);
                        cur.depth = Integer.parseInt(v);
                    } else if (k.equals("nodes")) {
                        Player cur = pl.get(who);
                        String ts2 = v.replaceAll("[.,]+", "");
                        cur.nodedepth = Integer.parseInt(ts2);
                    } else if (k.equals("fis")) {
                        String[] mi = v.split("\\s+");
                        Player cur = pl.get(who);
                        cur.fisMain = (long) (1000000000.0 * Double.parseDouble(mi[0]));
                        cur.fisInc = (long) (1000000000.0 * Double.parseDouble(mi[1]));
                        // pl.put(who, new Player(cur));
                    } else if (k.equals("tc")) {
                        String[] mi = v.split("\\s+");
                        Player cur = pl.get(who);
                        cur.tcMvs[0] = (long) Long.parseLong(mi[0]);
                        cur.tcTme[0] = (long) (1000000000.0 * Double.parseDouble(mi[1]));

                        if (mi.length > 3) {
                            cur.tcMvs[1] = (long) Long.parseLong(mi[2]);
                            cur.tcTme[1] = (long) (1000000000.0 * Double.parseDouble(mi[3]) );
                        } else {
                            cur.tcTme[1] = cur.tcTme[0];
                            cur.tcMvs[1] = cur.tcMvs[0];
                        }

                        // pl.put(who, new Player(cur));
                    } else {
                        // anything that goes this far becomes a setoption
                        System.out.printf("cur.name = %s\n",  who );
                        System.out.printf( "option: %s\n", k + " - " + v );

                        String ts = "setoption name " + k + " value " + v;
                        Player cur = pl.get(who);

                        if (cur.checkOption(ts) == false) {
                            System.out.printf("\nIllegal option found in configuration file\n");
                            System.out.printf("Please check configuration file syntax.\n");
                            System.out.printf("option: %s\n\n", k );
                            System.exit(1);
                        }


                        cur.options.add(ts);
                        // pl.put(who, new Player(cur));
                        // System.out.printf("opts: %s\n", ts);
                        //			pl.get(who).options.add(ts);
                        //			pl.put(who, pl.get(who) );
                    }
                }
            }

        } catch (IOException e) {
            System.out.println(e);
        }

        if (args.length > 1) {
            if (args[1].equals("clean")) {
                cleanPgn();
                System.exit(0);
            } else {
                System.out.println("\nDo not understand \"" + args[1] + "\"");
                System.out.println("Perhaps you mean \"clean\"\n");
                System.exit(0);
            }
        }

        parsePgn();


        // Read the opening book
        if ( opening_book == "" ) {
            System.out.printf("No opening book specified, using starting position.\n");
            Book.opening[Book.count] = null;
            Book.count = Book.count + 1;
        } else {
            try {
                System.out.printf("Use opening book specified: " + opening_book + "\n");
                File file = new File(opening_book);
                InputStreamReader reader = new InputStreamReader( new FileInputStream(file) );
                BufferedReader f = new BufferedReader(reader);
                while ( (s = f.readLine()) != null ) {
                    Book.opening[Book.count] = s;
                    Book.count = Book.count + 1;
                }
            } catch (IOException e) {
                System.out.println( "Error:" + e.getMessage() );
                System.exit(0);
            }
        }

        try {
            FileWriter fw = new FileWriter( pgnfile, true );
            pgnwrite = new BufferedWriter(fw);
        } catch (Exception e) {
            System.err.println( "Error: " + e.getMessage() );
            System.exit(0);
        }

        if (game_rounds == 0) game_rounds = DEFAULT_GAME_ROUNDS;


        // precompute the skip and offset factors
        // --------------------------------------
        for (int wid = 0; wid < totalPlayers; wid++) {
            for (int bid = 0; bid < totalPlayers; bid++) {
                if (wid != bid) {
                    if (sname[wid].compareTo(sname[bid]) > 0) {
                        Player whp = pl.get( sname[wid] );
                        whp.skip[bid] = setSkip( sname[bid], sname[wid] );
                        whp.ofst[bid] = setOfst( sname[bid], sname[wid] );
                    } else {
                        Player whp = pl.get( sname[wid] );
                        whp.skip[bid] = setSkip( sname[wid], sname[bid] );
                        whp.ofst[bid] = setOfst( sname[wid], sname[bid] );
                    }
                }
            }
        }

        start_time = System.nanoTime();

        total_games = -cpus;
        System.out.println("total_games = "+total_games);

        while (true) {

            boolean  finished = false;
            int pv = 0;

            if (killfile.exists() == true) {
                while (Counter.howMany() > 0) {
                    try {
                        Thread.sleep(10);
                    } catch(InterruptedException e) {
                        System.out.println(e.getMessage());
                    }
                }
                System.exit(0);
            }

            if (Counter.howMany() < cpus) {
                finished = false;
                pv = getNextMatch();

                Player w = pl.get( sname[pv >> 16] );
                Player b = pl.get( sname[pv & 0xffff] );
                long opn_ix = (w.count[b.id] / 2) % Book.count;  // which opening to play
                if (opn_ix < Book.count && total_games+cpus < game_rounds) {
                    System.out.println("opn_ix = "+opn_ix+", generate game round: "+(total_games+cpus));

                    total_games++;
                    opn_ix = (w.ofst[b.id] + opn_ix * w.skip[b.id]) % Book.count;

                    // track how many total white games, and how many against given player
                    w.count[b.id]++;
                    b.count[w.id]++;
                    //System.out.println("w.count = "+w.count[b.id]+", b.count = "+b.count[w.id]);
                   Counter.increment();
                   PlayGame  xx = new PlayGame();

                    xx.event = title;
                    //System.out.println( "opening ix = " + opn_ix );
                    xx.setOpening( Book.opening[ (int) opn_ix ] );
                    xx.setGameNumber(gn);
                    xx.setWhitePlayer(w);
                    xx.setBlackPlayer(b);
                    xx.drawMoves = adjudicate;
                    gn = gn + 1;
                    xx.begin();
                } else {
                    finished = true;
                    // System.out.printf("We think we are finished here strangely enough\n");
                }
            }


            try {
                Thread.sleep(10);
            } catch(InterruptedException e) {
                System.out.println(e);
            }

            cycle++;

            if (finished && Counter.howMany() == 0) {

                // are there any that have not been written?
                PlayGame.writePgn();

                try {
                    pgnwrite.close();
                } catch (Exception e) {
                    System.err.println( "Error: " + e.getMessage() );
                }

                long tg = total_games;
                if (tg < 0) tg = 0;
                long et = System.nanoTime() - start_time;
                double sec = (double)et / 1000000000.0;
                double gpm = 60.0 * tg / sec;

                int   igpm = (int)gpm;
                int   frac3 = (int) ((gpm - igpm) * 1000);
                System.out.printf("%10d.%1d sec  %10d.%03d gpm  %8d games\n",
                                  (int)sec, (int)(10 * (sec - (int)sec)),  igpm, frac3, gn );

                // PlayGame.oldest();
                System.out.printf("Finished ...\n");
                return;
            }


            if ( (cycle & 0x7ff) == 100 ) {
                long tg = total_games;
                if (tg < 0) tg = 0;
                long et = System.nanoTime() - start_time;
                double sec = et / 1000000000.0;
                double gpm = 60.0 * tg / sec;

                // work around gcj printf bug for floating point
                int   igpm = (int)gpm;
                int   frac3 = (int) ((gpm - igpm) * 1000);
                System.out.printf("%10d.%1d sec  %10d.%03d gpm  %8d games\n",
                                  (int)sec, (int)(10 * (sec - (int)sec)),  igpm, frac3, gn);
                // PlayGame.oldest();
            }

        }
    }
}


