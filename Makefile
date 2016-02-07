
all: scan_bad

scan_bad: scan_bad.c
	arm-linux-gcc -Wall -o scan_bad scan_bad.c
