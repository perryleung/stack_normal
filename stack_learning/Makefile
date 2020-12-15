#
# Makefile
#

CC  = gcc -c
CXX = g++ -c 
LD = g++
DEBUG = -g -Wall
FLAGS = -std=c++11 -I./include -lboost_regex -lyaml-cpp -L/usr/local/lib /usr/local/lib/libev.a /usr/local/lib/libev.so -lev

%.o: %.c
	$(CC) $(DEBUG) -o $*.o $<  $(FLAGS) 

%.o: %.cc
	$(CXX) $(DEBUG) -o $*.o $<  $(FLAGS) 

all: RoastFish

# 含有main函数的demo.o必须放在第一位，否则会核心转存
RoastFish:  demo.o \
	dev/serial.o \
	dev/clientsocket.o \
	tra/layer_four.o \
	tra/udp.o \
	net/vbf.o \
	net/Srt.o \
	net/Dbpcr.o \
    net/Dbr.o \
	net/RangeTest.o\
	mac/aloha.o \
	mac/simple_aloha.o \
	mac/fama-ntr.o\
	phy/simulate.o \
	phy/simulateWOSS.o \
	phy/aquasent.o \
	phy/clisocket_sim.o \
	phy/qpskclient_cqh.o \
	libs/client.o \
	libs/libev_tool.o \
	libs/routetable.o \
	libs/tools.o \
	libs/trace.o \
    libs/routing_layer_info_record.o \
	mac/new_aloha.o \
	mac/fama_ntr_forsim.o \
	mac/D_CSMA_Node.o \
	mac/D_CSMA_Sink.o
	$(LD) -o RoastFish $^ $(FLAGS) 

clean:
	find . -name "*.o" -exec rm '{}' \;
	rm RoastFish
