CC = gcc
TARGET := pgnstats
SRC := pgnstats.c
OBJ := $(addsuffix .o, $(basename $(SRC)))


%.o : %.c
	$(CC) -c -Wall -g -O3 $< -o $@

$(TARGET) : $(OBJ)
	$(CC) $(OBJ) -lm -o $@

clean :
	rm -f *.o *~ $(TARGET)
