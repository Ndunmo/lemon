#include <stdio.h>
#include <unistd.h>

int main(void) {
	int i;

	for(i = 0; i < 5;i++)
		printf("%d\n", sleep(1));

	return 0;
}
