# gmsh API (gmsh.h + libgmsh); set GMSH_DIR if gmsh is installed elsewhere
GMSH_DIR ?= /usr

CXX      ?= g++
CXXFLAGS ?= -O3 -DNDEBUG
CXXFLAGS += -std=c++17 -Wall -Wextra -I$(GMSH_DIR)/include
LDLIBS   = -L$(GMSH_DIR)/lib -lgmsh

SRC = main.cc reverse_cmk.cc update_labelling.cc msh22_writer.cc
OBJ = $(SRC:.cc=.o)

all: cmk

cmk: $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDLIBS)

%.o: %.cc cmk.h
	$(CXX) -c $(CXXFLAGS) $<

clean:
	rm -f *.o *~

cleanall: clean
	rm -f cmk

.PHONY: all clean cleanall
