TARGET=../bin/us
CPPFLAGS+=-Wall -O2
LDLIBS+=-pthread
ifdef ANDROID_NDK_ROOT
LDLIBS+=-static-libstdc++
endif
#LDLIBS+=-lws2_32 # for mingw

all: $(TARGET)

$(TARGET): us.o ../mingw_net.o
	$(LINK.cpp) $^ $(LOADLIBES) $(LDLIBS) -o $@
ifdef ANDROID_NDK_ROOT
	termux-elf-cleaner --api-level 23 $(TARGET)
	llvm-strip $(TARGET)
endif	

clean:
	rm -f $(TARGET) *.o ../mingw_net.o
