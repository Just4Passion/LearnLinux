
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>


int main()
{
    int fd = -1;
    int counter = 0;
    int old_counter = 0;

    fd = open("/dev/second", O_RDONLY);
    if (fd != -1)
    {
        while (1)
        {
            read(fd, &counter, sizeof(counter));
            if (counter != old_counter)
            {
                printf("seconds after open /dev/second: %d\r\n", counter);
                old_counter = counter;
            }
        }
    }
    else
    {
        printf("open device failedr\r\n");
    }

    return 0;
}

