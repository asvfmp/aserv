CC=gcc
#CC=arm-poky-linux-gnueabi-gcc
#LD=arm-poky-linux-gnueabi-ld
#CFLAGS=”${CFLAGS} --sysroot=<sysroot-dir>”
#CXXFLAGS=”${CXXFLAGS} --sysroot=<sysroot-dir>”
SOURCES=ioa.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=ioa

#all: $(SOURCES) $(EXECUTABLE)
#
#$(EXECUTABLE): $(OBJECTS)
#    $(CC) $(LDFLAGS) $(OBJECTS) -o $@
#
#.c.o:                                                                                     
#    $(CC) $(CFLAGS) $< -o $@      

ioa:
	$(CC) $(SOURCES) -o $(EXECUTABLE)
clean:
	rm -rf *.o $(EXECUTABLE)