# For systems without CMake
CC = g++
FLAGS = -O3 -std=c++17
INC = -I./header -I./parser -I./passes -I./dump

all: qasmtrans

qasmtrans: src/qasmtrans.cpp #dump/dump_qasm.hpp parser/lexer.hpp parser/parser_util.hpp parser/qasm_parser.hpp parser/qasm_parser_expr.hpp passes/transpiler.hpp passes/decompose.hpp passes/remapping.hpp passes/routing_mapping.hpp  
	$(CC) $(FLAGS) $(INC) src/qasmtrans.cpp -o $@
