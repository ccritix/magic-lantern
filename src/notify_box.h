#ifndef __NOTIFY_BOX_H_
#define __NOTIFY_BOX_H_

#define NOTIFY_BOX_TEXT_LENGTH 256

typedef struct
{
    uint32_t timeout;
    char *message;
    char *icon_file;
    struct bmp_file_t *icon_data;
} notify_job_t;

void NotifyBoxHide();
void NotifyBox(uint32_t timeout, char* fmt, ...) ;
void NotifyBoxIcon(uint32_t timeout, char *icon, char* fmt, ...);

#endif
