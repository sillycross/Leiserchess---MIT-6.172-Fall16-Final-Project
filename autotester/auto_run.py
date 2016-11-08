import os
import sys
total = len(sys.argv)
cmdargs = sys.argv
os.system("make");
#os.system("rm ../tests/basic.pgn")
if total == 2:
    pgnname = ""
    if cmdargs[1].endswith(".txt"):
        name = cmdargs[1][0:-4]
        pgnname = name + ".pgn"
        print pgnname
    else:
        pgnname = cmdargs[1] + ".pgn"
        print pgnname
    os.system("rm "+pgnname);
    os.system("java -jar lauto.jar " + cmdargs[1])
    os.system("../tests/pgnrate.tcl " + pgnname)
else:
    os.system("java -jar lauto.jar ../tests/basic.txt")
    print "finishes running java"
    os.system("../tests/pgnrate.tcl ../tests/basic.pgn")
