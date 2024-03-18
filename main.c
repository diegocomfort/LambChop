#include <stdio.h>  /* fprintf(), asprintf(), fopen(), fclose(), fread(), fwrite(), stderr */
#include <stdlib.h> /* malloc(), free() */
#include <unistd.h> /* fork(), execl() */
#include <sys/wait.h> /* waitpid() */
#include <dlfcn.h>  /* dlopen(), dlclode(), dlerror(), dlsym()*/
#include <string.h> /* strlen(), strerror() */
#include <errno.h>  /* errno */

#define CHILD_PID 0

struct lambda
{
    char *object_file;
    void *shared_object;
    void (*function)(void);
};

struct captures
{
    int id;
};

struct lambda *create_lambda(struct captures captures);
void free_labmda(struct lambda *lambda);

char *create_lambda_source_code(struct captures captures);
struct lambda *compile_lambda(char *source_file);
int open_lambda(struct lambda *lambda);

char *read_entire_file(char *file_path, size_t *length);
char *find_substring(char *text, char *substring);

int main(int argc, char **argv)
{
    struct captures locals;
    locals.id = 69;
    if (argc > 1)
    {
	locals.id = atoi(argv[1]);
    }
    struct lambda *fn = create_lambda(locals);
    if (fn == NULL)
    {
	printf("Failed to create a lambda :(\n");
	return 1;
    }
    fn->function();
    free_labmda(fn);
    fn = NULL;
    return 0;
}

struct lambda *create_lambda(struct captures captures)
{
    char *source_file = create_lambda_source_code(captures);
    if (source_file == NULL)
    {
	fprintf(stderr, "Failed to create a file with the lambda source code\n");
        return NULL;
    }

    struct lambda *lambda = compile_lambda(source_file);
    if (lambda == NULL)
    {
	fprintf(stderr, "Failed to compile the lambda source code\n");
        return NULL;
    }

    int error = open_lambda(lambda);
    if (error)
    {
	fprintf(stderr, "Failed to open the shared object with the lambda\n");
        free(lambda);
        return NULL;
    }
    
    return lambda;
}

char *create_lambda_source_code(struct captures captures)
{
    static char *file_name = "tmp_lambda.c";

    size_t source_code_length;
    char *lambda_code = "lambda_template.c";
    char *source_code = read_entire_file(lambda_code, &source_code_length);
    if (source_code == NULL)
    {
	fprintf(stderr, "Failed to read entire file: %s\n", lambda_code);
        return NULL;
    }

    char *code_after_captures = find_substring(source_code, "$@");
    if (code_after_captures == NULL)
    {
	fprintf(stderr, "Failed to find substring \"$@\" in lambda code: %s\n", source_code);
        free(source_code);
        return NULL;
    }
    *code_after_captures = '\0'; // Replaces '$' with '\0' to make `source_code` null-terminated
    code_after_captures += 2;    // Moves pointer to the character after '@' which is the rest of the code

    char *final_source_code = NULL;
    int bytes_written = asprintf(&final_source_code,
                     "%s\n"
                     ".id = %d,"
                     "%s",
                     source_code, captures.id, code_after_captures);
    free(source_code);
    if (bytes_written <= 0)
    {
	fprintf(stderr, "Failed to print source code with captures\n");
        return NULL;
    }
    size_t final_source_code_length = strlen(final_source_code);

    FILE *temp_file = fopen(file_name, "w");
    if (temp_file == NULL)
    {
	fprintf(stderr, "Failed to open file %s: %s\n", file_name, strerror(errno));
        free(final_source_code);
        return NULL;
    }

    size_t write_size = fwrite(final_source_code, sizeof(char), final_source_code_length, temp_file);
    free(final_source_code);
    fclose(temp_file);
    if (write_size < final_source_code_length)
    {
	fprintf(stderr, "Failed to write source code to file: %s\n", strerror(errno));
	remove(file_name);
        return NULL;
    }

    return file_name;
}

struct lambda *compile_lambda(char *source_file)
{
    static int lambdas_generated = 0;
    
    char *shared_object_name = NULL;
    int bytes_written = asprintf(&shared_object_name, "/tmp/liblambda%d.so", lambdas_generated);
    if (bytes_written <= 0)
    {
	fprintf(stderr, "Failed to format a name for the shared object\n");
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
	execl("/usr/bin/cc", "cc", "-o", shared_object_name, "-fPIC", "-shared", "-g", source_file, NULL);
	exit(127); // on error, exit
	// TODO: Do I return NULL? Do I free() do I exit()?
    }

    pid_t child_pid = wait(NULL);
    if (child_pid == -1)
    {
	fprintf(stderr, "Failed to wait for child to terminate: %s\n", strerror(errno));
	free(shared_object_name);
	return NULL;
    }
    
    struct lambda *lambda = malloc(sizeof(struct lambda));
    if (lambda == NULL)
    {
	fprintf(stderr, "Failed to allocate a lambda structure: %s\n", strerror(errno));
        free(shared_object_name);
        return NULL;
    }

    lambda->object_file = shared_object_name;
    lambda->shared_object = NULL;
    lambda->function = NULL;
    remove(source_file);
    ++lambdas_generated;

    return lambda;
}

int open_lambda(struct lambda *lambda)
{
    if (lambda == NULL ||
	lambda->object_file == NULL ||
	lambda->shared_object != NULL ||
	lambda->function != NULL)
    {
	fprintf(stderr, "Argument \'lambda\' in \'open_lambda\' is NULL or has invalid members\n");
	return 1;
    }

    lambda->shared_object = dlopen(lambda->object_file, RTLD_NOW);
    if (lambda->shared_object == NULL)
    {
	fprintf(stderr, "Failed to open lambda shared object: %s\n", dlerror());
	return 2;
    }

    // Casting to (void*) instead of function pointer to get around
    // pedantic warnings
    *(void**)&lambda->function = dlsym(lambda->shared_object, "function");
    if (lambda->function == NULL)
    {
	fprintf(stderr, "Failed to simulate lambda: %s\n", dlerror());
	dlclose(lambda->shared_object);
	return 3;
    }
    
    return 0;
}

void free_labmda(struct lambda *lambda)
{
    if (lambda == NULL)
    {
	return;
    }

    if (lambda->object_file != NULL)
    {
	remove(lambda->object_file);
	free(lambda->object_file);
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
	fprintf(stderr, "Argument \'file_path\' or \'length\' is NULL in \'read_entire_file\'\n");
	return NULL;
    }
    
    *length = 0;
    FILE *file = fopen(file_path, "rb");
    if (file == NULL)
    {
	fprintf(stderr, "Failed to open %s: %s\n", file_path, strerror(errno));
	return NULL;
    }

    fseek(file, 0, SEEK_END);
    *length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = malloc(*length + 1);
    if (buffer == NULL)
    {
	fprintf(stderr, "Failed to allocate space for a buffer of size %lu: %s\n", (*length + 1), strerror(errno));
	fclose(file);
	return NULL;
    }
    
    fread(buffer, sizeof(char), *length, file);
    buffer[*length] = '\0';
    fclose(file);

    return buffer;
}

char *find_substring(char *text, char *substring)
{
    if (text == NULL || substring == NULL)
    {
	fprintf(stderr, "Argument \'text\' or \'substring\' is NULL in \'find_substring\'\n");
	return NULL;
    }
    
    for (; *text != '\0'; ++text)
    {
	if (*text != *substring)
	    continue;

	/* if (strcmp(text, substring) == 0) */
	/* { */
	/*     return text; */
	/* } */

	// My own string comparison
	char *start_of_match = text;
	for (size_t i = 0; ; ++text, ++i)
	{
	    if (substring[i] == '\0')
		return start_of_match;
	    if (*text != substring[i])
		break;
	    if (*text == '\0')
		return NULL;
	}
    }

    return NULL;
}
