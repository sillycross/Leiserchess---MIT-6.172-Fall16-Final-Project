/**
 * Copyright (c) 2012 MIT License by 6.172 Staff
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


import  java.util.*;
import  java.util.regex.Pattern;
import  java.util.regex.Matcher;

class Player {
    Integer  id;         // each player is given an integer id for internal bookeeping
    String   name;
    String   fam;        // family designation
    String   invoke;
    String   desc;
    String[] opts;

    int     depth = 0;
    int     nodedepth = 0;   // if not zero, then use nodedepth
    long    fisMain;         // In nanoseconds
    long    fisInc;          // In nanoseconds
    long[]  tcTme = new long[2];        // In nanoseconds
    long[]  tcMvs = new long[2];        // In moves


    ArrayList<String> options = new ArrayList<String>();
    int[] count = new int[512];    // how many games against a given opponent
    int[]  skip = new int[512];    // precomputed skip factors (for each opponent) to save time
    int[]  ofst = new int[512];    // precomputed ofset factors (for each opponent) to save time

    private boolean isInteger(String in) 
    {
        try {

            Integer.parseInt(in);

        } catch (NumberFormatException ex) {
            return false;
        }

        return true;
    }


    public Boolean checkOption( String s )
    {
        int  i;
        String    userN;
        String    userV;
        Pattern   onp = Pattern.compile("^option name (.*) type (\\S+)");
        Matcher   onm = onp.matcher("");
        Pattern   usp = Pattern.compile("^setoption name (.*) value (.*)");
        Matcher   usm = usp.matcher("");

        usm.reset(s);
        if (usm.find()) {
            userN = usm.group(1).trim();
            userV = usm.group(2).trim();
        } else {
            return false;
        }

        for (i=0; i<512; i++) {
            if (opts[i] == null) break;

            onm.reset(opts[i]);
            if (onm.find()) {
                String n = onm.group(1).trim();
                String t = onm.group(2).trim();

                if ( n.equals(userN) ) {

                    if (t.equals("spin")) {
                        if (isInteger(userV) == false) {
                            System.out.printf("Value must be integer for %s\n", n );			    
                            return false;
                        }
                    }
                    if (t.equals("check")) {
                        if (userV.equals("true") || userV.equals("false")) {
                            return true;
                        } else {
                            System.out.printf("Value must be true or false for %s\n", n );
                            return false;
                        }
                    }
                    return true;
                }
            }
        }

        return false;
    }


    public Player()
    {

    }


}

