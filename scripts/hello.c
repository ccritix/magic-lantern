/*
@title Hello, World!
*/

#include "magic.h"

void main()
{
    printf("Hello from TCC!\n");
    sleep(1);
    
    printf("\n");
    printf("Let's try some floating point numbers!\n");
    for (int i = 1; i < 10; i++)
    {
        printf("%d ", (int)((10.0 / i) * 10.0));
    }
    for (int i = 1; i < 10; i++)
    {
        printf("%d ", (int)(i * 3.14f));
    }
    for (int i = 1; i < 10; i++)
    {
        printf("%d ", (int)(100 * sin(i)));
    }
    printf("\n");
}
