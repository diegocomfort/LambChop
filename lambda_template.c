#include <stdio.h>

struct captures
{
    int id;
};

void function(void)
{
    static struct captures captures = {
        $@
    };
    
    printf("My id is %d\n", captures.id);
}
