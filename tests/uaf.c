#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define bufs 512
#define bufs2 ((bufs/2) - 8)

int main(void) {
	char *init = "FOO\n";

	char *buf1, *buf2, *buf3;

	buf1 = (char *) malloc(bufs);
	buf2 = (char *) malloc(bufs);

	free(buf2);
	buf3 = (char *) malloc(bufs2);
	strncpy(buf2, init, bufs - 1);

	free(buf3);
}
