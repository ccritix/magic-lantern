#ifndef __NOTIFY_BOX_H_
#define __NOTIFY_BOX_H_

#define NOTIFY_BOX_TEXT_LENGTH 256

typedef struct
{
    uint32_t timeout;
    /* position where to print the dialog */
    int32_t pos_x;
    int32_t pos_y;
    char *message;
    /* if specified, the icon is scaled */
    uint32_t icon_width;
    uint32_t icon_height;
    /* if there is an icon filename specified, we will open it and load+free the bitmap data */
    char *icon_file;
    /* if there is only bitmap data specified, it won't get free'd */
    struct bmp_file_t *icon_data;
} notify_job_t;

void NotifyBoxHide();
void NotifyBox(uint32_t timeout, char* fmt, ...) ;
void NotifyBoxIcon(uint32_t timeout, char *icon, char *fmt, ...);
void NotifyBoxIconFileAdv(uint32_t timeout, char *icon, int32_t icon_width, uint32_t icon_height, uint32_t pos_x, uint32_t pos_y, char *fmt, ...);
void NotifyBoxIconDataAdv(uint32_t timeout, struct bmp_file_t *icon, int32_t icon_width, uint32_t icon_height, uint32_t pos_x, uint32_t pos_y, char *fmt, ...);
#endif
