CC := gcc
CPP:= g++
CXX:= g++
CPPFLAGS  := -fPIC  `pkg-config libavcodec --cflags` `pkg-config libavformat --cflags` `pkg-config libswresample --cflags` `pkg-config libswscale --cflags` `pkg-config libavdevice --cflags` `pkg-config libavutil --cflags` -std=c++11
LDFLAGS :=  `pkg-config libavcodec --libs` `pkg-config libavformat --libs` `pkg-config libswresample --libs` `pkg-config libswscale --libs` `pkg-config libavdevice --libs` `pkg-config libavutil --libs` 

OBJS  := $(patsubst %.cpp,%.o,$(wildcard *.cpp))
AR := ar
LD   := g++
INSTALL_PATH := ../

libff.so:libff.a
	$(CXX) $(CFLAGS) -shared -Wl,--whole-archive $^ -Wl,--no-whole-archive -lc    $(LDFLAGS)  -o $@    


libff.dll:libff.lib
	$(CXX) $(CFLAGS) -shared -Wl,--whole-archive $^ -Wl,--no-whole-archive     $(LDFLAGS)  -o $@    

libff.a:$(OBJS)
	$(AR) r $@ $^

libff.lib:$(OBJS)
	$(AR) r $@ $^

test:test.o
	$(LD) $(CFLAGS)  $^ -o $@ -L. -lff $(LDFLAGS) 

clean:
	rm -fr libff.lib *.o libff.so libff.dll libff.a test $(OBJS)
		
install:libff.so
	cp -fr $^ $(INSTALL_PATH)

win_install:libff.dll
	cp -fr $^ $(INSTALL_PATH)
