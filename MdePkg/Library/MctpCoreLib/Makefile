CFLAGS  = -Wall -g -I../../Include/ -I../../Include/X64/ -fshort-wchar
LDFLAGS = -g
LDLIBS_CMOCKA = -lcmocka
LDFLAGS_CMOCKA = -Wl,--wrap=write -Wl,--wrap=read

SRC=$(wildcard *.c)
SRC_EXT=
SRC_EXT+=../../Library/BaseMemoryLib/CopyMem.c
SRC_EXT+=../../Library/BaseMemoryLib/CopyMemWrapper.c
SRC_EXT+=../../Library/BaseMemoryLib/ZeroMemWrapper.c
SRC_EXT+=../../Library/BaseMemoryLib/SetMem.c
SRC_EXT+=../../Library/BaseMemoryLib/MemLibGeneric.c
SRC_EXT+=../../Library/BaseDebugLibNull/DebugLib.c
SRC_EXT+=../../Library/BaseLib/LShiftU64.c
SRC_EXT+=../../Library/BaseLib/Math64.c
SRC_EXT+=../../Library/BaseLib/SwapBytes32.c
SRC_EXT+=../..//Library/BaseLib/SwapBytes16.c

SRC_OBJ=$(patsubst %.c,obj/%.o,$(SRC))
SRC_OBJ+=$(patsubst %.c,obj/%.o,$(notdir $(SRC_EXT)))
OBJ_DIR=obj

all: test
$(SRC_OBJ): | $(OBJ_DIR)

$(OBJ_DIR):
	mkdir -p $@

$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: ../../Library/BaseMemoryLib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: ../../Library/BaseDebugLibNull/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: ../../Library/BaseLib/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test : $(SRC_OBJ)
	echo $(SRC_OBJ)
	$(LINK.o) $(LDFLAGS_CMOCKA) $^ $(LOADLIBES) $(LDLIBS_CMOCKA) -o $@

.PHONY: test_mocka
test_mocka : test
	./test

.PHONY: clean
clean:
	rm -f test $(OBJ_DIR)/*.o
