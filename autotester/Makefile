OPTS=-encoding US-ASCII -O

lauto.jar : Game.class Leiserchess.class Harness.class Pattach.class PlayGame.class Book.class Counter.class Player.class main.txt
	jar cmf main.txt lauto.jar *.class
	cp -f lauto.jar ../tests/	 

%.class : %.java
	javac ${OPTS} $<

clean : 
	rm -rf *.class *~ *.o lauto.jar
