pms5003: pms5003.c
	indent -kr -i2 -nut $< || true
	gcc -O3 -g -Werror -Wall -Wextra -pedantic -o $@ $<

all: pms5003
