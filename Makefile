CXX      = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2 -pthread
INCLUDES = -I/usr/local/include
LIBS     = -L/usr/local/lib -lrocksdb -ldl -lpthread -lsnappy -lz -lbz2 -llz4 -lzstd

SRC_DIR  = src
SRCS     = $(SRC_DIR)/main.cpp \
           $(SRC_DIR)/database.cpp \
           $(SRC_DIR)/loader.cpp \
           $(SRC_DIR)/transaction.cpp \
           $(SRC_DIR)/occ.cpp \
           $(SRC_DIR)/twopl.cpp \
           $(SRC_DIR)/workload.cpp

TARGET   = txn_system

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRCS) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)
	rm -rf /tmp/project_testdb

.PHONY: all clean
