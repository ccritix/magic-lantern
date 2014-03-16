/*
@title Hello, World!
*/

void main()
{
    printf("Hello from TCC!\n");
    msleep(1000);
    
    printf("Let's try some floating point numbers!\n");
    for (int i = 1; i < 10; i++)
    {
        /* this one works */
        printf("%d ", (int)((10.0 / i) * 10.0));
    }
    for (int i = 1; i < 10; i++)
    {
        /* this one doesn't work, figure out why */
        printf("%d ", (int)(i * 3.14f));
    }
    printf("\n");
}
