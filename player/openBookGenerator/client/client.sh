#!/bin/bash
server="http://haoran.xvm.mit.edu/6172"
for ((;1<2;))
do
	wget -q -O input${1}.txt "${server}/connect.php?password=6172finaLpassw0rd"
	gid=$(head -n 1 input${1}.txt)
	if [ $gid -eq 0 ]; then
		exit
	fi
	sed -i -e "1d" input${1}.txt
	echo "calculating bestmove for gid ${gid}.."
	./leiserchess input${1}.txt > output${1}.txt
	grep -oP "(?<=bestmove )[^ ]+" output${1}.txt > bestmove${1}.txt
	bestmove=$(head -n 1 bestmove${1}.txt)
	wget -q -O response${1}.txt "${server}/connect.php?password=6172finaLpassw0rd&report=1&gid=${gid}&result=${bestmove}"
done