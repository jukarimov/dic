/*
    This file is a part of a dic
    Copyright (C) 2012 Jalil Karimov <jukarimov@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.*/

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

