
#include <stdlib.h>
#include <stdio.h>

int main()
{
    char buf[2048];
    char str[10] = "aaa";
    char str2[10] = "bbb";
    sprintf(buf, "aaa\n");
    sprintf(buf, "bbb");
    printf("%s\n", buf);
    return 0;
}