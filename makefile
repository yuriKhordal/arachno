# ========== Definitions ==========
BIN = arachno #Binary Executable
HDR = $(wildcard include/*.h) #Header files
SRC = $(wildcard src/*.c) #Code files
#OBJ = $(subst src/,obj/,$(SRC:.c=.o)) #Object files
OBJ = $(SRC:src/%.c=obj/%.o) #Object files
#DEP = $(subst src/,deps/,$(SRC:.c=.d)) #Dependency files
DEP = $(SRC:src/%.c=deps/%.d) #Object files

CC = gcc
FLAGS = -DDEBUG -Wall -pedantic -rdynamic -g -lmagic -Iinclude

# ========== Rules ==========
.PHONY: compile clean run test

compile: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(FLAGS)

obj/%.o:
	$(CC) -c $< -o $@ $(FLAGS)

deps/%.d: src/%.c
	$(CC) -Iinclude -MF $@ -MG -MM -MP -MT $(<:src/%.c=obj/%.o) $<

-include $(DEP)

output%.txt: input%.txt $(BIN)
	$(BIN) > $@ 2>&1 < $<

clean:
	-rm -f $(BIN)
	-rm -f $(OBJ)
	-rm -f $(DEP)

run: compile
	$(BIN)

test:
	make -n --trace