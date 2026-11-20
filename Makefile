.DEFAULT_GOAL := all
CXX      := g++
CXXFLAGS := -O2 -std=c++17 -Wall -Isrc
LDFLAGS  :=
SRC      := $(wildcard src/*.cpp)
OBJ      := $(SRC:.cpp=.o)
ifeq ($(OS),Windows_NT)
TARGET   := BadApple.exe
LDFLAGS  += -lwinmm -static
RES      := src/app.res.o
OBJ      += $(RES)
else
TARGET   := bad_apple
RES      :=
endif
all: $(TARGET)
$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@
ifeq ($(OS),Windows_NT)
$(RES): src/app.rc assets/icon.ico
	windres src/app.rc -O coff -o $@
endif
tools/preprocess.exe: tools/preprocess.cpp
	$(CXX) -O2 -std=c++17 $< -o $@
# extract gray frames from the source video (needs ffmpeg)
raw:
	ffmpeg -v warning -i assets/bad_apple.mp4 -vf "scale=240:180:flags=area,format=gray" -r 60 -f rawvideo -y assets/frames.raw
preprocess: tools/preprocess.exe
	cd tools && preprocess.exe
clean:
	$(RM) $(OBJ) $(TARGET) tools/preprocess.exe src/app.res.o
.PHONY: all raw preprocess clean
