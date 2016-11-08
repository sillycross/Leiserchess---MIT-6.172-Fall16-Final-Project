#!/afs/csail.mit.edu/proj/courses/6.172/bin/python

'''
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
'''

import sys
import string, cgi, time
import os, subprocess
import json
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer

PLAYER = "../player/leiserchess"
player_in = None
player_out = None

info = ''

next_req_id = 0
pending_requests = dict()

def play(position, moves, time, inc):
    if len(moves) > 0:
      args = \
          'position ' + position + ' ' + \
          'moves ' + moves + '\n' + \
          'go ' + \
          'time ' + time + ' ' + \
          'inc ' + inc + '\n'
          # 'btime ' + btime + ' ' + \
          # 'wtime ' + wtime + ' ' + \
          # 'binc ' + binc + ' ' + \
          # 'winc ' + winc + '\n'
    else:
      args = \
         'position ' + position + '\n' + \
         'moves ' + moves + '\n' + \
         'go ' + \
         'time ' + time + ' ' + \
         'inc ' + inc + '\n'
         # 'btime ' + btime + ' ' + \
         # 'wtime ' + wtime + ' ' + \
         # 'binc ' + binc + ' ' + \
         # 'winc ' + winc + '\n'
 
    print args
    player_in.write(args)

    line = player_out.readline().strip()
    while True:
        if not line:
            print "ERROR: Leiserchess Player Terminated"
            break
        print line

        global info
        info += line + '\n'

        if line.startswith("bestmove "):
            return line.split(" ")[1]
        line = player_out.readline().strip()

# from http://stackoverflow.com/questions/3812849/how-to-check-whether-a-directory-is-a-sub-directory-of-another-directory
def in_directory(file, directory):
    #make both absolute
    directory = os.path.realpath(directory)
    file = os.path.realpath(file)

    #return true, if the common prefix of both is equal to directory
    #e.g. /a/b/c/d.rst and directory is /a/b, the common prefix is /a/b
    return os.path.commonprefix([file, directory]) == directory

class LeiserchessHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        start_time = time.time()

        if self.path == "/":
            self.path = "/index.html"

        contentTypes = {
            ".html" : "text/html",
            ".js" : "application/javascript",
            ".css" : "text/css",
            ".png" : "image/png",
            ".pdf" : "application/pdf",
        }

        try:
            for suffix, contentType in contentTypes.items():
                if self.path.endswith(suffix):
                    path = os.getcwd() + self.path
                    if not in_directory(path, os.getcwd()):
                        raise IOError

                    f = open(path)
                    self.send_response(200)
                    self.send_header('Content-type', contentType)
                    self.end_headers()
                    self.wfile.write(f.read())
                    f.close()

                    print time.time() - start_time
                    return

            self.send_error(404, 'File Not Found: %s' % self.path)

        except IOError:
            self.send_error(404, 'File Not Found: %s' % self.path)

    def do_POST(self):
        start_time = time.time()
        global next_req_id

        ctype, pdict = cgi.parse_header(self.headers.getheader('content-type'))
        if ctype == 'multipart/form-data':
            postvars = cgi.parse_multipart(self.rfile, pdict)
        elif ctype == 'application/x-www-form-urlencoded':
            length = int(self.headers.getheader('content-length'))
            postvars = cgi.parse_qs(self.rfile.read(length), keep_blank_values=1)
        else:
            postvars = {}

        if self.path.startswith("/move/"):
            print postvars

            position = postvars['position'][0]
            moves = postvars['moves'][0]
            gotime = postvars['gotime'][0]
            goinc = postvars['goinc'][0]

            returnMove = play(position, moves, gotime, goinc);

            pending_requests[next_req_id] = returnMove

            self.send_response(200)
            self.send_header('Content-type', "text/json")
            self.end_headers()
            self.wfile.write('{"move":"' + returnMove + '", "reqid": "'+str(next_req_id)+'"}')
            next_req_id = next_req_id + 1

            print time.time() - start_time
            return

        if self.path.startswith("/poll/"):
            print "Polling for id " + str(postvars['reqid'][0])
            returnMove = pending_requests[int(postvars['reqid'][0])]
            print "return move is " + str(returnMove)
            del pending_requests[int(postvars['reqid'][0])]
            self.send_response(200)
            self.send_header('Content-type', "text/json")
            self.end_headers()

            global info
            output = {
              "move": returnMove,
              "info": info
            }
            self.wfile.write(json.dumps(output))
            return

        self.send_error(404, 'File Not Found: %s' % self.path)

def main(argv = None):
    port = 5555

    if argv is None:
        argv = sys.argv
    if len(argv) > 1:
        port = int(argv[1])

    # Set up player
    global player_in
    global player_out
    proc = subprocess.Popen(PLAYER, stdin=subprocess.PIPE, stdout=subprocess.PIPE, shell=True)
    player_in = proc.stdin
    player_out = proc.stdout

    player_in.write('uci\n')
    while player_out.readline().strip() != "uciok":
        pass

    try:
        server = HTTPServer(('', port), LeiserchessHandler)
        print 'Start Leiserchess at port ' + str(port) + '...'
        server.serve_forever()
    except KeyboardInterrupt:
        print '^C received, shutting down server'
        server.socket.close()

if __name__ == '__main__':
    sys.exit(main())

