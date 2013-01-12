#include <stdio.h>

FILE *f = fopen("fred.txt", "w");
fwrite("hello\nhello\n", 1, 12, f);
fclose(f);

char freddy[7];
f = fopen("fred.txt", "r");
if (fread(freddy, 1, 6, f) != 6)
    printf("couldn't read fred.txt\n");

freddy[6] = '\0';
fclose(f);

printf("%s", freddy);

char InChar;
char ShowChar;
f = fopen("fred.txt", "r");
while ( (InChar = fgetc(f)) != EOF)
{
    ShowChar = InChar;
    if (ShowChar < ' ')
        ShowChar = '.';

    printf("ch: %d '%c'\n", InChar, ShowChar);
}
fclose(f);

f = fopen("fred.txt", "r");
while ( (InChar = getc(f)) != EOF)
{
    ShowChar = InChar;
    if (ShowChar < ' ')
        ShowChar = '.';

    printf("ch: %d '%c'\n", InChar, ShowChar);
}
fclose(f);

f = fopen("fred.txt", "r");
while (fgets(freddy, sizeof(freddy), f) != NULL)
    printf("x: %s", freddy);

fclose(f);

void main() {}
