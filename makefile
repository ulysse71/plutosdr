
PREFIX  = $(HOME)/machines/$(HOST)
#PREFIX  = /usr/local

all     : plutorec plutosend

plutorec        : plutorec.c
	gcc -Wall -Wextra -g plutorec.c -liio -lm -o plutorec
        
plutosend       : plutosend.c
	gcc -Wall -Wextra -g plutosend.c -liio -lm -o plutosend

install :
	install plutosend plutorec $(PREFIX)/bin

