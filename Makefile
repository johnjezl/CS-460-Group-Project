CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

TARGET5 = assignment5
TARGET6 = assignment6

OBJS5 = main_pa2.o comment_stripper.o lexer.o parser.o cst.o ast.o symbol_table.o
OBJS6 = main_pa6.o comment_stripper.o lexer.o parser.o cst.o ast.o symbol_table.o interpreter.o

all: $(TARGET5) $(TARGET6)

$(TARGET5): $(OBJS5)
	$(CXX) $(CXXFLAGS) -o $(TARGET5) $(OBJS5)

$(TARGET6): $(OBJS6)
	$(CXX) $(CXXFLAGS) -o $(TARGET6) $(OBJS6)

main_pa2.o: main_pa2.cpp comment_stripper.h lexer.h parser.h cst.h ast.h tokens.h symbol_table.h
	$(CXX) $(CXXFLAGS) -c main_pa2.cpp -o main_pa2.o

main_pa6.o: main_pa6.cpp comment_stripper.h lexer.h parser.h cst.h interpreter.h tokens.h symbol_table.h
	$(CXX) $(CXXFLAGS) -c main_pa6.cpp -o main_pa6.o

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

interpreter.o: interpreter.cpp interpreter.h cst.h
	$(CXX) $(CXXFLAGS) -c interpreter.cpp -o interpreter.o

clean:
	rm -f $(OBJS5) $(OBJS6) $(TARGET5) $(TARGET6) $(TARGET5).exe $(TARGET6).exe
