all: island 1d-life

island: island.c
	$(CC) -o $@ $^ -static

1d-life: 1d-life.c
	$(CC) -o $@ $^ -static

clean:
	rm -f island
