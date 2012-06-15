#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	char k[1000];
	char v[100 * 1000];
	char line[200 * 1000];
	unsigned int i, j, vlen, klen, len, pf;

	FILE *fp = fopen(argv[1], "r");
	// SQL-like K/V parser
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
		// take it while it hot...
		pf = ftell(fp) - vlen;
		printf("%s%c%d%c%d\n", k, 0xffffff88, vlen + 1,
					  0xffffffaf, pf - 4);
	}
	fclose(fp);

	return 0;
}

