# Target library
lib := libuthread.a

all: $(lib)

MAKEFLAGS	+= -rR

ifneq ($(V),1)
Q = @
V = 0
endif

# Current directory
CUR_PWD := $(shell pwd)

# Compiler, targets for compilation
CC	:= gcc
LIB	:= ar rcs
LIBN	:= libuthread.a
LIBCL	:= sem.o tps.o
LIBTARG	:= thread.o queue.o sem.o tps.o

# Flags given by project description
CFLAGS	:= -Wall -Werror
CFLAGS	+= -pthread

# Debug
ifneq ($(D),1)
CFLAGS	+= -O2
else
CFLAGS	+= -O0
CFLAGS	+= -g
endif

# Compile the library
$(LIBN): $(LIBTARG)
	@echo "AR	$@"
	$(Q)$(LIB) $(LIBN) $(LIBTARG)

# Only compile tps.c and sem.c, queue.o and thread.o are given
sem.o: sem.c
	@echo "CC	$@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

tps.o: tps.c
	@echo "CC	$@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

# Clean directories, return to original state
clean:
	@echo "CLEAN	$(CUR_PWD)"
	$(Q)rm -rf $(LIBN) $(LIBCL)

