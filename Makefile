SRC = main.cc ipc.cc ethlolmsg.c bytequeue.c crc8.c

# Change this to your paths
IPATH = .
IPATH +=./ipc

VPATH = .
VPATH +=./ipc

# Change this to whatever name you like
EXE = test 


CXXFLAGS =-O2 -ggdb -Wall -DIPC_TEST 
CFLAGS = -O2 -ggdb -Wall -DIPC_TEST

#CXX=bfin-linux-uclibc-g++
#CC=bfin-linux-uclibc-gcc
#CXXFLAGS +=-mcpu=bf561
#CFLAGS +=-mcpu=bf561 

CXXFLAGS += $(patsubst %, -I%, $(IPATH))
LDFLAGS = -lpthread

TOBJECTS = $(SRC:%.cc=objects/%.o)
OBJECTS = $(TOBJECTS:%.c=objects/%.o)

all: objects $(EXE) 

objects:
	mkdir objects

$(EXE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

objects/%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@
	
objects/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -f $(EXE) objects/*.o objects/*.lst *~
	rm -rf objects

