CC = gcc

NAME = pather

SRCS = main.c

LIB = dungen
OBJ = $(LIB)/Map.o

# CompSpec defined in windows environment.
ifdef ComSpec
	BIN = $(NAME).exe
else
	BIN = $(NAME)
endif

CFLAGS =
CFLAGS += -std=c99
CFLAGS += -Wshadow -Wall -Wpedantic -Wextra -Wdouble-promotion
CFLAGS += -g
CFLAGS += -Ofast -march=native

LDFLAGS =
LDFLAGS += -lm

ifdef ComSpec
	RM = del /F /Q
	MV = ren
else
	RM = rm -f
	MV = mv -f
endif

# Link.
$(BIN): $(SRCS:.c=.o)
	# Compile dep libs
	make -C $(LIB)
	# Link final exe with dep lib
	$(CC) $(CFLAGS) $(SRCS:.c=.o) $(OBJ) $(LDFLAGS) -o $(BIN)

# Compile.
%.o : %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -MT $@ -MF $*.td -c $<
	@$(RM) $*.d
	@$(MV) $*.td $*.d
%.d: ;
-include *.d

clean:
	# Clean local
	$(RM) vgcore.*
	$(RM) cachegrind.out.*
	$(RM) callgrind.out.*
	$(RM) $(BIN)
	$(RM) $(SRCS:.c=.o)
	$(RM) $(SRCS:.c=.d)
	# Clean libs
	make clean -C $(LIB)
