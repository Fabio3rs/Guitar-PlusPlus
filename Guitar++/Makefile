#########################
# customise these
CPPFILES := *.c
PROG := prog
CFLAGS := -Wall -Wextra -g
LDFLAGS :=
########################

# -MMD generates dependencies while compiling
CFLAGS += -MMD
CC := g++

OBJFILES := $(CPPFILES:.cpp=.o)
OBJFILES += $(CFILES:Lua/.c=.o)
DEPFILES := $(CPPFILES:.cpp=.d)
DEPFILES := $(CFILES:Lua/.c=.d)

$(PROG): $(OBJFILES)
    $(LINK.o) $(LDFLAGS) -o $@ $^

clean:
    m -f $(PROG) $(OBJFILES) $(DEPFILES)

-include $(DEPFILES)

