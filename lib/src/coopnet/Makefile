#################
# Build options #
#################

OSX_BUILD ?= 0
LOGGING ?= 0

#################

CXX = g++
CXXFLAGS = -Wall -Werror -Wno-unused-function -std=c++11 -fPIC -DJUICE_STATIC -g
INCLUDES = -Icommon -Ilib/include
LDFLAGS = -pthread
SERVER_LIBS = -lcurl

COMMON_SRC := $(wildcard common/*.cpp)
COMMON_OBJ := $(patsubst common/%.cpp, bin/o/common/%.o, $(COMMON_SRC))

CLIENT_SRC = $(wildcard client/*.cpp) $(COMMON_SRC)
CLIENT_OBJ = $(patsubst %.cpp, bin/o/%.o, $(CLIENT_SRC))

SERVER_SRC = $(wildcard server/*.cpp) $(wildcard server/extra/*.cpp) $(COMMON_SRC)
SERVER_OBJ = $(patsubst %.cpp, bin/o/%.o, $(SERVER_SRC))

BIN_DIR = bin
LIB_DIR = lib
LIBS = -l:libjuice.a
DYNLIB_NAME = libcoopnet.so

ifeq ($(OS),Windows_NT)
  LIBS += -lws2_32 -liphlpapi -lbcrypt
  ifeq ($(shell uname -m),x86_64)
    LIB_DIR := lib/win64
  else
    LIB_DIR := lib/win32
    CXXFLAGS += -Wno-error=format
  endif
else ifeq ($(OSX_BUILD),1)
  CXXFLAGS += -DOSX_BUILD=1
  LIB_DIR := ./lib/mac
  DYNLIB_NAME := libcoopnet.dylib
  LIBS := -l juice
  LDFLAGS += -rpath . -dynamiclib -install_name @rpath/$(DYNLIB_NAME)
else
  LIB_DIR := lib/libjuice
  DUMMY != $(MAKE) -C $(LIB_DIR) >&2 || echo FAIL
  ifeq ($(DUMMY),FAIL)
    $(error Failed to build libjuice)
  endif
endif

ifeq ($(LOGGING),1)
  CXXFLAGS += -DLOGGING
endif

.PHONY: all client server lib dynlib clean

all: client server lib dynlib

client: $(CLIENT_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -L$(LIB_DIR) $(LDFLAGS) -o $(BIN_DIR)/$@ $(CLIENT_OBJ) $(LIBS)

server: $(SERVER_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -L$(LIB_DIR) $(LDFLAGS) -o $(BIN_DIR)/$@ $(SERVER_OBJ) $(LIBS) $(SERVER_LIBS)

lib: $(CLIENT_OBJ) | $(BIN_DIR)
	ar rcs $(BIN_DIR)/libcoopnet.a $(COMMON_OBJ)

dynlib: $(CLIENT_OBJ) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -L$(LIB_DIR) $(LDFLAGS) -shared -o $(BIN_DIR)/$(DYNLIB_NAME) $(COMMON_OBJ) $(LIBS)

bin/o/%.o: %.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@
#	clang-tidy $< --checks="bugprone-*,-bugprone-unused-return-value,cert-*,cppcoreguidelines-*,hicpp-*,misc-*,performance-*,-cppcoreguidelines-avoid-magic-numbers,-cppcoreguidelines-pro-type-vararg,-misc-unused-parameters,-hicpp-vararg,-hicpp-uppercase-literal-suffix" -- $(INCLUDES)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)
	mkdir -p $(BIN_DIR)/o
	mkdir -p $(BIN_DIR)/o/client
	mkdir -p $(BIN_DIR)/o/server
	mkdir -p $(BIN_DIR)/o/server/extra
	mkdir -p $(BIN_DIR)/o/common

clean:
	rm -rf $(BIN_DIR)
	$(MAKE) -s -C $(LIB_DIR) dist-clean
