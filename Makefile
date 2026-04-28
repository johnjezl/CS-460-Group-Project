CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

TARGET = assignment5

OBJS = main_pa2.o comment_stripper.o lexer.o parser.o cst.o ast.o symbol_table.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

main_pa2.o: main_pa2.cpp comment_stripper.h lexer.h parser.h cst.h ast.h tokens.h symbol_table.h
	$(CXX) $(CXXFLAGS) -c main_pa2.cpp -o main_pa2.o

comment_stripper.o: comment_stripper.cpp comment_stripper.h
	$(CXX) $(CXXFLAGS) -c comment_stripper.cpp -o comment_stripper.o

lexer.o: lexer.cpp lexer.h tokens.h
	$(CXX) $(CXXFLAGS) -c lexer.cpp -o lexer.o

parser.o: parser.cpp parser.h tokens.h cst.h symbol_table.h
	$(CXX) $(CXXFLAGS) -c parser.cpp -o parser.o

cst.o: cst.cpp cst.h
	$(CXX) $(CXXFLAGS) -c cst.cpp -o cst.o

ast.o: ast.cpp ast.h cst.h
	$(CXX) $(CXXFLAGS) -c ast.cpp -o ast.o

symbol_table.o: symbol_table.cpp symbol_table.h
	$(CXX) $(CXXFLAGS) -c symbol_table.cpp -o symbol_table.o

clean:
	rm -f $(OBJS) $(TARGET)