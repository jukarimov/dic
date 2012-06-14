#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
	int fd = open(argv[1], O_RDONLY);

	struct stat sb;
	fstat(fd, &sb);

	int sz;
	sz = sb.st_size;

	void *ad;
	ad = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);

	int offt = 1999, len = 108;

	char *p = ad + offt;

	while (--len)
	{
		printf("%c", *p++);
	}

	puts("");

	return 0;
}
