#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

struct node {
	char *k, *v;
	struct node *ln;
	struct node *rn;
};

typedef struct node NODE;

#define init(var, type)	(var = (type *)malloc(sizeof(type)))

#define BUCKETS 	6000

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

NODE *treeadd(NODE *n, const char *k, const char *v)
{
	int cmp;
	if (n == NULL) {
		init(n, NODE);
		if (n == NULL)
			fprintf(stderr, "ERR:failed to init node\n");

		n->k = strdup(k);
		if (n->k == NULL)
			fprintf(stderr, "ERR:failed to copy key\n");
		n->v = strdup(v);
		if (n->v == NULL)
			fprintf(stderr, "ERR:failed to copy val\n");

		n->ln = n->rn = NULL;
	}
	else if ((cmp = strcmp(k, n->k)) > 0)
		n->rn = treeadd(n->rn, k, v);
	else if (cmp < 0)
		n->ln = treeadd(n->ln, k, v);
	else
		fprintf(stderr, "WARN: DUPLICATE key: %s\n", k);

	return n;
}

NODE *lookup(char *k, NODE *n)
{
	if (!n)
		return NULL;

	int cmp;
	if ((cmp = strcmp(k, n->k)) == 0)
		return n;
	else if (cmp > 0)
		return lookup(k, n->rn);
	else
		return lookup(k, n->ln);
	
}

void printree(NODE *n)
{
	if (n != NULL) {

		printree(n->ln);
		printf("%s: %s;\n", n->k, n->v);
		printree(n->rn);
	}
}

void freetree(NODE *n)
{
	if (n != NULL) {

		NODE *ln = n->ln;
		NODE *rn = n->rn;

		freetree(ln);
		freetree(rn);

		free(n->k);
		free(n->v);
		free(n);
	}
}

void htput(char *k, char *v)
{
	unsigned int h = hash(k);
	HT[h].n = treeadd(HT[h].n, k, v);
	++HT[h].len;
}

int main(int argc, char *argv[])
{
	char k[1000];
	char v[100 * 1000];
	char line[200 * 1000];
	unsigned int i, j, h, vlen, klen, len;
	for (i=0; i < BUCKETS; i++) {
		HT[i].n = NULL;
		HT[i].len = 0;
	}
	FILE *fp = fopen(argv[1], "r");
	while (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "INSERT")) {
			len = strlen(line) - 1;
			line[len] = 0;
			i = 0;
			while (line[i++] != '\'' && i < len)
				;
			j = 0;
			while (i < len && line[i] != '\'')
				k[j++] = line[i++];
			k[j] = 0;
			klen = j;

			i++;
			while (i < len && line[i] != '\'')
				i++;
			i++;

			j = 0;
			while (i < len && line[i] != '\'')
				v[j++] = line[i++];

			v[j] = 0;
			vlen = j;

		} else {
			printf("Expected INSERT, got %s\n", line);
			exit(-1);
		}

		htput(k, v);
		memset(k, 0, klen);
		memset(v, 0, vlen);
	}
	fclose(fp);

	//puts("==============================");

	while (fgets(k, sizeof(k), stdin)) {
		k[strlen(k) - 1] = 0;
		h = hash(k);
		if (HT[h].len) {
			NODE *n = lookup(k, HT[h].n);
			if (n) {
				puts(n->v);
				continue;
			}
		}
		puts("Not found");
	}

	for (i=0; i < BUCKETS; i++) {
		if (HT[i].len) {
			//printf("bucket %d: %d keys\n", i, HT[i].len);
			//printree(HT[i].n);
			freetree(HT[i].n);
		}
	}

	return 0;
}

