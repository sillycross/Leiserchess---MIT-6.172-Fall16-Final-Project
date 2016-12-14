#!/bin/bash
rm all.c
for codefile in fasttime tbassert simple_mutex leiserchess search eval move_gen tt fen util
do
	if [ -f ${codefile}.c ]; then
		echo '//' >> all.c 
		echo '//' >> all.c 
		cat ${codefile}.c >> all.c
	fi
done
