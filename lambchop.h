#ifndef LAMBDA_H
#define LAMBDA_H

#include <stdio.h>  /* fprintf(), asprintf(), fopen(), fclose(), fread(),
		     * fwrite(), stderr, remove(), ftell(), fseek() */
#include <stdlib.h> /* malloc(), free() */
#include <unistd.h> /* fork(), execl() */
#include <sys/wait.h> /* waitpid() */
#include <dlfcn.h>  /* dlopen(), dlclode(), dlerror(), dlsym()*/
#include <string.h> /* strlen(), strerror(), strstr() */
#include <errno.h>  /* errno */

struct lambda_group
{
	// struct {int x, y;} var = { .x = 1, .y = -1 };
	//                            ↑↑↑↑↑↑↑↑↑↑↑↑↑↑↑
	// It should allocate a string which is freed by the caller
	char *(*create_capture_intializer)(void *captures);
	char *template_file;    // NULL => defaults to "lambda_template.c"
	char *function_name;    // NULL => defaults to "function" (needed for
				// dlsym())
};

struct lambda
{
	char *object_file_name;	// gcc output
	void *shared_object;	// = dlopen(object_file_name, RTLD_NOW)
	void *function;	// = dlsym(shared_object, function_name)
};

// Used by user
struct lambda *create_lambda(struct lambda_group lambda_type, void *captures);
void free_lambda(struct lambda *lambda);

// Lambda-specific functions
char *create_lambda_source_code(struct lambda_group lambda_type,
				void *captures);
struct lambda *compile_lambda(char *source_file);
int open_lambda(struct lambda *lambda, struct lambda_group lambda_type);

// General functions I did myself
char *read_entire_file(char *file_path, size_t *length);

#endif // LAMBDA_H



#ifdef LAMBCHOP_IMPLEMENTATION
#define CHILD_PID  0
#define TMP_C_FILE "/tmp/tmp_lambda.c"

struct lambda *create_lambda(struct lambda_group lambda_type, void *captures)
{
	if (captures == NULL)
	{
		fprintf(stderr, "Argument \'captures\' is NULL in"
			" \'create_lambda()\'\n");
		return NULL;
	}

	if (lambda_type.create_capture_intializer == NULL)
	{
		fprintf(stderr, "Argument \'lambda_type\' has NULL"
			" feild \'create_capture_intializer\'"
			" in \'create_lambda()\'\n");
		return NULL;
	}
	if (lambda_type.template_file == NULL)
	{
		lambda_type.template_file = "lambda_template.c";
	}
	if (lambda_type.function_name == NULL)
	{
		lambda_type.function_name = "function";
	}

	char *source_file = create_lambda_source_code(lambda_type, captures);
	if (source_file == NULL)
	{
		fprintf(stderr, "Failed to create a file with the"
			" lambda source code\n");
		return NULL;
	}

	struct lambda *lambda = compile_lambda(source_file);
	if (lambda == NULL)
	{
		fprintf(stderr, "Failed to compile the lambda source code\n");
		return NULL;
	}

	int error = open_lambda(lambda, lambda_type);
	if (error)
	{
		fprintf(stderr, "Failed to open the shared object"
			" with the lambda\n");
		free(lambda);
		return NULL;
	}

	return lambda;
}

char *create_lambda_source_code(struct lambda_group lambda_type, void *captures)
{
	if (captures == NULL)
	{
		fprintf(stderr, "Argument \'captures\' is NULL in"
			" \'create_lambda_source_code()\'\n");
		return NULL;
	}

	if (lambda_type.create_capture_intializer == NULL)
	{
		fprintf(stderr, "Argument \'lambda_type\' has NULL"
			" feild \'create_capture_intializer\' in"
			" \'create_lambda_source_code()\'\n");
		return NULL;
	}

	if (lambda_type.template_file == NULL)
	{
		lambda_type.template_file = "lambda_template.c";
	}

	size_t source_code_length;
	char *source_code = read_entire_file(lambda_type.template_file,
					     &source_code_length);
	if (source_code == NULL)
	{
		fprintf(stderr, "Failed to read entire file: %s\n",
			lambda_type.template_file);
		return NULL;
	}

	char *code_after_captures = strstr(source_code, "$@");
	if (code_after_captures == NULL)
	{
		fprintf(stderr, "Failed to find substring \"$@\" "
			"in lambda code: %s\n", source_code);
		free(source_code);
		return NULL;
	}
	*code_after_captures = '\0'; // Replaces '$' with '\0' to make
				     // `source_code` null-terminated at the
				     // insert point, splitting the string
	code_after_captures += 2;    // Moves pointer to the character after '@'
				     // which is the rest of the code

	char *final_source_code = NULL;
	char *capture_initializer =
		lambda_type.create_capture_intializer(captures);
	if (capture_initializer == NULL)
	{
		free(source_code);
		fprintf(stderr, "Failed to create capture"
			" initializer string\n");
		return NULL;
	}
	int bytes_written = asprintf(&final_source_code,
				     "%s%s%s",
				     source_code, capture_initializer,
				     code_after_captures);
	free(capture_initializer);
	free(source_code);
	if (bytes_written <= 0)
	{
		fprintf(stderr, "Failed to print source code with captures\n");
		return NULL;
	}
	size_t final_source_code_length = strlen(final_source_code);

	FILE *temp_file = fopen(TMP_C_FILE, "w");
	if (temp_file == NULL)
	{
		fprintf(stderr, "Failed to open file %s: %s\n",
			TMP_C_FILE, strerror(errno));
		free(final_source_code);
		return NULL;
	}

	size_t write_size = fwrite(final_source_code, sizeof(char),
				   final_source_code_length, temp_file);
	free(final_source_code);
	fclose(temp_file);
	if (write_size < final_source_code_length)
	{
		fprintf(stderr, "Failed to write source code to file: %s\n",
			strerror(errno));
		remove(TMP_C_FILE);
		return NULL;
	}

	return TMP_C_FILE;
}

struct lambda *compile_lambda(char *source_file)
{
	static int lambdas_generated = 0;

	if (source_file == NULL)
	{
		fprintf(stderr, "Argument \'source_file\' is NULL"
			" in \'compile_lambda()\'\n");
		return NULL;
	}

	char *shared_object_name = NULL;
	int bytes_written = asprintf(&shared_object_name,
				     "/tmp/liblambda%d.so",
				     lambdas_generated);
	if (bytes_written <= 0)
	{
		fprintf(stderr, "Failed to format a name for"
			" the shared object\n");
		return NULL;
	}

	pid_t pid = fork();
	if (pid == -1)
	{
		fprintf(stderr, "Failed to fork: %s\n", strerror(errno));
		free(shared_object_name);
		return NULL;
	}
	else if (pid == CHILD_PID)
	{
		execl("/usr/bin/cc", "cc", "-o", shared_object_name,
		      "-fPIC", "-shared", source_file, NULL);
		exit(127); // on error, exit
		// TODO: Do I return NULL? Do I free() do I exit()?
	}

	pid_t child_pid = wait(NULL);
	if (child_pid == -1)
	{
		fprintf(stderr, "Failed to wait for child to terminate: %s\n",
			strerror(errno));
		free(shared_object_name);
		return NULL;
	}

	struct lambda *lambda = malloc(sizeof(struct lambda));
	if (lambda == NULL)
	{
		fprintf(stderr, "Failed to allocate a lambda structure: %s\n",
			strerror(errno));
		free(shared_object_name);
		return NULL;
	}

	lambda->object_file_name = shared_object_name;
	lambda->shared_object = NULL;
	lambda->function = NULL;
	remove(source_file);
	++lambdas_generated;

	return lambda;
}

int open_lambda(struct lambda *lambda, struct lambda_group lambda_type)
{
	if (lambda == NULL ||
	    lambda->object_file_name == NULL ||
	    lambda->shared_object != NULL ||
	    lambda->function != NULL)
	{
		fprintf(stderr, "Argument \'lambda\' in \'open_lambda\'"
			" is NULL or has invalid members\n");
		return 1;
	}

	if (lambda_type.create_capture_intializer == NULL)
	{
		fprintf(stderr, "Argument \'lambda_type\' has NULL feild"
			" \'create_capture_intializer\' in"
			" \'create_lambda_source_code()\'\n");
		return 2;
	}

	if (lambda_type.function_name == NULL)
	{
		lambda_type.function_name = "function";
	}

	lambda->shared_object = dlopen(lambda->object_file_name, RTLD_NOW);
	if (lambda->shared_object == NULL)
	{
		fprintf(stderr, "Failed to open lambda shared object: %s\n",
			dlerror());
		return 3;
	}

	lambda->function = dlsym(lambda->shared_object,
				 lambda_type.function_name);
	if (lambda->function == NULL)
	{
		fprintf(stderr, "Failed to simulate lambda: %s\n", dlerror());
		dlclose(lambda->shared_object);
		return 4;
	}

	return 0;
}

void free_lambda(struct lambda *lambda)
{
	if (lambda == NULL)
	{
		return;
	}

	if (lambda->object_file_name != NULL)
	{
		remove(lambda->object_file_name);
		free(lambda->object_file_name);
	}

	if (lambda->shared_object != NULL)
	{
		dlclose(lambda->shared_object);
	}

	free(lambda);
}

char *read_entire_file(char *file_path, size_t *length)
{
	if (file_path == NULL || length == NULL)
	{
		fprintf(stderr, "Argument \'file_path\' or \'length\' is"
			" NULL in \'read_entire_file\'\n");
		return NULL;
	}

	*length = 0;
	FILE *file = fopen(file_path, "rb");
	if (file == NULL)
	{
		fprintf(stderr, "Failed to open %s: %s\n",
			file_path, strerror(errno));
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	*length = ftell(file);
	fseek(file, 0, SEEK_SET);
	char *buffer = malloc(*length + 1);
	if (buffer == NULL)
	{
		fprintf(stderr, "Failed to allocate space for a buffer"
			" of size %lu: %s\n", (*length + 1), strerror(errno));
		fclose(file);
		return NULL;
	}

	fread(buffer, sizeof(char), *length, file);
	buffer[*length] = '\0';
	fclose(file);

	return buffer;
}

#undef LAMBCHOP_IMPLEMENTATION
#endif // LAMBCHOP_IMPLEMENTATION
