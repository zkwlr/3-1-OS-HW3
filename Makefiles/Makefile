SRCS = *.cpp

.PHONY: all
all: project3

project2: $(SRCS) main.cpp
	g++ -pedantic -std=c++17 -o $@ $^

.PHONY: clean
clean:
	rm -f project3