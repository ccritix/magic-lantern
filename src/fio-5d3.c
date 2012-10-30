#include "dryos.h"
#include "property.h"
#include "bmp.h"
#include "string.h"

// File I/O wrappers for handling the dual card slot on 5D3

int ml_card_select = 1;
int card_select = 1;

#define SHOOTING_CARD_LETTER (card_select == 1 ? "A" : "B")
#define ML_CARD_LETTER (ml_card_select == 1 ? "A" : "B")

void find_ml_card()
{
    int ml_cf = is_dir("A:/ML");
    int ml_sd = is_dir("B:/ML");
    
    if (ml_cf && !ml_sd) ml_card_select = 1;
    else if (!ml_cf && ml_sd) ml_card_select = 2;
    else if (ml_cf && ml_sd)
    {
        clrscr();
        bfnt_puts("ML is on both cards, format one of them!", 0, 0, COLOR_WHITE, COLOR_BLACK);
        beep();
        redraw_after(2000);
    }
    else
    {
        clrscr();
        bfnt_puts("Could not find ML files.", 0, 0, COLOR_WHITE, COLOR_BLACK);
        beep();
        redraw_after(2000);
    }
    
    card_select = ml_card_select;
}

int cluster_size = 0;
int cluster_size_a = 0;
int cluster_size_b = 0;

int free_space_raw = 0;
int free_space_raw_a = 0;
int free_space_raw_b = 0;

int file_number = 0;
int file_number_a = 0;
int file_number_b = 0;

int folder_number = 0;
int folder_number_a = 0;
int folder_number_b = 0;

PROP_HANDLER(PROP_CARD_SELECT)
{
    card_select = buf[0];
    if (card_select == 1)
    {
        cluster_size = cluster_size_a;
        free_space_raw = free_space_raw_a;
        file_number = file_number_a;
        folder_number = folder_number_a;
    }
    else
    {
        cluster_size = cluster_size_b;
        free_space_raw = free_space_raw_b;
        file_number = file_number_b;
        folder_number = folder_number_b;
    }
}

PROP_HANDLER(PROP_CLUSTER_SIZE_A)
{
    cluster_size_a = buf[0];
    if (card_select == 1) cluster_size = buf[0];
}

PROP_HANDLER(PROP_CLUSTER_SIZE_B)
{
    cluster_size_b = buf[0];
    if (card_select == 2) cluster_size = buf[0];
}

PROP_HANDLER(PROP_FREE_SPACE_A)
{
    free_space_raw_a = buf[0];
    if (card_select == 1) free_space_raw = buf[0];
}

PROP_HANDLER(PROP_FREE_SPACE_B)
{
    free_space_raw_b = buf[0];
    if (card_select == 2) free_space_raw = buf[0];
}

PROP_HANDLER(PROP_FILE_NUMBER_A)
{
    file_number_a = buf[0];
    if (card_select == 1) file_number = buf[0];
}

PROP_HANDLER(PROP_FILE_NUMBER_B)
{
    file_number_b = buf[0];
    if (card_select == 2) file_number = buf[0];
}

PROP_HANDLER(PROP_FOLDER_NUMBER_A)
{
    folder_number_a = buf[0];
    if (card_select == 1) folder_number = buf[0];
}

PROP_HANDLER(PROP_FOLDER_NUMBER_B)
{
    folder_number_b = buf[0];
    if (card_select == 2) folder_number = buf[0];
}

static char dcim_dir_suffix[6];
static char dcim_dir[100];

PROP_HANDLER(PROP_DCIM_DIR_SUFFIX)
{
    snprintf(dcim_dir_suffix, sizeof(dcim_dir_suffix), (const char *)buf);
}

const char* get_dcim_dir()
{
    snprintf(dcim_dir, sizeof(dcim_dir), "%s:/DCIM/%03d%s", SHOOTING_CARD_LETTER, folder_number, dcim_dir_suffix);
    return dcim_dir;
}

static void guess_drive_letter(char* new_filename, const char* old_filename, int size)
{
    if (old_filename[1] == ':')
    {
        snprintf(new_filename, size, "%s", old_filename);
        return;
    }
    
    if (old_filename[0] == 'M' && old_filename[1] == 'L' && old_filename[2] == '/') // something in ML dir
    {
        snprintf(new_filename, 100, "%s:/%s", ML_CARD_LETTER, old_filename);
    }
    else
    {
        snprintf(new_filename, 100, "%s:/%s", SHOOTING_CARD_LETTER, old_filename);
    }
}

FILE* _FIO_Open(const char* filename, unsigned mode );
FILE* FIO_Open(const char* filename, unsigned mode )
{
    char new_filename[100];
    guess_drive_letter(new_filename, filename, 100);
    return _FIO_Open(new_filename, mode);
}

FILE* _FIO_CreateFile(const char* filename );
FILE* FIO_CreateFile(const char* filename )
{
    char new_filename[100];
    guess_drive_letter(new_filename, filename, 100);
    return _FIO_CreateFile(new_filename);
}

//~ int _FIO_GetFileSize(const char * filename, unsigned * size);
int FIO_GetFileSize(const char * filename, unsigned * size)
{
    char new_filename[100];
    guess_drive_letter(new_filename, filename, 100);
    return _FIO_GetFileSize(new_filename, size);
}

int _FIO_RemoveFile(const char * filename);
int FIO_RemoveFile(const char * filename)
{
    char new_filename[100];
    guess_drive_letter(new_filename, filename, 100);
    return _FIO_RemoveFile(new_filename);
}

struct fio_dirent * _FIO_FindFirstEx(const char * dirname, struct fio_file * file);
struct fio_dirent * FIO_FindFirstEx(const char * dirname, struct fio_file * file)
{
    char new_dirname[100];
    guess_drive_letter(new_dirname, dirname, 100);
    return _FIO_FindFirstEx(new_dirname, file);
}

int _FIO_CreateDirectory(const char * dirname);
int FIO_CreateDirectory(const char * dirname)
{
    char new_dirname[100];
    guess_drive_letter(new_dirname, dirname, 100);
    return _FIO_CreateDirectory(new_dirname);
}

INIT_FUNC("fio", find_ml_card);
