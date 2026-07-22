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
RESEARCH_LIB ?= ../../build-i686/libresearch_gwy_shell_stubs.a
LAUNCHER_INC ?= ../../include

# Default make target keeps historical name but builds plain baseline.
.PHONY: main plain gwy gwy-research clean

main: plain

# Upstream baseline: strong unbound stubs (plain only; never linked into gwy).
plain: $(CORE_OBJS) gwy_vm_file_weak.o gwy_ext_obs_weak.o main.o
	$(CC) $(CFLAGS) -m32 -o ./bin/main_plain.exe $^ $(UNICORN) $(CAPSTONE) -lpthread -lm \
		-lws2_32 -lgdi32 -lmingw32 -L$(SDL2)/lib/ -lSDL2main -lSDL2
	@echo "[OK] built bin/main_plain.exe (stub unbound)"

# Product gwy: launcher_core + research stubs (no E10A/shell probe implementations).
# Dual targets: `plain` links strong stubs in gwy_vm_file_weak.c (no ELF weak; MinGW PE);
# `gwy` must not link gwy_vm_file_weak / gwy_ext_obs_weak.
gwy: CFLAGS += -DGWY_USE_VM_FILE_SERVICE -I$(LAUNCHER_INC)
gwy: $(CORE_OBJS) main.o
	@if [ ! -f "$(LAUNCHER_LIB)" ]; then echo "missing $(LAUNCHER_LIB); build gwy_launcher first"; exit 1; fi
	@if [ ! -f "$(RESEARCH_LIB)" ]; then echo "missing $(RESEARCH_LIB); build research stubs first"; exit 1; fi
	$(CC) $(CFLAGS) -m32 -o ./bin/main_gwy.exe $(CORE_OBJS) main.o \
		-Wl,--start-group $(LAUNCHER_LIB) $(RESEARCH_LIB) -Wl,--end-group \
		$(UNICORN) $(CAPSTONE) -lz -lpthread -lm -lws2_32 -lgdi32 -lmingw32 -L$(SDL2)/lib/ -lSDL2main -lSDL2
	@echo "[OK] built bin/main_gwy.exe (product: core + research stubs)"

# Research gwy: launcher_core + real research_gwy_shell (E10A / shell probes).
gwy-research: CFLAGS += -DGWY_USE_VM_FILE_SERVICE -I$(LAUNCHER_INC)
gwy-research: $(CORE_OBJS) main.o
	@if [ ! -f "$(LAUNCHER_LIB)" ]; then echo "missing $(LAUNCHER_LIB); build gwy_launcher first"; exit 1; fi
	@if [ ! -f "$(RESEARCH_LIB)" ]; then echo "missing $(RESEARCH_LIB); pass RESEARCH_LIB=libresearch_gwy_shell.a"; exit 1; fi
	$(CC) $(CFLAGS) -m32 -o ./bin/main_gwy_research.exe $(CORE_OBJS) main.o \
		-Wl,--start-group $(LAUNCHER_LIB) $(RESEARCH_LIB) -Wl,--end-group \
		$(UNICORN) $(CAPSTONE) -lz -lpthread -lm -lws2_32 -lgdi32 -lmingw32 -L$(SDL2)/lib/ -lSDL2main -lSDL2
	@echo "[OK] built bin/main_gwy_research.exe (research: core + research_gwy_shell)"

dll: $(CORE_OBJS) gwy_vm_file_weak.o gwy_ext_obs_weak.o
	$(CC) $(CFLAGS) -m32 -shared -o ./bin/vmrp.dll $^ $(UNICORN) $(CAPSTONE) -lpthread -lm -lws2_32 -lgdi32 -lmingw32

%.o: %.c
	$(CC) $(CFLAGS) -m32 -c $< -o $@

clean:
	-rm -f *.o
	-rm -f ./bin/main_plain.exe ./bin/main_gwy.exe ./bin/main_gwy_research.exe ./bin/main.exe
