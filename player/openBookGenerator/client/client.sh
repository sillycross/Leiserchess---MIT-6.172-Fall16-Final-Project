#!/bin/bash
server="http://haoran.xvm.mit.edu/6172"
for ((;1<2;))
do
	wget -q -O input.txt "${server}/connect.php?password=6172finaLpassw0rd"
	gid=$(head -n 1 input.txt)
	if [ $gid -eq 0 ]; then
		exit
	fi
	sed -i -e "1d" input.txt
	echo "calculating bestmove for gid ${gid}.."
	./leiserchess input.txt > output.txt
	grep -oP "(?<=bestmove )[^ ]+" output.txt > bestmove.txt
	bestmove=$(head -n 1 bestmove.txt)
	wget -q -O response.txt "${server}/connect.php?password=6172finaLpassw0rd&report=1&gid=${gid}&result=${bestmove}"
done