#define LAMBCHOP_IMPLEMENTATION
#include "lambchop.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

// There are the captures (static variables) that each lambda will
// have
struct captures
{
	int id;
	char *name;
};

// This function is used to print an initliazer for the captures
// struct. For example, if the captures passed to this funciton are
// id=10 and name="John", it will return this string:
// ".id = 10, .name = \"John\""
char *create_capture_initialzer(void *capture_variables)
{
	struct captures *captures = capture_variables;
	char *initializer = NULL;
	int bytes_written = asprintf(&initializer, ".id = %d, .name = \"%s\"",
				     captures->id, captures->name);
	if (bytes_written <= 0)
	{
		fprintf(stderr, "Failed to print capture initializer\n");
		return NULL;
	}
	return initializer;
}

int main (int argc, char **argv)
{
	if (argc <= 1)
	{
		fprintf(stderr, "usage: %s numbers...\n", argv[0]);
		return -1;
	}

	struct lambda_group my_lambdas;
	my_lambdas.create_capture_intializer = create_capture_initialzer;
	my_lambdas.function_name = "print_info";

	struct lambda *lambdas[argc - 1]; // Array of struct lambda pointers
	for (int i = 1; i < argc; ++i)
	{
		struct captures captures;
		captures.id = atoi(argv[i]);
		captures.name = argv[0];
		lambdas[i] = create_lambda(my_lambdas, &captures);
		if (lambdas[i] != NULL)
			continue;

		fprintf(stderr, "Failed to create %dth lambda\n", i);
		for (int j = 0; j < i; ++j)
		{
			free_lambda(lambdas[j]);
		}
		return -2;
	}

	for (int i = 1; i < argc; ++i)
	{
		void (*runtime_function)(void) = NULL;
		runtime_function = lambdas[i]->function;
		runtime_function();
		free_lambda(lambdas[i]);
	}

	return 0;
}
