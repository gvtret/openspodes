CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O2
CORE     = ber.o axdr.o xdlms.o

all: demo demo_xdlms test golden_gen

demo: $(CORE) demo.o
	$(CC) $(CFLAGS) $^ -o $@

demo_xdlms: $(CORE) demo_xdlms.o
	$(CC) $(CFLAGS) $^ -o $@

test: $(CORE) test.o
	$(CC) $(CFLAGS) $^ -o $@

golden_gen: $(CORE) golden_gen.o
	$(CC) $(CFLAGS) $^ -o $@

libdlmscodec.a: $(CORE)
	$(AR) rcs $@ $^

%.o: %.c dlms_codec.h xdlms.h
	$(CC) $(CFLAGS) -c $< -o $@

# run the test suite AND verify the golden file matches the codec output
check: test golden_gen
	./test
	./golden_gen | diff -u golden_vectors.txt - && echo "golden_vectors.txt: in sync"

# regenerate the golden-vector reference from the codec
golden: golden_gen
	./golden_gen > golden_vectors.txt

clean:
	$(RM) *.o demo demo_xdlms test golden_gen libdlmscodec.a

.PHONY: all check golden clean
