
PREFIX  = $(HOME)/machines/$(HOST)
#PREFIX  = /usr/local

all     : plutorec plutorecg plutosend

plutorec        : plutorec.c
	gcc -Wall -Wextra -g plutorec.c -liio -lm -o plutorec

plutorecg       : plutorec
	ln -f plutorec plutorecg
        
plutosend       : plutosend.c
	gcc -Wall -Wextra -g plutosend.c -liio -lm -o plutosend

install : all
	install plutosend plutorec* $(PREFIX)/bin

