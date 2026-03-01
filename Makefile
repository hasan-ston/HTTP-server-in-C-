CXX = g++
CXXFLAGS = -Wall -std=c++11

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp

clean:
	rm -f server
