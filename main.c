#include <stdlib.h>
#include <stdio.h>

#define LAMBDA_LIB_NAME_FORMAT "liblambda%"

struct lambda
{
    char *lib_name;
    void *shared_object;
    void (*print_id)(void);
};

struct captures
{
    int id;
};

struct lambda *create_lambda(int id);
FILE *create_lambda_source_code(struct captures captures);

int main(void)
{
    struct captures locals;
    locals.id = 69;
    struct labmda *fn = create_lambda(locals);
    fn->print_id();
    free_labmda(fn);
    return 0;
}

struct lambda *create_lambda(struct captures captures)
{
    static unsigned int lambdas_generated = 0;

    struct lambda *lambda = NULL;
    FILE *source_code = NULL;
    int error = 0;

    source_code = create_lambda_source_code(captures);
    if (source_code == NULL)
    {
        return NULL;
    }

    lambda = compile_labmbda(source_code, LAMBDA_LIB_NAME_FORMAT, lambdas_generated);
    if (lambda == NULL)
    {
        return NULL;
    }

    error = open_lambda(lambda);
    if (error)
    {
        
        return NULL;
    }
    
    return lambda;
}

FILE *create_lambda_source_code(struct captures captures)
{
    int error;
    size_t source_code_length;
    char *source_code;
    char *capture_insert;
    char *pre_capture;
    char *post_capture;

    source_code = read_entire_file("lambda_template.c", &source_code_length);
    capture_insert = 2 + find_substring(source_code, "$@");
    if (error)
    {
        free(source_code);
        return NULL;
    }

    char *final_source_code = NULL;
    error = asprintf(&final_source_code,
                     "%s\n"
                     ".id = %d,"
                     "%s",
                     pre_capture, post_capture);
    
}
