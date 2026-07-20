CC := gcc

CFLAGS := -g -Wall -DNETWORK_SUPPORT -DVMRP
CAPSTONE :=
UNICORN = ./windows/unicorn-1.0.2-win32/unicorn.lib
SDL2 = ./windows/SDL2-2.0.10/i686-w64-mingw32

CORE_OBJS := network.o fileLib.o vmrp.o utils.o rbtree.o bridge.o memory.o

ifeq ($(DEBUG),1)
	CFLAGS += -DDEBUG
	CORE_OBJS := debug.o $(CORE_OBJS)
	CAPSTONE := ./windows/capstone-4.0.1-win32/capstone.dll
endif

LAUNCHER_LIB ?= ../../build-i686/liblauncher_core.a
LAUNCHER_INC ?= ../../include

# Default make target keeps historical name but builds plain baseline.
.PHONY: main plain gwy clean

main: plain

# Upstream baseline: strong unbound stubs (plain only; never linked into gwy).
plain: $(CORE_OBJS) gwy_vm_file_weak.o gwy_ext_obs_weak.o main.o
	$(CC) $(CFLAGS) -m32 -o ./bin/main_plain.exe $^ $(UNICORN) $(CAPSTONE) -lpthread -lm \
		-lws2_32 -lgdi32 -lmingw32 -L$(SDL2)/lib/ -lSDL2main -lSDL2
	@echo "[OK] built bin/main_plain.exe (stub unbound)"

# Dual targets: `plain` links strong stubs in gwy_vm_file_weak.c (no ELF weak; MinGW PE);
# `gwy` links liblauncher_core.a and must not link the stub object.
gwy: CFLAGS += -DGWY_USE_VM_FILE_SERVICE -I$(LAUNCHER_INC)
gwy: $(CORE_OBJS) main.o
	@if [ ! -f "$(LAUNCHER_LIB)" ]; then echo "missing $(LAUNCHER_LIB); build gwy_launcher first"; exit 1; fi
	$(CC) $(CFLAGS) -m32 -o ./bin/main_gwy.exe $(CORE_OBJS) main.o \
		$(LAUNCHER_LIB) $(UNICORN) $(CAPSTONE) -lz -lpthread -lm -lws2_32 -lgdi32 -lmingw32 -L$(SDL2)/lib/ -lSDL2main -lSDL2
	@echo "[OK] built bin/main_gwy.exe (VmFileService bound)"

dll: $(CORE_OBJS) gwy_vm_file_weak.o gwy_ext_obs_weak.o
	$(CC) $(CFLAGS) -m32 -shared -o ./bin/vmrp.dll $^ $(UNICORN) $(CAPSTONE) -lpthread -lm -lws2_32 -lgdi32 -lmingw32

%.o: %.c
	$(CC) $(CFLAGS) -m32 -c $< -o $@

clean:
	-rm -f *.o
	-rm -f ./bin/main_plain.exe ./bin/main_gwy.exe ./bin/main.exe
