CXX = g++
CXXFLAGS = -pedantic -std=c++17

SRCS = main.cpp
OBJS = $(SRCS:.cpp=.o)
EXEC = project3

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(EXEC)

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(EXEC)