
all: server.out

server.out: Server.cpp
	g++ -std=c++0x  -pthread Server.cpp -o server.out
	
test:
	echo "this is a test"

clean:
	rm server.out

.PHONY: all clean
