
#ifndef _ime_base_h_
#define _ime_base_h_

#define IME_MAX_METHODS 32

/* for now UTF-8 is the only method that is obligatory */
#define IME_UTF8     0
#define IME_CP437    1
#define IME_CP819    2

/* allowed charsets - basic categories */
#define IME_CHARSET_ALPHA       0x01
#define IME_CHARSET_NUMERIC     0x02
#define IME_CHARSET_PUNCTUATION 0x04
#define IME_CHARSET_SPECIAL     0x08
#define IME_CHARSET_MATH        0x10
#define IME_CHARSET_MAIL        0x20
#define IME_CHARSET_FILENAME    0x40

/* all characters are allowed, passing parameter 0x00 will also map to this */
#define IME_CHARSET_ANY         0xFFFFFFFF

/* return codes */
#define IME_OK           0
#define IME_CANCEL       1
#define IME_ERR_UNAVAIL  2
#define IME_ERR_UNKNOWN  3


/* called whenever the string or the cursor position has changed.
   when x/y/w/h parameters for ime_base_start were other than zero, this function has to handle text update.
   this function is also used to validate the entered string.
   if it returns IME_OK, the string is valid,
   if it returns != 0, the IME knows that the string is invalid and grays out the OK functionality
   
   'selection_length' specifies how many characters starting from caret_pos are selected. 0 if none (plain cursor)
 */
typedef unsigned int (*t_ime_update_cbr)(void *ctx, unsigned char *text, int caret_pos, int selection_length);
#define IME_UPDATE_FUNC(func) unsigned int func(void *ctx, unsigned char *text, int caret_pos, int selection_length)

/* this callback is called when the dialog box was accepted or cancelled
   when the string was entered, status will be IME_OK.
   if the user aborted input, status will be IME_CANCEL.
 */
typedef unsigned int (*t_ime_done_cbr)(void *ctx, unsigned int status, unsigned char *text);
#define IME_DONE_FUNC(func) unsigned int func(void *ctx, unsigned int status, unsigned char *text)

/* call this function to start the IME system - this is asynchronous if done_cbr is != NULL.
   it will call 'update' if (update != NULL) periodically or on any update_cbr (caret pos or string) and done_cbr when the dialog is finished.
   return the context of the dialog if it was started. this is a paramete for future functions and used to identify the exact dialog.
   
   in case of any other error (e.g. unavailability of some resource) it will return NULL.
   if an error occured, the error message will be placed in the 'text' pointer given, so make sure you use a separate buffer.
   
   when the IME method supports this feature (i.e. non-fullscreen methods) the parameters x, y, w, h specify where the caller
   placed its text field that should not be overwritten. your update cbr must handle displaying the string in this case. 
   if you dont care about this, pass all values as zero. 
 */
extern void * ime_base_start (unsigned char *caption, unsigned char *text, int max_length, int codepage, int charset, t_ime_update_cbr update_cbr, t_ime_done_cbr done_cbr, int x, int y, int w, int h );
typedef void * (*t_ime_start) (unsigned char *caption, unsigned char *text, int max_length, int codepage, int charset, t_ime_update_cbr update_cbr, t_ime_done_cbr done_cbr, int x, int y, int w, int h );

/* this structure is passed when registering */
typedef struct 
{
    char *name;
    char *description;
    t_ime_start start;
    void (*configure)();
} t_ime_handler;

/* IME modules call this function to register themselves. threadsafe */
#ifndef _ime_base_c_
static unsigned int ime_base_unavail(t_ime_handler *handler)
{
    bmp_printf(FONT_MED, 30, 190, "IME Handler %s intalled, but 'ime_base' missing.", handler->name);
    beep();
    msleep(2000);
    return IME_ERR_UNAVAIL;
}

extern WEAK_FUNC(ime_base_unavail) unsigned int ime_base_register(t_ime_handler *handler);

#endif


#endif