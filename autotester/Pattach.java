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


import  java.io.*;
import  java.util.regex.Pattern;
import  java.util.regex.Matcher;

class Pattach {

    private OutputStream stdin = null;
    private InputStream stdout = null;
    private InputStream stderr = null;
    private BufferedReader br;

    private String invoke;
    private Process p;

    private Integer  depth_achieved = 0;
    private Pattern  depp = Pattern.compile(" depth (\\d+)");
    private Matcher  depm = depp.matcher("");

    private Long     nodes_achieved = 0L;
    private Pattern  nodesp = Pattern.compile(" nodes (\\d+)");
    private Matcher  nodesm = nodesp.matcher("");
    private boolean hasdied = false;

    public void snd(String s)
    {
        if (hasdied) return;

        try {
            stdin.write( s.getBytes() );
            stdin.flush();
        } catch (IOException e) {
            System.out.printf("Program crash\n");
            // System.out.println("Sending " + s);
            // System.out.println(e);
        }
    }

    public void cleanup()
    {
        if (hasdied) return;

        try {
            p.waitFor();
        } catch(InterruptedException e) {
            System.out.println("hey, something wrong\n");	    
            System.out.println(e);	    
        }

        try {
            stdin.close();
            stdout.close();
            stderr.close();
        } catch(java.io.IOException e) {
            System.out.println("hey, something wrong\n");	    
            System.out.println(e);	    
        }
    }

    public String rcv()
    {
        String  s = null;

        try {
            s = br.readLine(); 
        } catch( IOException e) {
            System.out.println(e);	    
        }
        if (s == null) {
            hasdied = true;
            return "hasdied";
        } else {
            return s;
        }
    }

    public Integer depthAchieved()
    {
        return  depth_achieved;
    }

    public Long nodesAchieved()
    {
        return nodes_achieved;
    }

    // static String[] sname = new String[512]; // map id to player name
    public String[] getOpts()
    {
        String    s = null;
        String    w = "uciok";
        int       x = w.length();
        int       c = 0;
        String[]  opts = new String[512];  


        while (true) {
            try {
                s = br.readLine();
            } catch( IOException e) {
                System.out.println(e);
            }

            if (s.length() >= w.length()) {
                if (s.substring(0, x).equals(w)) {
                    return opts;
                }
            }

            // System.out.printf("options: %s\n", s);
            opts[c] = new String(s);
            c = c + 1;
        }
    }


    public String waitfor(String w)
    {
        String  s = null;
        int     x = w.length();

        if (hasdied) return "hasdied";

        while (true) {
            try {
                s = br.readLine();
            } catch( IOException e) {
                System.out.println(e);
            }

						// C F: uncomment this to see what leiserchess prints out on each move
						if (s != null && s.contains("Score MISMATCH")) {
							System.out.println(s);
							System.out.println("Terminating run!");
							System.exit(0);
						}

            if (s == null) {
                hasdied = true;
                System.out.printf("Program %s has died!\n", invoke);
                return "hasdied";
            }

            depm.reset(s);
            if (depm.find()) {
                depth_achieved = Integer.parseInt(depm.group(1));
            }

            nodesm.reset(s);
            if (nodesm.find()) {
                nodes_achieved = Long.parseLong(nodesm.group(1));
            }

            if (s.length() >= w.length()) {
                if (s.substring(0, x).equals(w)) {
                    return s;
                }
            }
        }
    }


    public void  start()
    {
        try {
            p = Runtime.getRuntime().exec( invoke );
        } catch (IOException e) {
            System.out.println(e);
        }

        stdin = p.getOutputStream ();
        stderr = p.getErrorStream ();
        stdout = p.getInputStream ();

        br = new BufferedReader (new InputStreamReader (stdout));
    }


    public Pattach()
    {

    }    

    public void setInvoke( String s )
    {
        invoke = s;
    }


}
