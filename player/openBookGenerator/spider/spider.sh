#!/bin/bash
for ((i=$1;i<$2;i++))
do
	wget -q -O $i.1.tmp "http://scrimmage.csail.mit.edu/log?gameid=${i}&player=1&short=1&replay_game_iter=200"
	./process $i.1.tmp $i.1.txt
	rm $i.1.tmp
	wget -q -O $i.2.tmp "http://scrimmage.csail.mit.edu/log?gameid=${i}&player=2&short=1&replay_game_iter=200"
	./process $i.2.tmp $i.2.txt
	rm $i.2.tmp
	echo `cat $i.1.txt | wc -l` >> $3
	cat $i.1.txt >> $3
	echo `cat $i.2.txt | wc -l` >> $3
	cat $i.2.txt >> $3
	rm $i.1.txt
	rm $i.2.txt
done
