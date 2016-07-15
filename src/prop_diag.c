/**
 * Property parsing code inspired from g3gg0's Property Editor and Indy's parse_prop.py,
 * but rewritten from scratch :)
 * 
 * Python version available on request.
 * 
 * Data structure: in ROM, properties are organized in blocks, sub-blocks and sub-sub-blocks.
 * 
 * Each nesting level uses the same structure:
 * [id_1 size_1 data_1    id_2 size_2 data_2    ...    id_n size_n data_n    terminator]
 * 
 * - "size" covers the size of "id", "size" and "data" fields, so "data" has size-8 bytes
 * - "data" is either an array of sub-blocks [subblock_1 subblock_2 ... subblock_n terminator]
 *    or the property data itself.
 * - "id", "size" and "terminator" are 32-bit integers each.
 * 
 * Consistency check: sum of size_i == buffer_len - 4.
 * There are 4 nesting levels: let's call them "class", "group", "subgroup" and "property".
 * 
 */

#ifndef CONFIG_MAGICLANTERN
#include <stdio.h>
#include <malloc.h>
#include <stdint.h>
#else
extern int printf(const char* fmt, ... );
#endif

#define bool int
#include <compiler.h>
#include <property.h>

struct nesting_level
{
    char name[10];
    uint32_t terminator1;
    uint32_t terminator2;
};

struct nesting_level levels[] = {
    { "class",    0x0F000000, 0x1F000000 },
    { "group",    0x00FF0000 },
    { "subgroup", 0x0000FFFF },
    { "property", 0x00000000 },
};

/* camera info extracted from properties */
struct
{
    char* camera_model;
    char* firmware_version;
    char* firmware_subversion;
    char* artist;
    char* copyright;
    char* picstyle_1_name;
    char* picstyle_2_name;
    char* picstyle_3_name;
    char* file_prefix;
    char* dir_suffix;
    int file_number;
} cam_info = {
    "???", "???", "???", "???", "???",
    "???", "???", "???", "????", "?????", 0
};

union prop_data
{
    char str[400];
    int32_t i32[100];
    int8_t i8[400]; 
};

void process_property(uint32_t property, union prop_data * data, uint32_t data_len)
{
    switch (property)
    {
        case PROP_CAM_MODEL:
            cam_info.camera_model = data->str;
            break;
        
        case PROP_FIRMWARE_VER:
            cam_info.firmware_version = data->str;
            break;
        
        case 0x2000005:
            cam_info.firmware_subversion = data->str;
            break;
        
        case PROP_ARTIST_STRING:
            cam_info.artist = data->str;
            
            /* Canon only deletes the first char when you delete artist name?! */
            if (!cam_info.artist[0] && cam_info.artist[1])
                cam_info.artist++;
            
            break;
        
        case PROP_COPYRIGHT_STRING:
            cam_info.copyright = data->str;

            /* Canon only deletes the first char when you delete copyright string?! */
            if (!cam_info.copyright[0] && cam_info.copyright[1])
                cam_info.copyright++;
            
            break;
        
        case PROP_PC_FLAVOR1_PARAM:
            cam_info.picstyle_1_name = data->str + 4;
            break;

        case PROP_PC_FLAVOR2_PARAM:
            cam_info.picstyle_2_name = data->str + 4;
            break;

        case PROP_PC_FLAVOR3_PARAM:
            cam_info.picstyle_3_name = data->str + 4;
            break;
        
        case PROP_FILE_PREFIX:
        case PROP_USER_FILE_PREFIX:
            cam_info.file_prefix = data->str;
            break;
        
        case PROP_DCIM_DIR_SUFFIX:
            cam_info.dir_suffix = data->str;
            break;
        
        case PROP_FILE_NUMBER_A:
        case PROP_FILE_NUMBER_B:
            if (data->i32[0])
            {
                cam_info.file_number = data->i32[0];
            }
            break;
    }
}

/* parse a property buffer (id, size, data) */
/* buffer_len is always in bytes */
void parse_property(uint32_t* buffer, uint32_t buffer_len)
{
    uint32_t id = buffer[0];
    uint32_t size = buffer[1] - 8;
    uint32_t* data = &buffer[2];
    // assert len(data) == size
    
    #ifndef CONFIG_MAGICLANTERN
    printf("Prop %10x %6d %s\n", id, size, (char*)data);
    #endif

    process_property(id, (union prop_data *) data, size);
}

static int check_terminator(int level, uint32_t tail, int verbose)
{
    if (levels[level].terminator1 == 0)
    {
        /* nothing to check */
        return -1;
    }
    
    if (levels[level].terminator1 == tail)
    {
        /* check if terminator matches the current group's tail */
        return 1;
    }

    if (levels[level].terminator2)
    {
        /* some property blocks may use different terminator values */
        if (levels[level].terminator2 == tail)
        {
            return 1;
        }
    }

    if (verbose)
    {
        printf("%s end error (0x%x, expected 0x%x or 0x%x)\n",
            levels[level].name, tail,
            levels[level].terminator1,
            levels[level].terminator2
        );
    }

    return 0;
}

/* parse a class of groups of subgroups of properties (recursive) */
int parse_prop_group(uint32_t* buffer, int buffer_len, int level, int verbose, int output)
{
    //~ printf("parse prop group %p %x %d\n", buffer, buffer_len, level);
    uint32_t id = buffer[0];
    int size = buffer[1];
    
    if (size < 12 || size > buffer_len)
    {
        if (verbose) printf("%s size error (0x%x, expected between 0x0c and 0x%x)\n", levels[level].name, size, buffer_len);
        return 0;
    }
    
    uint32_t tail = buffer[size/4 - 1];

    if (output && levels[level].terminator1 == 0)
    {
        parse_property(buffer, size);
    }
    
    if (verbose)
    {
        printf("%s: id=0x%x size=0x%x tail=0x%x\n", levels[level].name, id, size, tail);
    }

    if (check_terminator(level, tail, verbose))
    {
        if (level+1 < COUNT(levels))
        {
            int ok = parse_prop_group(buffer+2, size-8, level+1, verbose, output);
            if (!ok)
            {
                if (verbose) printf("%s: check failed\n", levels[level+1].name);
                return 0;
            }
        }
    }
    else
    {
        /* error message was printed in check_terminator */
        return 0;
    }

    if (buffer_len - size == 4)
    {
        if (verbose) printf("%s end OK\n", levels[level].name);
        return 1;
    }
    else
    {
        if (verbose) printf("%s: 0x%x bytes remaining\n", levels[level].name, buffer_len - size);
        return parse_prop_group(buffer + size/4, buffer_len - size, level, verbose, output);
    }

    return 1;
}

/* Brute force: find the offsets that look like valid property data structures */
void guess_prop(uint32_t* buffer, uint32_t buffer_len, int verbose)
{
    int total = buffer_len;
    int offset = 0;
    while (offset < total)
    {
        int size = buffer[offset/4 + 1];
        if (size > 0 && (size & 3) == 0)                        // size looks 32-bit aligned?
        {
            int last_pos = offset + size - 4;
            if (last_pos > 12 && last_pos < total-4)            // not out of range?
            {
                uint32_t last = buffer[last_pos/4];
                if (check_terminator(0, last, 0))               // terminator OK?
                {
                    if (verbose) printf("Trying offset 0x%x, size=0x%x...\n", offset, size);

                    /* let's try to parse it quietly, without any messages */
                    /* if successful, will parse again with output enabled */
                    if (parse_prop_group(buffer + offset/4, size+4, 0, verbose, 0))
                    {
                        parse_prop_group(buffer + offset/4, size+4, 0, verbose, 1);
                    }
                }
            }
        }
        
        /* these properties must be aligned, right? */
        offset += 0x100;
    }
}

static void print_camera_info()
{
    printf(" - Camera model: %s\n", cam_info.camera_model);
    printf(" - Firmware version: %s / %s\n", cam_info.firmware_version, cam_info.firmware_subversion);
    printf(" - IMG naming: 100%s/%s%04d.JPG\n", cam_info.dir_suffix, cam_info.file_prefix, cam_info.file_number);
    if (cam_info.artist[0])
    {
        printf(" - Artist: %s\n", cam_info.artist);
    }
    if (cam_info.copyright[0])
    {
        printf(" - Copyright: %s\n", cam_info.copyright);
    }
    if (cam_info.picstyle_1_name[0] || cam_info.picstyle_2_name[0] || cam_info.picstyle_3_name[0])
    {
        printf(" - User PS: %s %s %s\n", cam_info.picstyle_1_name, cam_info.picstyle_2_name, cam_info.picstyle_3_name);
    }
}

#ifdef CONFIG_MAGICLANTERN
    /* for running on the camera */
    void prop_diag()
    {
        guess_prop((void*)0xF0000000, 0x1000000, 0);
        guess_prop((void*)0xF8000000, 0x1000000, 0);
        guess_prop((void*)0xFC000000, 0x2000000, 0);
        print_camera_info();
    }
#else
    /* for offline testing */
    void main(int argc, char** argv)
    {
        if (argc < 2)
        {
            printf("usage: %s ROM0.BIN\n", argv[0]);
            return;
        }
        printf("Loading %s...\n", argv[1]);
        char* filename = argv[1];
        FILE* f = fopen(filename, "rb");
        if (f)
        {
            fseek(f, 0, SEEK_END);
            int len = ftell(f);
            fseek(f, 0, SEEK_SET);
            int* buf = malloc(len);
            fread(buf, len, 1, f);
            fclose(f);
            
            printf("Scanning from %p to %p...\n", buf, ((void*)buf) + len);
            guess_prop(buf, len, 0);
            print_camera_info();
        }
    }
#endif
