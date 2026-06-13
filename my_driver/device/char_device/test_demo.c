#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

char *wbuf = "Hello MyCharDevice\n";
char rbuf[128];

int main()
{
    printf("MyCharDevice test\n");
    int fd = open("/dev/MyCharDevice", O_RDWR);
    write(fd, wbuf, strlen(wbuf));
    close(fd);
     fd = open("/dev/MyCharDevice", O_RDWR);
    read(fd, rbuf, 128);
    printf("The content : %s \n", rbuf);
    close(fd);
    return 0;
}