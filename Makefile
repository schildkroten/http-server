FLAGS = -Wall -g

all: server clean

server: main.o parser.o
	gcc main.o parser.o -o bin/server $(FLAGS)

main.o:
	gcc main.c -c $(FLAGS)

parser.o:
	gcc src/parser.c -c $(FLAGS)

clean:
	rm -f main.o parser.o
