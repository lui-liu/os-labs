CC = gcc
CFLAGS = -O2 -Wall -pthread

THREADS = 1 2 4 6 8 12 16 128 1024
SIZE = 2048
WINDOW = 5
K = 10
RUNS = 3

main: main.c
	$(CC) $(CFLAGS) main.c -o main

test: main
	./main 2 5 5 3 3
	cat steps.txt

bench: main
	@echo "потоки прогон T_seq T_par результат" | tee results_$(SIZE).txt
	@for t in $(THREADS); do \
		for r in $$(seq $(RUNS)); do \
			./main $$t $(SIZE) $(SIZE) $(WINDOW) $(K) | awk -v t=$$t -v r=$$r \
				'/Последовательно/{ts=$$2} /Параллельно/{tp=$$(NF-1)} /Результаты/{ok=$$2} END{print t, r, ts, tp, ok}' \
				| tee -a results_$(SIZE).txt; \
		done; \
	done

clean:
	rm -f main steps.txt results_*.txt

.PHONY: test bench clean
