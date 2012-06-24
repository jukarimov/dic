

all: index lookup

index:
	cc index.c -Wall -O2 -o $@
	
lookup:
	cc lookup.c linenoise.c -Wall -O2 -g -o $@

clean:
	rm -f *.idx *.o a.out *.dic *.sql index lookup .lookups
