.PHONY: all clean

TRG:=server
SRC:=$(wildcard *.c)

INC_PATH:=/usr/include \
          /usr/include/x86_64-linux-gnu

INC:=$(foreach dir,$(INC_PATH),-I$(dir))

CC_OPT:=-g --std=gnu99

all:
	gcc $(CC_OPT) $(INC) -o $(TRG) $(SRC)

clean:
	rm -f $(TRG)
