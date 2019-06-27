
all:
	g++ -Wall -O3 -std=c++17 example.cc -o example
	./example example.com 80

