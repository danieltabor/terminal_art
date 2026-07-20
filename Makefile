all: island life

island: island.c
	$(CC) -o $@ $^ -static

life: life.c
	$(CC) -g -o $@ $^ -static

clean:
	rm -f island
	rm -f life
