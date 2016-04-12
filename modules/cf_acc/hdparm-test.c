/* for testing the stripped-down hdparm on the PC */

#include <stdio.h>

extern void hdparm_identify(int,int);

int do_drive_cmd (int fd, unsigned char *args, unsigned int timeout_secs)
{
    printf(
        "do_drive_cmd(%d, [%x %x %x %x], %d)\n",
        fd, args[0], args[1], args[2], args[3], timeout_secs
    );
    return 0;
}

void main()
{
    hdparm_identify(1, 0);
}
