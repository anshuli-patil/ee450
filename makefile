all:
	g++ -std=c++11 -o client client.cpp
	g++ -std=c++11 -o edge edge.cpp
	g++ -std=c++11 -o server_or server_or.cpp
	g++ -std=c++11 -o server_and server_and.cpp

$(phony server_and):
	./server_and

server_or:
	./server_or

edge:
	./edge


