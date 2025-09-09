CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -Iinclude
SRC = src/main.c src/log.c src/lock.c src/unlock.c src/meta.c
OUTDIR = build
OUT = $(OUTDIR)/securefs

all: $(OUT)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUT): $(OUTDIR) $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC)

clean:
	rm -rf build/*.o build/securefs

.PHONY: all clean
