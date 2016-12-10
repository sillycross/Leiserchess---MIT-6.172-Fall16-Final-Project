CC = clang
TARGET := leiserchess
SRC := util.c tt.c fen.c move_gen.c search.c eval.c closebook.c openbook.c
OBJ := $(SRC:.c=.o)
UNAME := $(shell uname)

ifeq ($(PARALLEL),1)
	OS_TYPE := Parallel Linux
	PFLAG := -DPARALLEL -D_BSD_SOURCE -D_XOPEN_SOURCE=700
	# -g needed for test framework assertions
	#
	# switch -fdetach to -ftapir to enable Tapir optimization passes for better
	# performance.
	CFLAGS := -std=gnu99 -Wall -g -ftapir
	LDFLAGS= -Wall -lm -lrt -ldl -lpthread -lcilkrts
else
ifeq ($(UNAME),Darwin)
	OS_TYPE := Mac
	PFLAG := -DMACPORT -D__MACH__ -D_XOPEN_SOURCE
	CFLAGS := -std=gnu99 -lrt -Wall -DNDEBUG -g
	LDFLAGS= -lrt -Wall
else
	OS_TYPE := Linux
	#PFLAG := -D_XOPEN_SOURCE
	CFLAGS := -std=gnu99 -Wall -g
	LDFLAGS= -Wall -lm -lrt -ldl -lpthread
endif
endif

ifeq ($(DEBUG),1)
	CFLAGS += -O0 -DDEBUG $(PFLAG)
else
	CFLAGS += -O3 -ftapir -DNDEBUG $(PFLAG)
endif

ifeq ($(REFERENCE),1)
	CFLAGS += -DRUN_REFERENCE_CODE=1
endif

CFLAGS += $(OTHER_CFLAGS)

LDFLAGS= -Wall -lm -lrt -ldl -lpthread -lcilkrts

.PHONY : default clean


default : $(TARGET)

# Each C source file will have a corresponding file of prerequisites.
# Include the prerequisites for each of our C source files.
-include $(SRC:.c=.d)

# This rule generates a file of dependencies (i.e., a makefile) %.d
# from the C source file %.c.
%.d : %.c
	@set -e; rm -f $@; \
	$(CC) -MM -MT $*.o -MP -MF $@.$$$$ $(CFLAGS) $<; \
	sed -e 's|\($*\)\.o[ :]*|\1.o $@ : |g' < $@.$$$$ > $@; \
	rm -f $@.$$$$*

# We could use the -MMD and -MP flags here to have this rule generate
# the dependencies file.
%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

search.c: search_scout.c


leiserchess : leiserchess.o $(OBJ)
	$(CC) $^ $(LDFLAGS) -o $@ -lrt

clean :
	rm -f *.o *.d* *~ $(TARGET)
