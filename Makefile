
INC = -Iinclude
LIB = -lpthread

SRC = src
OBJ = obj
INCLUDE = include

CC := gcc
detected_OS := $(shell sh -c 'uname 2>/dev/null || echo Unknown')

ifeq ($(detected_OS), Darwin)
	CC := clang
endif

ifeq ($(DEBUG),TRUE)
	DEBUG=-g -O0 -DDEBUG
else
	DEBUG=-g -O3
endif
CFLAGS = -Wall -Wextra -c $(DEBUG)
LFLAGS = -Wall -Wextra -flto $(DEBUG)

vpath %.c $(SRC)
vpath %.h $(INCLUDE)

MAKE = $(CC) $(INC) 

# Object files needed by modules
MEM_OBJ = $(addprefix $(OBJ)/, paging.o mem.o cpu.o loader.o)
OS_OBJ = $(addprefix $(OBJ)/, mem.o cpu.o loader.o queue.o os.o sched.o timer.o)
SCHED_OBJ = $(addprefix $(OBJ)/, cpu.o loader.o mem.o queue.o os.o sched.o timer.o)
HEADER = $(wildcard $(INCLUDE)/*.h)

all: mem sched os test_all

# Just compile memory management modules
mem: $(MEM_OBJ)
	$(MAKE) $(LFLAGS) $(MEM_OBJ) -o mem $(LIB)

# Just compile scheduler
sched: $(SCHED_OBJ)
	$(MAKE) $(LFLAGS) $(SCHED_OBJ) -o os $(LIB)

# Compile the whole OS simulation
os: $(OS_OBJ)
	$(MAKE) $(LFLAGS) $(OS_OBJ) -o os $(LIB)

test_all: test_mem test_sched test_os

test_mem: mem
	@echo ------ MEMORY MANAGEMENT TEST 0 ------------------------------------
	./mem ./input/proc/m0
	@echo NOTE: Read file output/m0 to verify your result
	@echo ------ MEMORY MANAGEMENT TEST 1 ------------------------------------
	./mem ./input/proc/m1
	@echo 'NOTE: Read file output/m1 to verify your result (your implementation should print nothing)'

test_sched: os
	@echo ------ SCHEDULING TEST 0 -------------------------------------------
	./os ./input/sched_0
	@echo NOTE: Read file output/sched_0 to verify your result
	@echo ------ SCHEDULING TEST 1 -------------------------------------------
	./os ./input/sched_1
	@echo NOTE: Read file output/sched_1 to verify your result
	
test_os: os
	@echo ----- OS TEST 0 ----------------------------------------------------
	./os ./input/os_0
	@echo NOTE: Read file output/os_0 to verify your result
	@echo ----- OS TEST 1 ----------------------------------------------------
	./os ./input/os_1
	@echo NOTE: Read file output/os_1 to verify your result

run_sched_gen_gantt_chart: sched
	@echo ----- SCHEDULING TEST 0 -------------------------------------------
	./os ./input/sched_0 | tee report/sched_0_output.txt | ./report/grantt_gen/gen.py
	@echo NOTE: os output will be written into report/sched_0_output.txt
	@echo ----- SCHEDULING TEST 1 -------------------------------------------
	./os ./input/sched_1 | tee report/sched_1_output.txt | ./report/grantt_gen/gen.py
	@echo NOTE: os output will be written into report/sched_1_output.txt

run_os_gen_gantt_chart: os
	@echo ----- OS TEST 0 ----------------------------------------------------
	./os ./input/os_0 | tee report/os_0_output.txt | ./report/grantt_gen/gen.py
	@echo NOTE: os output will be written into report/os_0_output.txt
	@echo ----- OS TEST 1 ----------------------------------------------------
	./os ./input/os_1 | tee report/os_1_output.txt | ./report/grantt_gen/gen.py
	@echo NOTE: os output will be written into report/os_1_output.txt 

$(OBJ)/%.o: %.c ${HEADER}
	$(MAKE) $(CFLAGS) $< -o $@

clean:
	rm -f obj/*.o os sched mem report/*.txt



