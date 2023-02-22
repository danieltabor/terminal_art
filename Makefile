all: island

island: island.c
	$(CC) -o $@ $^ -static

clean:
	rm -f island
