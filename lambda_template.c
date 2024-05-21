#include <stdio.h>

struct capture_group
{
	int id;
	char *name;
};

void print_info(void)
{
	static struct capture_group captures = {
		$@
	};

	printf("My id is %d\n", captures.id);
	printf("My name is %s\n", captures.name);
}
