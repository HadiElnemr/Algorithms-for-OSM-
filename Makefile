CC = g++
CFLAGS = -O3 -Wall -Wextra -std=c++11 -march=native -ffast-math -flto=auto #-fprofile-generate -fprofile-use
LDFLAGS = -lbz2 -lz -lexpat
TARGET_DIR = bin
SRCS := $(wildcard *.cpp)
TARGETS := $(patsubst %.cpp,$(TARGET_DIR)/%,$(SRCS))

.PHONY: all clean

all: $(TARGETS)
# compile all cpp files in the current directory but start with main.cpp
#all: $(TARGET_DIR)/main $(TARGETS)

$(TARGET_DIR)/main: main.cpp $(TARGETS) | $(TARGET_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)


# $(TARGET_DIR)/%: %.cpp | $(TARGET_DIR)
	# $(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(TARGET_DIR):
	mkdir -p $(TARGET_DIR)

clean:
	rm -rf $(TARGET_DIR)

