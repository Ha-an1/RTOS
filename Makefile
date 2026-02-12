################################################################################
# Makefile — RTOS Task Scheduler
#
# Targets:
#   make        Build the executable
#   make clean  Remove build artifacts
#   make test   Build and run all test scenarios
#   make demo   Build and run the priority inheritance demo (test 3)
################################################################################

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2
LDFLAGS = -lm

# Source files
SRCS = task.c scheduler.c rtos_time.c mutex.c semaphore.c timeline.c tests.c main.c

# Object files
OBJS = $(SRCS:.c=.o)

# Output binary
TARGET = rtos_scheduler

# ── Default target ───────────────────────────────────────────────────────────

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Convenience targets ──────────────────────────────────────────────────────

test: $(TARGET)
	./$(TARGET) all

demo: $(TARGET)
	./$(TARGET) 3

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

# ── Header dependencies ─────────────────────────────────────────────────────

task.o:      task.c task.h scheduler.h timeline.h mutex.h
scheduler.o: scheduler.c scheduler.h task.h timeline.h
timeline.o:  timeline.c timeline.h task.h mutex.h
mutex.o:     mutex.c mutex.h task.h scheduler.h timeline.h
semaphore.o: semaphore.c semaphore.h task.h scheduler.h
rtos_time.o: rtos_time.c rtos_time.h scheduler.h task.h timeline.h
tests.o:     tests.c task.h scheduler.h mutex.h semaphore.h timeline.h rtos_time.h
main.o:      main.c

.PHONY: all test demo clean
