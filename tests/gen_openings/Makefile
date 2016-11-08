VPATH = ../../player

include ../../player/Makefile

.PHONY : clean_gen

default : gen_openings

gen_openings.o : CFLAGS += -DGEN_OPENINGS
gen_openings : gen_openings.o $(OBJ)
	$(CXX) $^ $(LDFLAGS) -o $@

clean : clean_gen

clean_gen :
	rm -f *.o gen_openings
