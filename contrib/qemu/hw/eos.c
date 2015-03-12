#include "hw/hw.h"
#include "hw/loader.h"
#include "sysemu/sysemu.h"
#include "hw/devices.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "exec/memory-internal.h"
#include "hw/sysbus.h"
#include "qemu/thread.h"
#include "dirent.h"
#include "ui/console.h"
#include "ui/pixel_ops.h"
#include "hw/display/framebuffer.h"
#include "eos.h"

EOSRegionHandler eos_handlers[] =
{
    { "RAM Trace",    0x00000000, 0x10000000, eos_handle_ram, 0 },
    { "FlashControl", 0xC0000000, 0xC0001FFF, eos_handle_flashctrl, 0 },
    { "ROM0",         0xF8000000, 0xFFFFFFFF, eos_handle_rom, 0 },
    { "ROM1",         0xF0000000, 0xF7FFFFFF, eos_handle_rom, 1 },
    { "Interrupt",    0xC0201000, 0xC0201FFF, eos_handle_intengine, 0 },
    { "Timers",       0xC0210000, 0xC0210FFF, eos_handle_timers, 0 },
    { "RTC??",        0xC0242014, 0xC0242014, eos_handle_unk, 0 },
    { "GPIO",         0xC0220000, 0xC022FFFF, eos_handle_gpio, 0 },
    { "Basic1",       0xC0400000, 0xC0400FFF, eos_handle_basic, 1 },
    { "Basic2",       0xC022F000, 0xC022FFFF, eos_handle_basic, 2 },
    { "SDIO",         0xC0C10000, 0xC0C10FFF, eos_handle_sdio, 0 },
    { "TIO",          0xC0800000, 0xC08000FF, eos_handle_tio, 0 },
    { "SIO0",         0xC0820000, 0xC08200FF, eos_handle_sio, 0 },
    { "SIO1",         0xC0820100, 0xC08201FF, eos_handle_sio, 1 },
    { "SIO2",         0xC0820200, 0xC08202FF, eos_handle_sio, 2 },
    { "SIO3",         0xC0820300, 0xC08203FF, eos_handle_sio, 3 },
    { "DMA1",         0xC0A10000, 0xC0A100FF, eos_handle_dma, 1 },
    { "DMA2",         0xC0A20000, 0xC0A200FF, eos_handle_dma, 2 },
    { "DMA3",         0xC0A30000, 0xC0A300FF, eos_handle_dma, 3 },
    { "DMA4",         0xC0A40000, 0xC0A400FF, eos_handle_dma, 4 },
    { "CARTRIDGE",    0xC0F24000, 0xC0F24FFF, eos_handle_cartridge, 0 },
    { "ASIF",         0xC0920000, 0xC0920FFF, eos_handle_asif, 4 },
    
    { "ML helpers",   0xCF123000, 0xCF123EFF, eos_handle_ml_helpers, 0 },
    { "FIO wrapper",  0xCF123F00, 0xCF123FFF, eos_handle_ml_fio, 0 },
};

/* io range access */
static uint64_t eos_io_read(void *opaque, hwaddr addr, uint32_t size)
{
    addr += IO_MEM_START;

    uint32_t type = MODE_READ;

    switch(size)
    {
        case 1:
            type |= WIDTH_BYTE;
            break;
        case 2:
            type |= WIDTH_HALF;
            break;
        case 4:
            type |= WIDTH_WORD;
            break;
    }

    return eos_handler ( opaque, addr, type, 0 );
}

static void eos_io_write(void *opaque, hwaddr addr, uint64_t val, uint32_t size)
{
    addr += IO_MEM_START;

    uint32_t type = MODE_WRITE;

    switch(size)
    {
        case 1:
            type |= WIDTH_BYTE;
            break;
        case 2:
            type |= WIDTH_HALF;
            break;
        case 4:
            type |= WIDTH_WORD;
            break;
    }

    eos_handler ( opaque, addr, type, val );
}

/* used to dump bmp vram */
static int R[] = {128, 234, 0, 0, 163, 31, 0, 1, 234, 0, 185, 27, 200, 0, 201, 209, 232, 216, 0, 231, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 231, 9, 18, 27, 36, 41, 46, 50, 55, 59, 64, 69, 73, 82, 92, 101, 109, 117, 119, 124, 129, 133, 138, 142, 147, 152, 156, 161, 165, 170, 175, 179, 184, 188, 193, 198, 202, 207, 211, 216, 221, 225, 229, 220, 206, 193, 177, 162, 160, 156, 150, 140, 132, 125, 119, 106, 92, 76, 62, 49, 36, 22, 108, 101, 95, 89, 81, 72, 64, 55, 49, 42, 39, 233, 199, 192, 147, 102, 57, 16, 8, 6, 5, 6, 6, 3, 6, 3, 6, 4, 4, 2, 2, 1, 51, 48, 41, 39, 35, 31, 26, 26, 23, 18, 16, 14, 11, 221, 206, 196, 185, 173, 172, 163, 155, 146, 140, 133, 121, 108, 93, 80, 72, 60, 44, 30, 113, 108, 102, 94, 87, 78, 67, 56, 46, 37, 28, 233, 230, 222, 211, 198, 188, 174, 174, 168, 164, 152, 141, 130, 121, 109, 93, 80, 71, 62, 46, 28, 115, 106, 97, 83, 73, 62, 52, 45, 37, 24, 12, 232, 231, 192, 149, 102, 53, 8, 0, 0, 23, 24, 24, 24, 90, 72, 0, 24, 0, 0, 1, 0, 26, 23, 21, 20, 18, 17, 13, 10, 7, 4, 1, 1, 1, 25, 29, 29, 33, 35, 38, 40, 42, 43, 43, 45, 45, 54, 65, 79, 90};
static int G[] = {128, 235, 0, 0, 56, 187, 153, 172, 0, 66, 186, 34, 0, 0, 0, 191, 0, 94, 62, 109, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 18, 27, 36, 41, 46, 50, 55, 59, 64, 69, 73, 82, 92, 101, 110, 117, 119, 124, 129, 133, 138, 142, 147, 152, 156, 161, 165, 170, 175, 178, 184, 188, 192, 198, 202, 206, 210, 216, 221, 225, 230, 193, 148, 101, 53, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 35, 34, 31, 29, 26, 25, 20, 17, 17, 13, 12, 1, 0, 212, 186, 164, 139, 117, 111, 107, 102, 101, 94, 87, 81, 74, 66, 60, 52, 40, 29, 20, 97, 90, 82, 74, 67, 60, 52, 47, 42, 36, 33, 220, 179, 214, 193, 172, 151, 128, 127, 119, 114, 108, 103, 99, 90, 79, 69, 59, 55, 45, 33, 24, 93, 90, 86, 80, 72, 64, 56, 47, 40, 32, 24, 230, 184, 206, 175, 141, 108, 78, 73, 71, 69, 64, 61, 55, 51, 46, 39, 34, 29, 26, 20, 11, 71, 64, 59, 50, 46, 39, 33, 28, 22, 15, 9, 140, 109, 210, 190, 165, 139, 118, 113, 110, 104, 25, 24, 24, 25, 67, 31, 24, 25, 37, 29, 23, 75, 70, 66, 60, 51, 46, 39, 31, 24, 14, 4, 222, 179, 29, 33, 35, 39, 43, 46, 48, 51, 52, 56, 57, 57, 67, 81, 95, 110};
static int B[] = {128, 235, 0, 0, 0, 216, 0, 1, 1, 211, 139, 126, 0, 168, 154, 0, 231, 76, 75, 0, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 232, 9, 18, 27, 36, 41, 46, 50, 55, 59, 64, 69, 73, 82, 92, 101, 109, 117, 119, 124, 129, 133, 138, 142, 147, 152, 156, 161, 165, 170, 174, 178, 184, 188, 193, 198, 202, 207, 211, 216, 221, 224, 229, 195, 150, 102, 54, 10, 0, 1, 2, 1, 0, 1, 1, 0, 0, 2, 1, 1, 0, 2, 39, 34, 32, 29, 27, 25, 22, 18, 18, 15, 14, 1, 0, 227, 220, 212, 208, 202, 201, 189, 186, 181, 167, 155, 145, 132, 120, 107, 94, 74, 54, 38, 136, 126, 115, 107, 95, 83, 74, 64, 57, 52, 46, 232, 232, 193, 147, 100, 52, 6, 2, 0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 0, 1, 2, 37, 37, 35, 33, 31, 27, 23, 20, 18, 12, 10, 0, 0, 193, 148, 100, 53, 7, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 40, 35, 33, 27, 27, 23, 18, 15, 14, 10, 5, 0, 0, 192, 149, 102, 53, 8, 1, 0, 1, 151, 152, 151, 150, 85, 92, 152, 152, 1, 0, 0, 27, 24, 23, 21, 18, 16, 12, 11, 9, 7, 3, 1, 0, 38, 44, 46, 51, 56, 59, 64, 66, 69, 72, 73, 73, 86, 106, 126, 143};

static void eos_dump_vram(uint32_t address)
{
    static int screenshot_count = 0;
    char fn[100];
    snprintf(fn, sizeof(fn), "vram%02d.txt", screenshot_count);
    screenshot_count++;
    
    FILE* f = fopen(fn, "w");
    fprintf(f, "# ImageMagick pixel enumeration: 720,480,255,rgb\n");

    int i,j;
    for (i = 0; i < 480; i++)
    {
        for (j = 0; j < 720; j++)
        {
            uint8_t* b = qemu_get_ram_ptr(address + TCM_SIZE);
            int c = b[BM(j,i)];
            fprintf(f, "%d,%d: (%d,%d,%d)\n", j, i, R[c], G[c], B[c]);
        }
    }
    fclose(f);
}

static void reverse_bytes_order(char* buf, int count)
{
    short* buf16 = (short*) buf;
    int i;
    for (i = 0; i < count/2; i++)
    {
        short x = buf16[i];
        buf[2*i+1] = x;
        buf[2*i] = x >> 8;
    }
}

unsigned int eos_handle_ml_helpers ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    if(type & MODE_WRITE)
    {
        switch (address)
        {
            case REG_PRINT_CHAR:
                printf("%c", (uint8_t)value);
                return 0;

            case REG_PRINT_NUM:
                printf("%x (%d)\n", (uint32_t)value, (uint32_t)value);
                return 0;

            case REG_DUMP_VRAM:
                eos_dump_vram(value ? value : ws->bmp_vram);
                return 0;

            case REG_SHUTDOWN:
                printf("Goodbye!\n");
                qemu_system_shutdown_request();
                return 0;
            
            case REG_BMP_VRAM:
                ws->bmp_vram = (uint32_t) value;
                return 0;

            case REG_IMG_VRAM:
                ws->img_vram = (uint32_t) value;
                if (value)
                {
                    eos_load_image(ws, "LV-000.422", 0, -1, value, 0);
                }
                else
                {
                    printf("Image buffer disabled\n");
                }
                return 0;
            
            case REG_RAW_BUFF:
                ws->raw_buff = (uint32_t) value;
                if (value)
                {
                    /* fixme: hardcoded strip offset */
                    eos_load_image(ws, "RAW-000.DNG", 33792, -1, value, 1);
                }
                else
                {
                    printf("Raw buffer disabled\n");
                }
                return 0;

            case REG_DISP_TYPE:
                ws->display_type = (uint32_t) value;
                return 0;
        }
    }
    else
    {
        switch (address)
        {
            case REG_GET_KEY:
            {
                if (ws->key_index_r == ws->key_index_w)
                {
                    /* no key in queue */
                    return 0;
                }
                else
                {
                    /* return a key from the circular buffer */
                    return ws->keybuf[(ws->key_index_r++) & 15];
                }
            }

            case REG_BMP_VRAM:
                return ws->bmp_vram;

            case REG_IMG_VRAM:
                return ws->img_vram;
            
            case REG_RAW_BUFF:
                return ws->raw_buff;
            
            case REG_DISP_TYPE:
                return ws->display_type;
        }
        return 0;
    }
    return 0;
}

static char* ml_fio_translate_filename(char* filename)
{
    static char buf[1000];
    char* f;
    
    for (f = filename; *f; f++)
    {
        if (*f == '.' && *(f+1) == '.')
        {
            printf("Hacker detected :) %s\n", filename);
            goto err;
        }
    }
    
    if (filename[0] == 'A' && filename[1] == ':' && filename[2] == '/')
    {
        snprintf(buf, sizeof(buf), "cfcard/%s", filename+3);
        goto ok;
    }
    
    if (filename[0] == 'B' && filename[1] == ':' && filename[2] == '/')
    {
        snprintf(buf, sizeof(buf), "sdcard/%s", filename+3);
        goto ok;
    }

err:
    printf("[FIO wrapper] invalid filename (%s)\n", filename);
    buf[0] = 0;
    return buf;

ok:
    /* files copied from a FAT32 card are all uppercase */
    for (f = buf+6; *f; f++)
    {
        *f = toupper(*f);
    }
    return buf;
}

unsigned int eos_handle_ml_fio ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    static int arg0, arg1, arg2, arg3;
    static char buffer[1000] = {0};
    static uint32_t  buffer_index = 0;
    
    static char dirname[1000];
    static DIR * dir;
    static FILE* file;
    
    if(type & MODE_WRITE)
    {
        switch (address)
        {
            case REG_FIO_NUMERIC_ARG0:
                arg0 = value;
                return 0;

            case REG_FIO_NUMERIC_ARG1:
                arg1 = value;
                return 0;

            case REG_FIO_NUMERIC_ARG2:
                arg2 = value;
                return 0;

            case REG_FIO_NUMERIC_ARG3:
                arg3 = value;
                return 0;
            
            case REG_FIO_BUFFER:
                if (buffer_index < COUNT(buffer))
                {
                    buffer[buffer_index++] = value;
                }
                return 0;
            
            case REG_FIO_BUFFER_SEEK:
                buffer_index = value;
                return 0;
        }
    }
    else
    {
        switch (address)
        {
            case REG_FIO_NUMERIC_ARG0:
                return arg0;

            case REG_FIO_NUMERIC_ARG1:
                return arg1;

            case REG_FIO_NUMERIC_ARG2:
                return arg2;

            case REG_FIO_NUMERIC_ARG3:
                return arg3;

            case REG_FIO_BUFFER:
                if (buffer_index < COUNT(buffer))
                {
                    return buffer[buffer_index++];
                }

            case REG_FIO_GET_FILE_SIZE:
            {
                struct stat sb;
                char* filename = ml_fio_translate_filename(buffer);
                if (stat(filename, &sb) != -1)
                {
                    printf("[FIO wrapper] GetFileSize(%s) => %d\n", filename, (int)sb.st_size);
                    return sb.st_size;
                }
                else
                {
                    printf("[FIO wrapper] GetFileSize(%s) error\n", filename);
                    return -1;
                }
            }

            case REG_FIO_OPENFILE:
            {
                char* filename = ml_fio_translate_filename(buffer);

                if (file == 0)
                {
                    file = fopen(filename, "rb");
                    if (file)
                    {
                        printf("[FIO wrapper] openfile(%s)\n", filename);
                        return 1;
                    }
                    else
                    {
                        printf("[FIO wrapper] openfile(%s) error\n", filename);
                    }
                }
                else
                {
                    printf("[FIO wrapper] openfile(%s) already in use\n", filename);
                }
                return 0;
            }
            
            case REG_FIO_CLOSEFILE:
            {
                if (file)
                {
                    fclose(file);
                    file = 0;
                    printf("[FIO wrapper] closefile()\n");
                    return 1;
                }
                else
                {
                    printf("[FIO wrapper] closefile() nothing open\n");
                }
                return 0;
            }

            case REG_FIO_READFILE:
            {
                int size = MIN(arg0, sizeof(buffer));
                int pos = arg1;
                buffer[0] = buffer_index = 0;
                
                if (file)
                {
                    //~ printf("[FIO wrapper] reading %d bytes from pos=%d\n", size, pos);
                    fseek(file, pos, pos >= 0 ? SEEK_SET : SEEK_END);
                    return fread(buffer, 1, size, file);
                }
                else
                {
                    printf("[FIO wrapper] readfile() nothing open\n");
                }
                return 0;
            }
            
            case REG_FIO_OPENDIR:
            {
                char* path = ml_fio_translate_filename(buffer);

                if (dir == 0)
                {
                    dir = opendir(path);
                    if (dir)
                    {
                        int len = snprintf(dirname, sizeof(dirname), "%s", path);
                        if (dirname[len-1] == '/') dirname[len-1] = 0;
                        printf("[FIO wrapper] opendir(%s)\n", path);
                        return 1;
                    }
                    else
                    {
                        printf("[FIO wrapper] opendir(%s) error\n", path);
                    }
                }
                else
                {
                    printf("[FIO wrapper] opendir(%s) already in use\n", path);
                }
                return 0;
            }
            
            case REG_FIO_CLOSEDIR:
            {
                if (dir)
                {
                    closedir(dir);
                    dir = 0;
                    printf("[FIO wrapper] closedir()\n");
                    return 1;
                }
                else
                {
                    printf("[FIO wrapper] closedir() nothing open\n");
                }
                return 0;
            }

            case REG_FIO_READDIR:
            {
                buffer[0] = buffer_index = 0;
                arg0 = arg1 = arg2 = 0;
                
                if (dir)
                {
                    struct dirent * d = readdir(dir);
                    if (d)
                    {
                        snprintf(buffer, sizeof(buffer), "%s", d->d_name);

                        char fullpath[1000];
                        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirname, d->d_name);
                        
                        struct stat sb;
                        if (stat(fullpath, &sb) != -1)
                        {
                            arg0 = sb.st_size;
                            arg1 = S_ISDIR(sb.st_mode) ? 0x10 : 0;
                            arg2 = sb.st_mtime;
                        }
                        
                        printf("[FIO wrapper] readdir() => %s size=%d mode=%x time=%x\n", buffer, arg0, arg1, arg2);
                        return 1;
                    }
                }
                else
                {
                    printf("[FIO wrapper] closedir() nothing open\n");
                }
                return 0;
            }
        }
    }
    return 0;
}

static void eos_load_image(EOSState *s, const char* file, int offset, int max_size, uint32_t addr, int swap_endian)
{
    int size = get_image_size(file);
    if (size < 0)
    {
        fprintf(stderr, "%s: file not found '%s'\n", __func__, file);
        abort();
    }

    if (size < offset) {
        fprintf(stderr, "%s: file '%s': offset '%d' is too big\n", __func__,
            file, offset);
        abort();
    }

    fprintf(stderr, "[EOS] loading '%s' to 0x%08X-0x%08X\n", file, addr, size + addr - 1);

    uint8_t* buf = malloc(size);
    if (!buf)
    {
        fprintf(stderr, "%s: malloc error loading '%s'\n", __func__, file);
        abort();
    }

    if (load_image(file, buf) != size)
    {
        fprintf(stderr, "%s: error loading '%s'\n", __func__, file);
        abort();
    }

    size = size - offset;

    if ((max_size > 0) && (size > max_size)) {
        size = max_size;
    }
    
    if (swap_endian) {
        reverse_bytes_order(buf + offset, size);
    }

    cpu_physical_memory_write_rom(addr, buf + offset, size);

    free(buf);
}

static const MemoryRegionOps iomem_ops = {
    .read = eos_io_read,
    .write = eos_io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void *eos_interrupt_thread(void *parm)
{
    EOSState *s = (EOSState *)parm;

    while (1)
    {
        uint32_t pos;

        usleep(100);

        /* go through all interrupts and check if they are pending/scheduled */
        for(pos = 0; pos < INT_ENTRIES; pos++)
        {
            /* it is pending, so trigger int and set to 0 */
            if(s->irq_schedule[pos] == 1)
            {
                /* wait, its not enabled. keep it pending */
                if(s->irq_enabled[pos] && !s->irq_id)
                {
                    /* timer interrupt will re-fire periodically */
                    if(pos == 0x0A)
                    {
                        s->irq_schedule[pos] = 100;
                    }
                    else
                    {
                        printf("[EOS] trigger int 0x%02X (delayed)\n", pos);
                        s->irq_schedule[pos] = 0;
                    }

                    s->irq_id = pos;
                    cpu_interrupt(CPU(s->cpu), CPU_INTERRUPT_HARD);
                }
            }

            /* still counting down? */
            if(s->irq_schedule[pos] > 1)
            {
                s->irq_schedule[pos]--;
            }
        }
    }

    return NULL;
}

// precompute some parts of YUV to RGB computations
static int yuv2rgb_RV[256];
static int yuv2rgb_GU[256];
static int yuv2rgb_GV[256];
static int yuv2rgb_BU[256];

/** http://www.martinreddy.net/gfx/faqs/colorconv.faq
 * BT 601:
 * R'= Y' + 0.000*U' + 1.403*V'
 * G'= Y' - 0.344*U' - 0.714*V'
 * B'= Y' + 1.773*U' + 0.000*V'
 * 
 * BT 709:
 * R'= Y' + 0.0000*Cb + 1.5701*Cr
 * G'= Y' - 0.1870*Cb - 0.4664*Cr
 * B'= Y' - 1.8556*Cb + 0.0000*Cr
 */

static void precompute_yuv2rgb(int rec709)
{
    int u, v;
    if (rec709)
    {
        /*
        *R = *Y + 1608 * V / 1024;
        *G = *Y -  191 * U / 1024 - 478 * V / 1024;
        *B = *Y + 1900 * U / 1024;
        */
        for (u = 0; u < 256; u++)
        {
            int8_t U = u;
            yuv2rgb_GU[u] = (-191 * U) >> 10;
            yuv2rgb_BU[u] = (1900 * U) >> 10;
        }

        for (v = 0; v < 256; v++)
        {
            int8_t V = v;
            yuv2rgb_RV[v] = (1608 * V) >> 10;
            yuv2rgb_GV[v] = (-478 * V) >> 10;
        }
    }
    else // REC 601
    {
        /*
        *R = *Y + ((1437 * V) >> 10);
        *G = *Y -  ((352 * U) >> 10) - ((731 * V) >> 10);
        *B = *Y + ((1812 * U) >> 10);
        */
        for (u = 0; u < 256; u++)
        {
            int8_t U = u;
            yuv2rgb_GU[u] = (-352 * U) >> 10;
            yuv2rgb_BU[u] = (1812 * U) >> 10;
        }

        for (v = 0; v < 256; v++)
        {
            int8_t V = v;
            yuv2rgb_RV[v] = (1437 * V) >> 10;
            yuv2rgb_GV[v] = (-731 * V) >> 10;
        }
    }
}

#define COERCE(x,lo,hi) MAX(MIN((x),(hi)),(lo))

static void yuv2rgb(int Y, int U, int V, int* R, int* G, int* B)
{
    const int v_and_ff = V & 0xFF;
    const int u_and_ff = U & 0xFF;
    int v = Y + yuv2rgb_RV[v_and_ff];
    *R = COERCE(v, 0, 255);
    v = Y + yuv2rgb_GU[u_and_ff] + yuv2rgb_GV[v_and_ff];
    *G = COERCE(v, 0, 255);
    v = Y + yuv2rgb_BU[u_and_ff];
    *B = COERCE(v, 0, 255);
}

#define UYVY_GET_Y1(uyvy) (((uyvy) >>  8) & 0xFF)
#define UYVY_GET_Y2(uyvy) (((uyvy) >> 24) & 0xFF)
#define UYVY_GET_U(uyvy)  (((uyvy)      ) & 0xFF)
#define UYVY_GET_V(uyvy)  (((uyvy) >> 16) & 0xFF)


/* todo: supoort other bith depths */

typedef void (*drawfn_bmp_yuv)(void *, uint8_t *, const uint8_t *, const uint8_t*, int, int, int);

static void draw_line8_32(void *opaque,
                uint8_t *d, const uint8_t *s, int width, int deststep)
{
    uint8_t v, r, g, b;
    
    do {
        v = ldub_raw((void *) s);
        if (v)
        {
            r = R[v];
            g = G[v];
            b = B[v];
            ((uint32_t *) d)[0] = rgb_to_pixel32(r, g, b);
        }
        else
        {
            r = g = b = rand();
            ((uint32_t *) d)[0] = rgb_to_pixel32(r, g, b);
        }
        s ++;
        d += 4;
    } while (-- width != 0);
}

static void draw_line8_32_bmp_yuv(void *opaque,
                uint8_t *d, const uint8_t *bmp, const uint8_t *yuv, int width, int deststep, int yuvstep)
{
    uint8_t v, r, g, b;
    
    do {
        v = ldub_raw((void *) bmp);
        if (v)
        {
            r = R[v];
            g = G[v];
            b = B[v];
            ((uint32_t *) d)[0] = rgb_to_pixel32(r, g, b);
        }
        else
        {
            uint32_t uyvy =  ldl_raw((uintptr_t)yuv & ~3);
            int Y = (uintptr_t)yuv & 3 ? UYVY_GET_Y2(uyvy) : UYVY_GET_Y1(uyvy);
            int U = UYVY_GET_U(uyvy);
            int V = UYVY_GET_V(uyvy);
            int R, G, B;
            yuv2rgb(Y, U, V, &R, &G, &B);
            ((uint32_t *) d)[0] = rgb_to_pixel32(R, G, B);
        }
        bmp ++;
        yuv += yuvstep;
        d += 4;
    } while (-- width != 0);
}

/* similar to QEMU's framebuffer_update_display, but with two image planes */
/* main plane is BMP (8-bit, same size as output), secondary plane is YUV (scaled to match the BMP one) */
static void framebuffer_update_display_bmp_yuv(
    DisplaySurface *ds,
    MemoryRegion *address_space,
    hwaddr base_bmp,
    hwaddr base_yuv,
    int cols, /* Width in pixels.  */
    int rows_bmp, /* Height in pixels.  */
    int rows_yuv,
    int src_width_bmp, /* Length of source line, in bytes.  */
    int src_width_yuv,
    int dest_row_pitch, /* Bytes between adjacent horizontal output pixels.  */
    int dest_col_pitch, /* Bytes between adjacent vertical output pixels.  */
    int invalidate, /* nonzero to redraw the whole image.  */
    drawfn_bmp_yuv fn,
    void *opaque,
    int *first_row, /* Input and output.  */
    int *last_row /* Output only */)
{
    hwaddr src_len_bmp;
    hwaddr src_len_yuv;
    uint8_t *dest;
    uint8_t *src_bmp;
    uint8_t *src_yuv;
    uint8_t *src_base_bmp;
    uint8_t *src_base_yuv;
    int first, last = 0;
    int dirty;
    int i;
    ram_addr_t addr_bmp;
    ram_addr_t addr_yuv;
    ram_addr_t addr_base_yuv;
    MemoryRegionSection mem_section_bmp;
    MemoryRegionSection mem_section_yuv;
    MemoryRegion *mem_bmp;
    MemoryRegion *mem_yuv;

    i = *first_row;
    *first_row = -1;
    src_len_bmp = src_width_bmp * rows_bmp;
    src_len_yuv = src_width_yuv * rows_yuv;

    mem_section_bmp = memory_region_find(address_space, base_bmp, src_len_bmp);
    mem_section_yuv = memory_region_find(address_space, base_yuv, src_len_yuv);
    mem_bmp = mem_section_bmp.mr;
    mem_yuv = mem_section_yuv.mr;
    if (int128_get64(mem_section_bmp.size) != src_len_bmp ||
            !memory_region_is_ram(mem_section_bmp.mr)) {
        goto out;
    }
    assert(mem_bmp);
    assert(mem_section_bmp.offset_within_address_space == base_bmp);

    if (int128_get64(mem_section_yuv.size) != src_len_yuv ||
            !memory_region_is_ram(mem_section_yuv.mr)) {
        goto out;
    }
    assert(mem_yuv);
    assert(mem_section_yuv.offset_within_address_space == base_yuv);

    memory_region_sync_dirty_bitmap(mem_bmp);
    memory_region_sync_dirty_bitmap(mem_yuv);
    src_base_bmp = cpu_physical_memory_map(base_bmp, &src_len_bmp, 0);
    src_base_yuv = cpu_physical_memory_map(base_yuv, &src_len_yuv, 0);
    /* If we can't map the framebuffer then bail.  We could try harder,
       but it's not really worth it as dirty flag tracking will probably
       already have failed above.  */
    if (!src_base_bmp)
        goto out;
    if (!src_base_yuv)
        goto out;
    if (src_len_bmp != src_width_bmp * rows_bmp) {
        cpu_physical_memory_unmap(src_base_bmp, src_len_bmp, 0, 0);
        goto out;
    }
    if (src_len_yuv != src_width_yuv * rows_yuv) {
        cpu_physical_memory_unmap(src_base_yuv, src_len_yuv, 0, 0);
        goto out;
    }
    src_bmp = src_base_bmp;
    src_yuv = src_base_yuv;
    dest = surface_data(ds);
    if (dest_col_pitch < 0)
        dest -= dest_col_pitch * (cols - 1);
    if (dest_row_pitch < 0) {
        dest -= dest_row_pitch * (rows_bmp - 1);
    }
    first = -1;
    addr_bmp = mem_section_bmp.offset_within_region;
    addr_yuv = mem_section_yuv.offset_within_region;
    addr_base_yuv = addr_yuv;

    int j = i * rows_yuv / rows_bmp;
    addr_bmp += i * src_width_bmp;
    src_bmp += i * src_width_bmp;
    addr_yuv = addr_base_yuv + j * src_width_yuv;
    src_yuv = src_base_yuv + j * src_width_yuv;
    dest += i * dest_row_pitch;
    
    /* fixme: only works for integer factors */
    int src_yuv_pitch = src_width_yuv / cols;

    for (; i < rows_bmp; i++) {
        dirty = memory_region_get_dirty(mem_bmp, addr_bmp, src_width_bmp,
                                             DIRTY_MEMORY_VGA);
        dirty |= memory_region_get_dirty(mem_yuv, addr_yuv, src_width_yuv,
                                             DIRTY_MEMORY_VGA);
        if (dirty || invalidate) {
            fn(opaque, dest, src_bmp, src_yuv, cols, dest_col_pitch, src_yuv_pitch);
            if (first == -1)
                first = i;
            last = i;
        }

        int j = i * rows_yuv / rows_bmp;
        addr_bmp += src_width_bmp;
        src_bmp += src_width_bmp;
        addr_yuv = addr_base_yuv + j * src_width_yuv;
        src_yuv = src_base_yuv + j * src_width_yuv;
        dest += dest_row_pitch;
    }
    cpu_physical_memory_unmap(src_base_bmp, src_len_bmp, 0, 0);
    cpu_physical_memory_unmap(src_base_yuv, src_len_yuv, 0, 0);
    if (first < 0) {
        goto out;
    }
    memory_region_reset_dirty(mem_bmp, mem_section_bmp.offset_within_region, src_len_bmp,
                              DIRTY_MEMORY_VGA);
    memory_region_reset_dirty(mem_yuv, mem_section_yuv.offset_within_region, src_len_yuv,
                              DIRTY_MEMORY_VGA);
    *first_row = first;
    *last_row = last;
out:
    memory_region_unref(mem_bmp);
    memory_region_unref(mem_yuv);
}

static void eos_update_display(void *parm)
{
    EOSState *s = (EOSState *)parm;

    DisplaySurface *surface = qemu_console_surface(s->con);
    
    /* these numbers need double-checking */
    /*                  LCD    HDMI-1080   HDMI-480    SD-PAL      SD-NTSC */
    int widths[]      = {   720,   960,        720,        720,        720     };
    int heights[]     = {   480,   540,        480,        576,        480     };
    int yuv_widths[]  = {   720,  1920,        720,        540,        540     };
    int yuv_heights[] = {   480,  1080,        480,        572,        480     };
    
    int width       = widths     [s->display_type];
    int height      = heights    [s->display_type];
    int yuv_width   = yuv_widths [s->display_type];
    int yuv_height  = yuv_heights[s->display_type];

    if (width != surface_width(surface) || height != surface_height(surface))
    {
        qemu_console_resize(s->con, width, height);
        surface = qemu_console_surface(s->con);
        s->display_invalidate = 1;
    }
    
    int first, last;
    
    first = 0;
    int linesize = surface_stride(surface);
    
    if (s->img_vram)
    {
        framebuffer_update_display_bmp_yuv(
            surface,
            s->system_mem,
            s->bmp_vram,
            s->img_vram,
            width, height, yuv_height,
            960, yuv_width*2, linesize, 0, s->display_invalidate,
            draw_line8_32_bmp_yuv, 0,
            &first, &last
        );
    }
    else
    {
        framebuffer_update_display(
            surface,
            s->system_mem,
            s->bmp_vram,
            width, height,
            960, linesize, 0, 1,
            draw_line8_32, 0,
            &first, &last
        );
    }
    
    if (first >= 0) {
        dpy_gfx_update(s->con, 0, first, width, last - first + 1);
    }
    
    s->display_invalidate = 0;
}

static void eos_invalidate_display(void *parm)
{
    EOSState *s = (EOSState *)parm;
    s->display_invalidate = 1;
}

static const GraphicHwOps eos_display_ops = {
    .invalidate  = eos_invalidate_display,
    .gfx_update  = eos_update_display,
};

static void eos_key_event(void *parm, int keycode)
{
    /* keys sent to guest machine */
    EOSState *s = (EOSState *)parm;
    s->keybuf[(s->key_index_w++) & 15] = keycode;
}

static EOSState *eos_init_cpu(void)
{
    EOSState *s = g_new(EOSState, 1);

    s->verbosity = 0xFFFFFFFF;
    s->tio_rxbyte = 0x100;

    memset(s->irq_enabled, 0x00, sizeof(s->irq_enabled));
    memset(s->irq_schedule, 0x00, sizeof(s->irq_schedule));
    s->system_mem = get_system_memory();

    memory_region_init_ram(&s->tcm_code, NULL, "eos.tcm_code", TCM_SIZE);
    memory_region_add_subregion(s->system_mem, 0x00000000, &s->tcm_code);
    memory_region_init_ram(&s->tcm_data, NULL, "eos.tcm_data", TCM_SIZE);
    memory_region_add_subregion(s->system_mem, CACHING_BIT, &s->tcm_data);

    /* set up RAM, cached and uncached */
    memory_region_init_ram(&s->ram, NULL, "eos.ram", RAM_SIZE - TCM_SIZE);
    memory_region_add_subregion(s->system_mem, TCM_SIZE, &s->ram);
    memory_region_init_alias(&s->ram_uncached, NULL, "eos.ram_uncached", &s->ram, 0x00000000, RAM_SIZE - TCM_SIZE);
    memory_region_add_subregion(s->system_mem, CACHING_BIT | TCM_SIZE, &s->ram_uncached);

    /* set up ROM0 */
    memory_region_init_ram(&s->rom0, NULL, "eos.rom0", ROM0_SIZE);
    memory_region_add_subregion(s->system_mem, ROM0_ADDR, &s->rom0);

    uint64_t offset;
    for(offset = ROM0_ADDR + ROM0_SIZE; offset < ROM1_ADDR; offset += ROM0_SIZE)
    {
        char name[32];
        MemoryRegion *image = g_new(MemoryRegion, 1);
        sprintf(name, "eos.rom0_mirror_%02X", (uint32_t)offset >> 24);

        memory_region_init_alias(image, NULL, name, &s->rom0, 0x00000000, ROM0_SIZE);
        memory_region_add_subregion(s->system_mem, offset, image);
    }

    /* set up ROM1 */
    memory_region_init_ram(&s->rom1, NULL, "eos.rom1", ROM1_SIZE);
    memory_region_add_subregion(s->system_mem, ROM1_ADDR, &s->rom1);

    // uint64_t offset;
    for(offset = ROM1_ADDR + ROM1_SIZE; offset < 0x100000000; offset += ROM1_SIZE)
    {
        char name[32];
        MemoryRegion *image = g_new(MemoryRegion, 1);
        sprintf(name, "eos.rom1_mirror_%02X", (uint32_t)offset >> 24);

        memory_region_init_alias(image, NULL, name, &s->rom1, 0x00000000, ROM1_SIZE);
        memory_region_add_subregion(s->system_mem, offset, image);
    }

    //memory_region_init_ram(&s->rom1, "eos.rom", 0x10000000);
    //memory_region_add_subregion(s->system_mem, 0xF0000000, &s->rom1);

    /* set up io space */
    memory_region_init_io(&s->iomem, NULL, &iomem_ops, s, "eos.iomem", IO_MEM_LEN);
    memory_region_add_subregion(s->system_mem, IO_MEM_START, &s->iomem);

    /*ROMState *rom0 = eos_rom_register(0xF8000000, NULL, "ROM1", ROM1_SIZE,
                                NULL,
                                0x100, 0x100, 32,
                                0, 0, 0, 0, 0);

                                */

    vmstate_register_ram_global(&s->ram);

    s->cpu = cpu_arm_init("arm946eos");
    if (!s->cpu)
    {
        fprintf(stderr, "Unable to find CPU definition\n");
        exit(1);
    }

    s->rtc.transfer_format = 0xFF;

    qemu_thread_create(&s->interrupt_thread_id, eos_interrupt_thread, s, QEMU_THREAD_JOINABLE);

    s->con = graphic_console_init(NULL, &eos_display_ops, s);
    
    qemu_add_kbd_event_handler(eos_key_event, s);

    return s;
}

static void eos_init_common(const char *rom_filename, uint32_t rom_start)
{
    EOSState *s = eos_init_cpu();

    /* populate ROM0 */
    eos_load_image(s, rom_filename, 0, ROM0_SIZE, ROM0_ADDR, 0);
    /* populate ROM1 */
    eos_load_image(s, rom_filename, ROM0_SIZE, ROM1_SIZE, ROM1_ADDR, 0);

    s->cpu->env.regs[15] = rom_start;
}

static void ml_init_common(const char *rom_filename, uint32_t rom_start)
{
    EOSState *s = eos_init_cpu();

    /* populate ROM0 */
    eos_load_image(s, rom_filename, 0, ROM0_SIZE, ROM0_ADDR, 0);
    /* populate ROM1 */
    eos_load_image(s, rom_filename, ROM0_SIZE, ROM1_SIZE, ROM1_ADDR, 0);

    eos_load_image(s, "autoexec.bin", 0, -1, 0x00800000, 0);

    /* we will replace Canon stubs with our own implementations */
    /* see qemu-helper.c, stub_mappings[] */
    eos_load_image(s, "qemu-helper.bin", 0, -1, Q_HELPER_ADDR, 0);
    uint32_t magic  = 0x12345678;
    uint32_t addr   = Q_HELPER_ADDR;
    while (eos_get_mem_w(s, addr) != magic || eos_get_mem_w(s, addr + 4) != magic)
    {
        addr += 4;
        if (addr > 0x30100000) { fprintf(stderr, "stub list not found\n"); abort(); }
    }
    uint32_t ram_offset = eos_get_mem_w(s, addr + 8);
    addr += 12;
    while (eos_get_mem_w(s, addr) != magic || eos_get_mem_w(s, addr + 4) != magic)
    {
        uint32_t old = eos_get_mem_w(s, addr);
        uint32_t new = eos_get_mem_w(s, addr + 4);
        if (old < 0xFF000000)
            old += ram_offset;
        uint32_t jmp[] = {FAR_CALL_INSTR, new};
        printf("[QEMU_HELPER] stub %x -> %x (%x)\n", old, new, eos_get_mem_w(s, old));
        cpu_physical_memory_write_rom(old, (uint8_t*)jmp, 8);

        addr += 8;
        if (addr > 0x30100000) { fprintf(stderr, "stub list error\n"); abort(); }
    }

    // set entry point
    s->cpu->env.regs[15] = 0x800000;
    s->cpu->env.regs[13] = 0x8000000;
    
    precompute_yuv2rgb(1);
}

ML_MACHINE(50D,   0xFF010000);
ML_MACHINE(60D,   0xFF010000);
ML_MACHINE(600D,  0xFF010000);
ML_MACHINE(500D,  0xFF010000);
ML_MACHINE(5D2,   0xFF810000);
ML_MACHINE(5D3,   0xFF0C0000);
ML_MACHINE(650D,  0xFF0C0000);
ML_MACHINE(100D,  0xFF0C0000);
ML_MACHINE(7D,    0xFF010000);
ML_MACHINE(550D,  0xFF010000);
ML_MACHINE(6D,    0xFF0C0000);

EOS_MACHINE(50D,  0xFF010000);
EOS_MACHINE(60D,  0xFF010000);
EOS_MACHINE(600D, 0xFF010000);
EOS_MACHINE(500D, 0xFF010000);
EOS_MACHINE(5D2,  0xFF810000);
EOS_MACHINE(5D3,  0xFF0C0000);
EOS_MACHINE(650D, 0xFF0C0000);
EOS_MACHINE(100D, 0xFF0C0000);
EOS_MACHINE(7D,   0xFF010000);
EOS_MACHINE(550D, 0xFF010000);
EOS_MACHINE(6D,   0xFF0C0000);

static void eos_machine_init(void)
{
    qemu_register_machine(&canon_eos_machine_ml_50D);
    qemu_register_machine(&canon_eos_machine_ml_60D);
    qemu_register_machine(&canon_eos_machine_ml_600D);
    qemu_register_machine(&canon_eos_machine_ml_500D);
    qemu_register_machine(&canon_eos_machine_ml_5D2);
    qemu_register_machine(&canon_eos_machine_ml_5D3);
    qemu_register_machine(&canon_eos_machine_ml_650D);
    qemu_register_machine(&canon_eos_machine_ml_100D);
    qemu_register_machine(&canon_eos_machine_ml_7D);
    qemu_register_machine(&canon_eos_machine_ml_550D);
	qemu_register_machine(&canon_eos_machine_ml_6D);
    qemu_register_machine(&canon_eos_machine_50D);
    qemu_register_machine(&canon_eos_machine_60D);
    qemu_register_machine(&canon_eos_machine_600D);
    qemu_register_machine(&canon_eos_machine_500D);
    qemu_register_machine(&canon_eos_machine_5D2);
    qemu_register_machine(&canon_eos_machine_5D3);
    qemu_register_machine(&canon_eos_machine_650D);
    qemu_register_machine(&canon_eos_machine_100D);
    qemu_register_machine(&canon_eos_machine_7D);
    qemu_register_machine(&canon_eos_machine_550D);
	qemu_register_machine(&canon_eos_machine_6D);
}

machine_init(eos_machine_init);

void eos_set_mem_w ( EOSState *ws, uint32_t addr, uint32_t val )
{
    cpu_physical_memory_write(addr, &val, sizeof(val));
}

void eos_set_mem_h ( EOSState *ws, uint32_t addr, uint16_t val )
{
    cpu_physical_memory_write(addr, &val, sizeof(val));
}

void eos_set_mem_b ( EOSState *ws, uint32_t addr, uint8_t val )
{
    cpu_physical_memory_write(addr, &val, sizeof(val));
}

uint32_t eos_get_mem_w ( EOSState *ws, uint32_t addr )
{
    uint32_t buf;

    cpu_physical_memory_read(addr, &buf, sizeof(buf));

    return buf;
}

uint16_t eos_get_mem_h ( EOSState *ws, uint32_t addr )
{
    uint16_t buf;

    cpu_physical_memory_read(addr, &buf, sizeof(buf));

    return buf;
}

uint8_t eos_get_mem_b ( EOSState *ws, uint32_t addr )
{
    uint8_t buf;

    cpu_physical_memory_read(addr, &buf, sizeof(buf));

    return buf;
}

unsigned int eos_default_handle ( EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int data = 0;
    unsigned int pc = ws->cpu->env.regs[15];

    switch ( type & WIDTH_MASK )
    {
        case WIDTH_WORD:
            if ( type & MODE_WRITE )
                eos_set_mem_w ( ws, address, value );
            else
                data = eos_get_mem_w ( ws, address );
            break;

        case WIDTH_HALF:
            if ( type & MODE_WRITE )
                eos_set_mem_h ( ws, address, value );
            else
                data = eos_get_mem_h ( ws, address );
            break;

        case WIDTH_BYTE:
            if ( type & MODE_WRITE )
                eos_set_mem_b ( ws, address, value );
            else
                data = eos_get_mem_b ( ws, address );
            break;
    }

    /* do not log ram/flash access */
    if(((address & 0xF0000000) == 0) || ((address & 0xF0000000) == 0xF0000000) || ((address & 0xF0000000) == 0x40000000))
    {
        return data;
    }

    if ( type & MODE_WRITE )
    {
        if(ws->verbosity & 1)
        {
            printf("Write: at [0x%08X] [0x%08X] -> [0x%08X]\r\n", pc, value, address);
        }
    }
    else
    {
        static int mod = 0;
        mod++;
        mod %= 2;

        if(mod)
        {
            data = ~data;
        }
        if(ws->verbosity & 1)
        {
            printf("Read: at [0x%08X] [0x%08X] <- [0x%08X]\r\n", pc, data, address);
        }
    }
    return data;
}

EOSRegionHandler *eos_find_handler( unsigned int address)
{
    int pos = 0;
    for(pos = 0; pos < sizeof(eos_handlers) / sizeof(eos_handlers[0]); pos++)
    {
        if(eos_handlers[pos].start <= address && eos_handlers[pos].end >= address)
        {
            return &eos_handlers[pos];
        }
    }

    return NULL;
}

unsigned int eos_handler ( EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    EOSRegionHandler *handler = eos_find_handler(address);

    if(handler)
    {
        return handler->handle(handler->parm, ws, address, type, value);
    }
    else
    {
        static uint32_t last_addr = 0;
        static uint32_t repeats = 0;

        if(address != last_addr || repeats < 5)
        {
            if(address == last_addr)
            {
                repeats++;
            }
            else
            {
                last_addr = address;
                repeats = 0;
            }

            if(type & MODE_WRITE)
            {
                printf("[???] [0x%08X] -> [0x%08X] PC: 0x%08X\r\n", value, address, ws->cpu->env.regs[15]);
            }
            else
            {
                printf("[???] [0x%08X] <- [0x%08X] PC: 0x%08X\r\n", value, address, ws->cpu->env.regs[15]);
            }
        }
    }
    return 0;
}

unsigned int eos_trigger_int(EOSState *ws, unsigned int id, unsigned int delay)
{
    if(!delay && ws->irq_enabled[id] && !ws->irq_id)
    {
        printf("[EOS] trigger int 0x%02X\n", id);
        ws->irq_id = id;
        cpu_interrupt(CPU(ws->cpu), CPU_INTERRUPT_HARD);
    }
    else
    {
        printf("[EOS] trigger int 0x%02X (delayed!)\n", id);
        if(!ws->irq_enabled[id])
        {
            delay = 1;
        }
        ws->irq_schedule[id] = delay;
    }
    return 0;
}

unsigned int eos_handle_intengine ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int pc = ws->cpu->env.regs[15];

    switch(address & 0xFFF)
    {
        case 0x04:
            if(type & MODE_WRITE)
            {
                printf("[Int] Wrote int reason [0x%08X] -> [0x%08X] PC: [0x%08X]\r\n", address, value, pc);
                return 0;
            }
            else
            {
                if(ws->irq_id != 0x0A)
                {
                    printf("[Int] Requested int reason [0x%08X] <- [0x%08X] PC: [0x%08X]\r\n", ws->irq_id << 2, address, pc);
                }
                return ws->irq_id << 2;
            }
            break;

        case 0x10:
            if(type & MODE_WRITE)
            {
                if(!ws->irq_enabled[value] || (value != 0x0A && value != 0x2F && value != 0x74 && value != 0x75))
                {
                    printf("[Int] Enabled interrupt ID 0x%02X PC: [0x%08X]\r\n", value, pc);
                }

                cpu_reset_interrupt(CPU(ws->cpu), CPU_INTERRUPT_HARD);
                ws->irq_id = 0;
                ws->irq_enabled[value] = 1;
                return 0;
            }
            else
            {
                return 0;
            }
            break;
    }

    if(type & MODE_WRITE)
    {
        printf("[Int] Write to Int space [0x%08X] -> [0x%08X] PC: [0x%08X]\r\n", value, address, pc);
    }
    else
    {
        return 0;
    }
    return 0;
}

unsigned int eos_handle_timers_ ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int pc = ws->cpu->env.regs[15];

    if(type & MODE_WRITE)
    {
        printf("[Timer?] at [0x%08X] [0x%08X] -> [0x%08X]\r\n", pc, value, address);
    }
    else
    {
        return 0;
    }
    return 0;
}

unsigned int eos_handle_timers ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int ret = 0;

    switch(address & 0xFF)
    {
        case 0x00:
            if(type & MODE_WRITE)
            {
                if(value & 1)
                {
                    printf("[Timer] at [0x%08X] Starting triggering\r\n", ws->cpu->env.regs[15]);
                    eos_trigger_int(ws, 0x0A, 5000);
                }
            }
            else
            {
                ret = 0;
            }
            break;
    }

    if(type & MODE_WRITE)
    {
        unsigned int pc = ws->cpu->env.regs[15];
        printf("[Timer] at [0x%08X] [0x%08X] -> [0x%08X]\r\n", pc, value, address);
    }
    else
    {
        return ret;
    }
    return 0;
}

unsigned int eos_handle_gpio ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int ret = 1;

    switch(parm)
    {
        case 0:
            if((address & 0xFFF) == 0xB6C)
            {
                /* 5D3 expects this one to be 0x10 in bootloader */
                ret = 0x10;
            }
            if((address & 0xFFF) == 0x0DC)
            {
                /* abort situation for FROMUTIL on 600D */
                ret = 0;
            }
            if((address & 0xFFF) == 0x0B0)
            {
                /* FUNC SW OFF on 7D */
                ret = 0;
            }
            if((address & 0xFFF) == 0x024)
            {
                /* master woke up on 7D */
                ret = 0;
            }

            if((address & 0xFFF) == 0x070)
            {
                /* VIDEO on 600D */
                printf("[GPIO] VIDEO CONNECT read at [0x%08X]\r\n", ws->cpu->env.regs[15]);
                return 0;
            }
            if((address & 0xFFF) == 0x108)
            {
                /* ERASE SW OFF on 600D */
                printf("[GPIO] ERASE SW OFF read at [0x%08X]\r\n", ws->cpu->env.regs[15]);
                return 0;
            }
            if((address & 0xFFF) == 0x0E8)
            {
                /* MIC on 600D */
                printf("[GPIO] MIC CONNECT read at [0x%08X]\r\n", ws->cpu->env.regs[15]);
                return 1;
            }
            if((address & 0xFFF) == 0x034)
            {
                /* USB on 600D */
                printf("[GPIO] USB CONNECT read at [0x%08X]\r\n", ws->cpu->env.regs[15]);
                return 0;
            }
            if((address & 0xFFF) == 0x138)
            {
                /* HDMI on 600D */
                printf("[GPIO] HDMI CONNECT read at [0x%08X]\r\n", ws->cpu->env.regs[15]);
                return 0;
            }
            if((address & 0xFFF) == 0x014)
            {
                /* /VSW_ON on 600D */
                printf("[GPIO] /VSW_ON read at [0x%08X]\r\n", ws->cpu->env.regs[15]);
                return 0;
            }
            if((address & 0xFFF) == 0x128)
            {
                /* CS for RTC on 600D */
                if(type & MODE_WRITE)
                {
                    if((value & 0x06) == 0x06)
                    {
                        printf("[RTC] CS set at [0x%08X]\r\n", ws->cpu->env.regs[15]);
                        ws->rtc.transfer_format = 0xFF;
                    }
                    else
                    {
                        printf("[RTC] CS reset at [0x%08X]\r\n", ws->cpu->env.regs[15]);
                    }
                }
                ret = 0;
            }
            break;
        case 3:
            if((address & 0xFFF) == 0x01C)
            {
                /* 40D CF Detect -> set low, so there is no CF */
                ret = 0;
            }
            break;
    }

        unsigned int pc = ws->cpu->env.regs[15];
    if(type & MODE_WRITE)
    {
        printf("[GPIO] at [0x%08X] [0x%08X] -> [0x%08X]\r\n", pc, value, address);
    }
    else
    {
        printf("[GPIO] at [0x%08X] [0x%08X] <- [0x%08X]\r\n", pc, ret, address);
        return ret;
    }
    return 0;
}

unsigned int eos_handle_ram ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    if(type & MODE_WRITE)
    {
        printf("[RAM] [0x%08X] -> [0x%08X]\r\n", value, address);
    }
    else
    {
        printf("[RAM] [0x%08X] <- [0x%08X]\r\n", value, address);
    }
    return eos_default_handle ( ws, address, type, value );
}

unsigned int eos_handle_cartridge ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    if(type & MODE_WRITE)
    {
        printf("[Cartridge] [0x%08X] -> [0x%08X]\r\n", value, address);
    }
    else
    {
        return 0;
    }
    return 0;
}

unsigned int eos_handle_dma ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int ret = 0;
    unsigned int log = 1;
    static unsigned int srcAddr = 0;
    static unsigned int dstAddr = 0;
    static unsigned int count = 0;
    unsigned int interruptId[] = { 0x00, 0x2f, 0x74, 0x75, 0x76 };

    switch(address & 0xFF)
    {
        case 0x08:
            if(type & MODE_WRITE)
            {
                if(value & 1)
                {
                    /* Start DMA */
                    printf("[DMA%i] Copy [0x%08X] -> [0x%08X], length [0x%08X], flags [0x%08X]\r\n", parm, srcAddr, dstAddr, count, value);

                    uint32_t blocksize = 8192;
                    uint8_t *buf = malloc(blocksize);
                    uint32_t remain = count;

                    while(remain)
                    {
                        uint32_t transfer = (remain > blocksize) ? blocksize : remain;

                        cpu_physical_memory_rw(srcAddr, buf, transfer, 0);
                        cpu_physical_memory_rw(dstAddr, buf, transfer, 1);

                        remain -= transfer;
                    }
                    free(buf);

                    printf("[DMA%i] OK\n", parm);

                    eos_trigger_int(ws, interruptId[parm], 0);
                    log = 0;
                }
            }
            else
            {
                ret = 0;
            }
            break;

        case 0x18:
            if(type & MODE_WRITE)
            {
                srcAddr = value;
                log = 1;
            }
            else
            {
                return srcAddr;
            }
            break;

        case 0x1C:
            if(type & MODE_WRITE)
            {
                dstAddr = value;
                log = 1;
            }
            else
            {
                return dstAddr;
            }
            break;

        case 0x20:
            if(type & MODE_WRITE)
            {
                count = value;
                log = 1;
            }
            else
            {
                return count;
            }
            break;
    }

    if(log)
    {
        if(type & MODE_WRITE)
        {
            printf("[DMA%i] [0x%08X] -> [0x%08X]\r\n", parm, value, address);
        }
        else
        {
            printf("[DMA%i] [0x%08X] <- [0x%08X]\r\n", parm, ret, address);
        }
    }

    return 0;
}


unsigned int eos_handle_tio ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    switch(address & 0xFF)
    {
        case 0x00:
            if(type & MODE_WRITE)
            {
                if((value == 0x08 || value == 0x0A || (value >= 0x20 && value <= 0x7F)))
                {
                    printf("%c", value);
                }
            }
            else
            {
                return 0;
            }
            break;

        case 0x04:
            printf("[TIO] Read byte: 0x%02X\r\n", ws->tio_rxbyte & 0xFF);
            return ws->tio_rxbyte & 0xFF;
            break;

        case 0x14:
            if(type & MODE_WRITE)
            {
                if(value & 1)
                {
                    printf("[TIO] Reset RX indicator\r\n");
                    ws->tio_rxbyte |= 0x100;
                }
            }
            else
            {
                if((ws->tio_rxbyte & 0x100) == 0)
                {
                    printf("[TIO] Signalling RX indicator\r\n");
                    return 3;
                }
                return 2;
            }
            break;
    }

    return 0;
}

unsigned int eos_handle_sio ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    static unsigned int last_sio_data = 0;
    static unsigned int last_sio_setup1 = 0;
    static unsigned int last_sio_setup2 = 0;
    static unsigned int last_sio_setup3 = 0;
    unsigned int pc = ws->cpu->env.regs[15];

    switch(address & 0xFF)
    {
        case 0x04:
            if((type & MODE_WRITE) && (value & 1))
            {
                printf("[SIO%i] Transmit: 0x%08X, setup 0x%08X 0x%08X 0x%08X PC: 0x%08X\r\n", parm, last_sio_data, last_sio_setup1, last_sio_setup2, last_sio_setup3, pc );

                switch(ws->rtc.transfer_format)
                {
                    /* no special mode */
                    case 0xFF:
                    {
                        uint8_t cmd = value & 0x0F;
                        uint8_t reg = (value>>4) & 0x0F;
                        ws->rtc.transfer_format = cmd;
                        ws->rtc.current_reg = reg;

                        switch(cmd)
                        {
                            /* burst writing */
                            case 0x00:
                            /* burst reading */
                            case 0x04:
                            /* 1 byte writing */
                            case 0x08:
                            /* 1 byte reading */
                            case 0x0C:
                                break;

                            default:
                                printf("[SIO%i] RTC: Requested invalid transfer mode 0x%02X at PC: 0x%08X\r\n", parm, value, pc );
                                break;
                        }
                    }

                    /* burst writing */
                    case 0x00:
                        ws->rtc.regs[ws->rtc.current_reg] = value;
                        ws->rtc.current_reg++;
                        ws->rtc.current_reg %= 0x10;
                        break;

                    /* burst reading */
                    case 0x04:
                        last_sio_data = ws->rtc.regs[ws->rtc.current_reg];
                        ws->rtc.current_reg++;
                        ws->rtc.current_reg %= 0x10;
                        break;

                    /* 1 byte writing */
                    case 0x08:
                        ws->rtc.regs[ws->rtc.current_reg] = value;
                        ws->rtc.transfer_format = 0xFF;
                        break;

                    /* 1 byte reading */
                    case 0x0C:
                        last_sio_data = ws->rtc.regs[ws->rtc.current_reg];
                        ws->rtc.transfer_format = 0xFF;
                        break;

                    default:
                        break;
                }
                return 0;
            }
            else
            {
                return 0;
            }
            break;

        case 0x0C:
            if(type & MODE_WRITE)
            {
                last_sio_setup1 = value;
                return 0;
            }
            else
            {
                return last_sio_setup1;
            }

        case 0x10:
            if(type & MODE_WRITE)
            {
                last_sio_setup3 = value;
                return 0;
            }
            else
            {
                return last_sio_setup3;
            }

        case 0x14:
            if(type & MODE_WRITE)
            {
                last_sio_setup1 = value;
                return 0;
            }
            else
            {
                return last_sio_setup1;
            }

        case 0x18:
            if(type & MODE_WRITE)
            {
                last_sio_data = value;

                printf("[SIO%i] Write to TX register PC: 0x%08X\r\n", parm, pc);
                return 0;
            }
            else
            {
                return last_sio_data;
            }

        case 0x1C:
            if(type & MODE_WRITE)
            {
                printf("[SIO%i] Write access to RX register\r\n", parm);
                return 0;
            }
            else
            {
                printf("[SIO%i] Read from RX register PC: 0x%08X  read: 0x%02X\r\n", parm, pc, last_sio_data);

                return last_sio_data;
            }
    }

    return 0;
}

unsigned int eos_handle_unk ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int pc = ws->cpu->env.regs[15];
    unsigned int ret = 0;
    static unsigned val = 0;

    switch(parm)
    {
        case 0:
            if(type & MODE_WRITE)
            {
                ret = 0;
            }
            else
            {
                ret = val;
                val += 1000;
            }
            break;
    }

    if(type & MODE_WRITE)
    {
        printf("[Timer] at [0x%08X] [0x%08X] -> [0x%08X]\r\n", pc, value, address);
    }
    else
    {
        //printf("[Timer] at [0x%08X] [0x%08X] <- [0x%08X]\r\n", pc, ret, address);
    }
    return ret;
}

unsigned int eos_handle_sdio ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int pc = ws->cpu->env.regs[15];
    unsigned int ret = 0;

    switch(address & 0xFFF)
    {
        case 0x10:
            /* code is waiting for bit0 getting high, bit1 is an error flag */
            ret = 3;
            break;
        case 0x34:
            /* code is waiting for transfer status */
            ret = 0xFFFFFF;
            break;
    }

    if(type & MODE_WRITE)
    {
        printf("[Basic] at [0x%08X] [0x%08X] -> [0x%08X]\r\n", pc, value, address);
    }
    else
    {
        printf("[Basic] at [0x%08X] [0x%08X] <- [0x%08X]\r\n", pc, ret, address);
    }
    return ret;
}

unsigned int eos_handle_basic ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int ret = 0;
    unsigned int pc = ws->cpu->env.regs[15];
    static int unk = 0;

    switch(parm)
    {
        case 1:
            switch(address & 0xFFF)
            {
                case 0xA4:
                    if(type & MODE_WRITE)
                    {
                    }
                    else
                    {
                        ret = 1;
                    }
                    break;
                    
                case 0x244:
                    if(type & MODE_WRITE)
                    {
                    }
                    else
                    {
                        /* idk, expected to be so in 5D3 123 */
                        ret = 1;
                    }
                    break;
                case 0x204:
                    if(type & MODE_WRITE)
                    {
                    }
                    else
                    {
                        /* idk, expected to be so in 5D3 bootloader */
                        ret = 2;
                    }
                    break;
            }
            break;

        case 2:
            switch(address & 0xFFF)
            {
                case 0x100:
                    if(type & MODE_WRITE)
                    {
                    }
                    else
                    {
                        ret = unk;
                        unk++;
                        unk %= 2;
                    }
                    break;

                case 0x198:
                    if(type & MODE_WRITE)
                    {
                    }
                    else
                    {
                        ret = unk;
                        unk++;
                        unk %= 2;
                    }
                    break;

                /*
                0xC022F480 [32]  Other VSW Status
                   0x40000 /VSW_OPEN Hi
                   0x80000 /VSW_REVO Hi
                */
                case 0x480:
                    if(type & MODE_WRITE)
                    {
                    }
                    else
                    {
                        ret = 0x40000 | 0x80000;
                        printf("[Basic] VSW_STATUS at [0x%08X] [0x%08X] <- [0x%08X]\r\n", pc, ret, address);
                        return ret;
                    }
                    break;

            }
            break;
    }
    if(type & MODE_WRITE)
    {
        printf("[Basic] at [0x%08X] [0x%08X] -> [0x%08X]\r\n", pc, value, address);
    }
    else
    {
        printf("[Basic] at [0x%08X] [0x%08X] <- [0x%08X]\r\n", pc, ret, address);
    }

    return ret;
}

unsigned int eos_handle_asif ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int ret = 0;

    if(type & MODE_WRITE)
    {
        printf("[ASIF] [0x%08X] -> [0x%08X]\r\n", value, address);
    }
    else
    {
        switch(address & 0xFFF)
        {
            case 0x110:
            case 0x114:
                /* audio meters; don't print messages, since it will flood the console */
                return 0;
        }
        printf("[ASIF] [0x%08X] <- [0x%08X]\r\n", value, address);
    }

    return ret;
}

#define FLASH_STATE_READ      0
#define FLASH_STATE_UNLOCK_2  1
#define FLASH_STATE_UNLOCKED  2
#define FLASH_STATE_ERASE_1   3
#define FLASH_STATE_ERASE_2   4
#define FLASH_STATE_ERASE_3   5
#define FLASH_STATE_PROGRAM   6
#define FLASH_STATE_UNLOCK_BYPASS   7
#define FLASH_STATE_UNLOCK_BYPASS_RESET 8
#define FLASH_STATE_UNLOCK_BYPASS_ERASE 9
#define FLASH_STATE_BLOCK_ERASE_BUSY 10

unsigned int flash_get_blocksize(unsigned int rom, unsigned int size, unsigned int word_offset)
{
    switch(size)
    {
        /* 32mbit flash x16 */
        case 0x00400000:
            if((word_offset < 0x8000) || (word_offset > 0x1F0000))
            {
                /* 4 kwords */
                return 4 * 1024 * 2;
            }
            else
            {
                /* 32 kwords */
                return 32 * 1024 * 2;
            }
            break;

        default:
            return 0;
    }
}

unsigned int eos_handle_rom ( unsigned int rom, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int pc = ws->cpu->env.regs[15];
    unsigned int ret = 0;
    unsigned int real_address = 0;
    unsigned int byte_offset = 0;
    unsigned int word_offset = 0;
    unsigned int base = 0;
    unsigned int size = 0;
    unsigned int fail = 0;

    static int block_erase_counter = 0;
    static int state[2] = { FLASH_STATE_READ, FLASH_STATE_READ };

    switch(rom)
    {
        case 0:
            base = 0xF8000000;
            size = ROM1_SIZE;
            break;
        case 1:
            base = 0xF0000000;
            size = ROM0_SIZE;
            break;
    }

    /* the offset relative from flash chip start */
    byte_offset = (address - base) & (size - 1);
    word_offset = byte_offset >> 1;

    /* the address of the flash data in memory space */
    real_address = base + byte_offset;

    if(!ws->flash_state_machine)
    {
        return eos_default_handle ( ws, real_address, type, value );
    }

    if(type & MODE_WRITE)
    {
        switch(state[rom])
        {
            case FLASH_STATE_READ:
                if(value == 0xF0)
                {
                    state[rom] = FLASH_STATE_READ;
                }
                else if(word_offset == 0x555 && value == 0xAA)
                {
                    state[rom] = FLASH_STATE_UNLOCK_2;
                }
                else if(value == 0xA0)
                {
                    printf("[ROM%i:%i] at [0x%04X] Command: UNLOCK BYPASS PROGRAM\r\n", rom, state[rom], pc);
                    state[rom] = FLASH_STATE_PROGRAM;
                }
                else if(value == 0x80)
                {
                    state[rom] = FLASH_STATE_UNLOCK_BYPASS_ERASE;
                }
                else if(value == 0x90)
                {
                    state[rom] = FLASH_STATE_UNLOCK_BYPASS_RESET;
                }
                else if(value == 0x98)
                {
                    printf("[ROM%i:%i] at [0x%04X] Command: UNLOCK BYPASS CFI unhandled\r\n", rom, state[rom], pc);
                    state[rom] = FLASH_STATE_READ;
                }
                else
                {
                    fail = 1;
                }
                break;

            case FLASH_STATE_UNLOCK_BYPASS:
                printf("[ROM%i:%i] at [0x%04X]       2nd UNLOCK BYPASS [0x%08X] -> [0x%08X] unhandled\r\n", rom, state[rom], pc, value, word_offset);
                state[rom] = FLASH_STATE_READ;
                break;


            case FLASH_STATE_UNLOCK_BYPASS_RESET:
                if(value == 0x00)
                {
                    printf("[ROM%i:%i] at [0x%04X] Command: UNLOCK BYPASS RESET\r\n", rom, state[rom], pc);
                    state[rom] = FLASH_STATE_READ;
                }
                else
                {
                    fail = 1;
                }
                break;

            case FLASH_STATE_UNLOCK_2:
                if(word_offset == 0x2AA && value == 0x55)
                {
                    state[rom] = FLASH_STATE_UNLOCKED;
                }
                else
                {
                    state[rom] = FLASH_STATE_READ;
                    fail = 1;
                }
                break;

            case FLASH_STATE_UNLOCKED:
                if(value == 0x90)
                {
                    printf("[ROM%i:%i] at [0x%04X] [0x%08X] -> [0x%08X] in autoselect unhandled\r\n", rom, state[rom], pc, value, word_offset);
                    state[rom] = FLASH_STATE_READ;
                }
                else if(word_offset == 0x555 && value == 0xA0)
                {
                    //printf("[ROM%i:%i] at [0x%04X] Command: PROGRAM\r\n", rom, state[rom], pc);
                    state[rom] = FLASH_STATE_PROGRAM;
                }
                else if(word_offset == 0x555 && value == 0x20)
                {
                    printf("[ROM%i:%i] at [0x%04X] Command: UNLOCK BYPASS\r\n", rom, state[rom], pc);
                    state[rom] = FLASH_STATE_READ;
                }
                else if(word_offset == 0x555 && value == 0x80)
                {
                    state[rom] = FLASH_STATE_ERASE_1;
                }
                else
                {
                    state[rom] = FLASH_STATE_READ;
                    fail = 1;
                }
                break;

            case FLASH_STATE_ERASE_1:
                if(word_offset == 0x555 && value == 0xAA)
                {
                    state[rom] = FLASH_STATE_ERASE_2;
                }
                else
                {
                    state[rom] = FLASH_STATE_READ;
                    fail = 1;
                }
                break;

            case FLASH_STATE_ERASE_2:
                if(word_offset == 0x2AA && value == 0x55)
                {
                    state[rom] = FLASH_STATE_ERASE_3;
                }
                else
                {
                    state[rom] = FLASH_STATE_READ;
                    fail = 1;
                }
                break;

            case FLASH_STATE_UNLOCK_BYPASS_ERASE:
                if(value == 0x30)
                {
                    int pos = 0;
                    int block_size = flash_get_blocksize(rom, size, word_offset);

                    printf("[ROM%i:%i] at [0x%04X] Command: UNLOCK BYPASS BLOCK ERASE [0x%08X]\r\n", rom, state[rom], pc, real_address);
                    for(pos = 0; pos < block_size; pos += 2)
                    {
                        eos_set_mem_w ( ws, real_address + pos, 0xFFFF );
                    }
                    block_erase_counter = 0;
                    state[rom] = FLASH_STATE_BLOCK_ERASE_BUSY;
                }
                else if(value == 0x10)
                {
                    int pos = 0;

                    printf("[ROM%i:%i] at [0x%04X] Command: UNLOCK BYPASS CHIP ERASE\r\n", rom, state[rom], pc);
                    for(pos = 0; pos < size; pos += 2)
                    {
                        eos_set_mem_w ( ws, base + pos, 0xFFFF );
                    }
                    state[rom] = FLASH_STATE_READ;
                }
                else
                {
                    fail = 1;
                }
                break;

            case FLASH_STATE_ERASE_3:
                if(word_offset == 0x555 && value == 0x10)
                {
                    int pos = 0;
                    printf("[ROM%i:%i] at [0x%04X] Command: CHIP ERASE\r\n", rom, state[rom], pc);
                    for(pos = 0; pos < size; pos += 2)
                    {
                        eos_set_mem_w ( ws, base + pos, 0xFFFF );
                    }
                    state[rom] = FLASH_STATE_READ;
                }
                else if(value == 0x30)
                {
                    int pos = 0;
                    int block_size = flash_get_blocksize(rom, size, word_offset);

                    printf("[ROM%i:%i] at [0x%04X] Command: BLOCK ERASE [0x%08X]\r\n", rom, state[rom], pc, real_address);
                    for(pos = 0; pos < block_size; pos += 2)
                    {
                        eos_set_mem_w ( ws, real_address + pos, 0xFFFF );
                    }
                    block_erase_counter = 0;
                    state[rom] = FLASH_STATE_BLOCK_ERASE_BUSY;
                }
                else
                {
                    state[rom] = FLASH_STATE_READ;
                    fail = 1;
                }
                break;

            case FLASH_STATE_PROGRAM:
                printf("[ROM%i:%i] at [0x%04X] Command: PROGRAM [0x%04X] -> [0x%08X]\r\n", rom, state[rom], pc, value, real_address);
                eos_set_mem_w ( ws, real_address, value );
                state[rom] = FLASH_STATE_READ;
                break;
        }
        if(fail)
        {
            printf("[ROM%i:%i] at [0x%04X] [0x%08X] -> [0x%08X]\r\n", rom, state[rom], pc, value, word_offset);
        }
    }
    else
    {

        switch(state[rom])
        {
            case FLASH_STATE_READ:
                ret = eos_default_handle ( ws, real_address, type, value );
                break;

            case FLASH_STATE_BLOCK_ERASE_BUSY:
                if(block_erase_counter < 0x10)
                {
                    block_erase_counter++;
                    ret = ((block_erase_counter&1)<<6) | ((block_erase_counter&1)<<2);
                }
                else
                {
                    ret = 0x80;
                    state[rom] = FLASH_STATE_READ;
                }
                break;

            default:
                printf("[ROM%i:%i] at [0x%04X] read in unknown state [0x%08X] <- [0x%08X]\r\n", rom, state[rom], pc, ret, word_offset);
                break;
        }
    }

    return ret;
}


unsigned int eos_handle_flashctrl ( unsigned int parm, EOSState *ws, unsigned int address, unsigned char type, unsigned int value )
{
    unsigned int pc = ws->cpu->env.regs[15];
    unsigned int ret = 0;

    switch(address & 0x1FF)
    {
        case 0x10:
            if(type & MODE_WRITE)
            {
                if(((value | (value >> 16)) & 0xFFFF) == 0xD9C5)
                {
                    printf("[FlashIF] at [0x%08X]: 'Write enable' enabled\r\n", pc);
                }
                else if(value == 0x0)
                {
                    printf("[FlashIF] at [0x%08X]: 'Write enable' disabled\r\n", pc);
                }
                else
                {
                    printf("[FlashIF] at [0x%08X]: unknown command\r\n", pc);
                }
            }
            else
            {
                ret = 1;
            }
            break;
    }
    return ret;
}


/* its not done yet */
#if defined(EOS_ROM_DEVICE_IMPLEMENTED)
ROMState *eos_rom_register(hwaddr base, DeviceState *qdev, const char *name, hwaddr size,
                                BlockDriverState *bs,
                                uint32_t sector_len, int nb_blocs, int width,
                                uint16_t id0, uint16_t id1,
                                uint16_t id2, uint16_t id3, int be)
{
    DeviceState *dev = qdev_create(NULL, "eos.rom");
    SysBusDevice *busdev = SYS_BUS_DEVICE(dev);
    ROMState *pfl = (ROMState *)object_dynamic_cast(OBJECT(dev),
                                                    "cfi.pflash01");

    if (bs && qdev_prop_set_drive(dev, "drive", bs)) {
        abort();
    }
    qdev_prop_set_uint32(dev, "num-blocks", nb_blocs);
    qdev_prop_set_uint64(dev, "sector-length", sector_len);
    qdev_prop_set_uint8(dev, "width", width);
    qdev_prop_set_uint8(dev, "big-endian", !!be);
    qdev_prop_set_uint16(dev, "id0", id0);
    qdev_prop_set_uint16(dev, "id1", id1);
    qdev_prop_set_uint16(dev, "id2", id2);
    qdev_prop_set_uint16(dev, "id3", id3);
    qdev_prop_set_string(dev, "name", name);
    qdev_init_nofail(dev);

    sysbus_mmio_map(busdev, 0, base);
    return pfl;
}

static const MemoryRegionOps eos_rom_ops = {
/*    .old_mmio = {
        .read = { pflash_readb_be, pflash_readw_be, pflash_readl_be, },
        .write = { pflash_writeb_be, pflash_writew_be, pflash_writel_be, },
    },
    .endianness = DEVICE_NATIVE_ENDIAN,*/
};

static int eos_rom_init(SysBusDevice *dev)
{
    ROMState *pfl = FROM_SYSBUS(typeof(*pfl), dev);
    uint64_t total_len = 0x00100000;
    int ret = 0;

    memory_region_init_rom_device(&pfl->mem, &eos_rom_ops, pfl, pfl->name, total_len);
    vmstate_register_ram(&pfl->mem, DEVICE(pfl));
    pfl->storage = memory_region_get_ram_ptr(&pfl->mem);
    sysbus_init_mmio(dev, &pfl->mem);

    if (pfl->bs) {
        /* read the initial flash content */
        ret = bdrv_read(pfl->bs, 0, pfl->storage, total_len >> 9);

        if (ret < 0) {
            vmstate_unregister_ram(&pfl->mem, DEVICE(pfl));
            memory_region_destroy(&pfl->mem);
            return 1;
        }
    }

    return ret;
}

static Property eos_rom_properties[] = {
    DEFINE_PROP_DRIVE("drive", ROMState, bs),
    DEFINE_PROP_UINT32("num-blocks", ROMState, nb_blocs, 0),
    DEFINE_PROP_UINT64("sector-length", ROMState, sector_len, 0),
    DEFINE_PROP_UINT8("width", ROMState, width, 0),
    DEFINE_PROP_UINT8("big-endian", ROMState, be, 0),
    DEFINE_PROP_UINT16("id0", ROMState, ident0, 0),
    DEFINE_PROP_UINT16("id1", ROMState, ident1, 0),
    DEFINE_PROP_UINT16("id2", ROMState, ident2, 0),
    DEFINE_PROP_UINT16("id3", ROMState, ident3, 0),
    DEFINE_PROP_STRING("name", ROMState, name),
    DEFINE_PROP_END_OF_LIST(),
};

static const TypeInfo eos_rom_info = {
    .name           = "eos.rom",
    .parent         = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(ROMState),
    .class_init     = eos_rom_class_init,
};


static void eos_rom_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    SysBusDeviceClass *k = SYS_BUS_DEVICE_CLASS(class);

    k->init = eos_rom_init;
    dc->props = eos_rom_properties;
}

static void eos_rom_register_types(void)
{
    type_register_static(&eos_rom_info);
}

type_init(eos_rom_register_types)
#endif
