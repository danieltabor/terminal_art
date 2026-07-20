all: island 1d-life 2d-life

island: island.c
	$(CC) -o $@ $^ -static

1d-life: 1d-life.c
	$(CC) -o $@ $^ -static

2d-life: 2d-life.c
	$(CC) -o $@ $^ -static

clean:
	rm -f island
