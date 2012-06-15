

all: index dic

index:
	cc index.c -Wall -O2 -o $@
	
dic:
	cc parse.c linenoise.c -Wall -O2 -g -o $@

clean:
	rm -f *.idx *.o a.out *.dic *.sql index dic
