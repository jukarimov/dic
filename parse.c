#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

struct index {
	char	*k;
	int	vlen;
	int	offt;
};

typedef struct index	IDX;

struct node {
	struct index	*idx;
	struct node	*ln, *rn;
};

typedef struct node NODE;

#define init(var, type)	(var = (type *)malloc(sizeof(type)))

#define BUCKETS 	200000

#define	DCHARHASH(h, c)	((h) = 0x63c63cd9*(h) + 0x9c39c33d + (c))

struct bucket {
	NODE *n;
	int len;
};

struct bucket HT[BUCKETS];

unsigned int hash(char *k)
{
	unsigned int h;
	for (h=0; *k; ++k)
		DCHARHASH(h, *k);

	return h % BUCKETS;
}

NODE *treeadd(NODE *n, const char *k, int vlen, int offt)
{
	int cmp;
	if (n == NULL) {
		init(n, NODE);
		if (n == NULL)
			fprintf(stderr, "ERR:failed to init node\n");

		init(n->idx, IDX);
		if (n->idx == NULL)
			fprintf(stderr, "ERR:failed to init node\n");

		n->idx->k = strdup(k);
		if (n->idx->k == NULL)
			fprintf(stderr, "ERR:failed to copy key\n");

		n->idx->vlen = vlen;
		n->idx->offt = offt;

		n->ln = n->rn = NULL;
	}
	else if ((cmp = strcmp(k, n->idx->k)) > 0)
		n->rn = treeadd(n->rn, k, vlen, offt);
	else if (cmp < 0)
		n->ln = treeadd(n->ln, k, vlen, offt);
	else
		fprintf(stderr, "WARN: DUPLICATE key: %s\n", k);

	return n;
}

NODE *lookup(char *k, NODE *n)
{
	if (!n)
		return NULL;

	int cmp;
	if ((cmp = strcmp(k, n->idx->k)) == 0)
		return n;
	else if (cmp > 0)
		return lookup(k, n->rn);
	else
		return lookup(k, n->ln);
	
}

void freetree(NODE *n)
{
	if (n != NULL) {

		NODE *ln = n->ln;
		NODE *rn = n->rn;

		freetree(ln);
		freetree(rn);

		free(n->idx->k);
		free(n->idx);
		free(n);
	}
}

void htput(char *k, int vlen, int offt)
{
	unsigned int h = hash(k);
	HT[h].n = treeadd(HT[h].n, k, vlen, offt);
	++HT[h].len;
}

FILE *CFP;

int fgetln(char *line)
{
	int c;
	size_t i = 0;
	while ((c=fgetc(CFP)) != EOF) {
		if (c == '\n') {
			line[i] = 0;
			return i;
		}
		line[i++] = c;
	}
	fclose(CFP);
	return -1;
}

void parse(const char *line, char *key, int *len, int *oft)
{
	while (*line != '\n') {
		*key++ = *line++;
		if (*line == 0xffffff88) {
			*key = 0;
			line++;
			*len = atoi(line);
			while (*line++ != 0xffffffaf)
				;
			*oft = atoi(line);
			return;
		}
	}
}

int main(int argc, char *argv[])
{
	char line[8000];
	char k[8000];
	int i, h, vlen, offt, len;

	if (argc < 3) {
		puts("Usage: run [idx] [dic]");
		exit(-1);
	}

	CFP = fopen(argv[1], "r");

	for (i=0; i < BUCKETS; i++) {
		HT[i].n = NULL;
		HT[i].len = 0;
	}

	while ((len=fgetln(line)) != -1) {

		parse(line, k, &vlen, &offt);
		//printf("%s:%d@%d\n", k, vlen, offt);
		htput(k, vlen, offt);
	}

	int fd = open(argv[2], O_RDONLY);

	struct stat sb;
	fstat(fd, &sb);

	int sz;
	sz = sb.st_size;

	void *ad;
	ad = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);

	while (printf("\n>>>") && fgets(k, sizeof(k), stdin)) {
		
		len = strlen(k) - 1;
		if (len <= 1)
			continue;
		k[len] = 0;
		h = hash(k);
		if (HT[h].len) {
			NODE *n = lookup(k, HT[h].n);
			if (n) {

				char *p = ad + n->idx->offt;

				while (--n->idx->vlen)
					printf("%c", *p++);

			} else
				puts("Not found");
		}
		//printf(">");
	}

	close(fd);

	for (i=0; i < BUCKETS; i++) {
		if (HT[i].len) {
			//printf("bucket %d: %d keys\n", i, HT[i].len);
			freetree(HT[i].n);
		}
	}

	return 0;
}
