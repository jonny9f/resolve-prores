CXX = g++
CXXFLAGS = -std=c++11 -Wall -I/usr/include/opencv4 -g 
LIBS =  -lavformat -lavcodec -lavutil `pkg-config --cflags --libs opencv4`

SOURCES = main.cpp
EXECUTABLE = prores_encoder

all: $(SOURCES)
		$(CXX) $(CXXFLAGS) $(SOURCES) -o $(EXECUTABLE) $(LIBS)

clean:
		rm -f $(EXECUTABLE)
