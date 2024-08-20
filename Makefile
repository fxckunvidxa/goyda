CFLAGS = -O2 -mwindows -lgdi32 -s

all: clean goyda.exe

clean:
	rm -f *.exe *.o

%.o: %.bmp
	$(LD) -r -b binary $< -o $@

goyda.exe: goyda.c svo.o
	$(CC) $(CFLAGS) $^ -o $@