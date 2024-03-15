#include <stdio.h>

void print_id(void)
{
    struct captures
    {
        int id;
    };
    static struct captures captures = {
        $@
    };
    
    printf("My id is %d\n", captures.id);
}
