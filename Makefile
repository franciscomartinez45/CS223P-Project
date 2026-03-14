CXX      = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -pthread

UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
  INCLUDES = -I/opt/homebrew/include
  LIBS     = -L/opt/homebrew/lib -lrocksdb -ldl -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd
else
  INCLUDES = -I/usr/local/include
  LIBS     = -L/usr/local/lib -lrocksdb -ldl -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd
endif

SRC_DIR  = src
SRCS     = $(SRC_DIR)/main.cpp \
           $(SRC_DIR)/database.cpp \
           $(SRC_DIR)/transaction.cpp \
           $(SRC_DIR)/workload.cpp \
           $(SRC_DIR)/workload_interp.cpp

TARGET   = transaction_system

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)
	rm -rf /tmp/project_testdb

.PHONY: all clean
