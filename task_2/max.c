#include <stdio.h>

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("Недостаточно аргументов!\n");
        return 1;
    }
    else
    {
        if (argv[1] > argv[2]) printf("%s\n", argv[1]);
        else printf("%s\n", argv[2]);
        return 0;
    }
}
