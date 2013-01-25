#include "interpreter.h"

void LibMsleep(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    msleep(Param[0]->Val->Integer);
}

void LibTakePic(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    lens_take_picture(64,0);
}

void LibBulbPic(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    bulb_take_pic(Param[0]->Val->Integer);
}

void LibPress(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    int btn = Param[0]->Val->Integer;
    switch (btn)
    {
        case BGMT_PRESS_HALFSHUTTER:
            SW1(1,50);
            return;
        case BGMT_PRESS_FULLSHUTTER:
            SW2(1,50);
            return;
        default:
            if (btn > 0) fake_simple_button(btn);
    }
}

void LibUnpress(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    int press = Param[0]->Val->Integer;
    int unpress = 0;
    switch (press)
    {
        case BGMT_PRESS_HALFSHUTTER:
            SW1(0,50);
            return;
        case BGMT_PRESS_FULLSHUTTER:
            SW2(0,50);
            return;
        case BGMT_PRESS_SET:
            unpress = BGMT_UNPRESS_SET;
            break;
        case BGMT_PRESS_ZOOMIN_MAYBE:
            unpress = BGMT_UNPRESS_ZOOMIN_MAYBE;
            break;
        case BGMT_PRESS_ZOOMOUT_MAYBE:
            unpress = BGMT_UNPRESS_ZOOMOUT_MAYBE;
            break;
        
        #ifdef BGMT_UNPRESS_UDLR
        case BGMT_PRESS_LEFT:
        case BGMT_PRESS_RIGHT:
        case BGMT_PRESS_UP:
        case BGMT_PRESS_DOWN:
            unpress = BGMT_UNPRESS_UDLR;
            break;
        #else
        case BGMT_PRESS_LEFT: 
            unpress = BGMT_UNPRESS_LEFT; 
            break;
        case BGMT_PRESS_RIGHT: 
            unpress = BGMT_UNPRESS_RIGHT; 
            break;
        case BGMT_PRESS_UP: 
            unpress = BGMT_UNPRESS_UP; 
            break;
        case BGMT_PRESS_DOWN: 
            unpress = BGMT_UNPRESS_DOWN; 
            break;
        #endif
        
        default:
            // this button does not have an unpress code
            return;
    }
    ASSERT(unpress > 0);
    fake_simple_button(unpress);
}

void LibClick(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    LibPress(Parser, ReturnValue, Param, NumArgs);
    LibUnpress(Parser, ReturnValue, Param, NumArgs);
}


void LibBeep(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    beep();
}

void LibConsoleShow(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    console_show();
}

void LibConsoleHide(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    console_hide();
}

void LibMovieStart(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    movie_start();
}

void LibMovieEnd(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    movie_end();
}

struct _tm { int hour; int minute; int second; int year; int month; int day; };
void LibGetTime(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    struct tm now;
    LoadCalendarFromRTC(&now);
    
    static struct _tm t;
    t.hour = now.tm_hour;
    t.minute = now.tm_min;
    t.second = now.tm_sec;
    t.year = now.tm_year + 1900;
    t.month = now.tm_mon + 1;
    t.day = now.tm_mday;
    ReturnValue->Val->Pointer = &t;
}

/* list of all library functions and their prototypes */
struct LibraryFunction PlatformLibrary[] =
{
    /** General-purpose functions */
    {LibMsleep,         "void msleep(int delay);"       },  // sleep X milliseconds
    {LibBeep,           "void beep();"                  },  // short beep sound
    {LibConsoleShow,    "void console_show();"          },  // show the script console
    {LibConsoleHide,    "void console_hide();"          },  // hide the script console

    /** Date/time */
    {LibGetTime,        "struct tm * get_time();"},     // get current date/time (hour, minute, second, year, month, day); the pointer is to a static structure

    /** Picture taking */
    {LibTakePic,        "void takepic();"               },  // take a picture
    {LibBulbPic,        "void bulbpic(int ms);"         },  // take a picture in bulb mode
    
    /** Video recording */
    {LibMovieStart,     "void movie_start();"           },  // start recording
    {LibMovieEnd,       "void movie_end();"             },  // stop recording

    /** Button press emulation */
    // LEFT, RIGHT, UP, DOWN, SET, MENU, PLAY, ERASE, Q, LV, INFO, ZOOM_IN, ZOOM_OUT
    // SHOOT_FULL, SHOOT_HALF
    {LibPress,          "void press(int button);"       },  // "press" a button
    {LibUnpress,        "void unpress(int button);"     },  // "unpress" a button
    {LibClick,          "void click(int button);"       },  // "press" and then "unpress" a button

    /** Exposure settings */
    // APEX units
#if 0 // not yet implemented
    {LibGetTv,        "float get_tv();"                 },
    {LibGetAv,        "float get_av();"                 },
    {LibGetSv,        "float get_sv();"                 },
    {LibSetTv,        "void set_tv(float tv);"          },
    {LibSetAv,        "void set_av(float av);"          },
    {LibSetSv,        "void set_sv(float av);"          },

    // Conventional units (ISO 100, 1.0/4000, 2.8)
    {LibGetISO,        "int get_iso();"                 },
    {LibGetShutter,    "float get_shutter();"           },
    {LibGetAperture,   "float get_aperture();"          },
    {LibSetISO,        "void set_iso(int iso);"         },
    {LibSetShutter,    "void set_shutter(float s);"     },
    {LibSetAperture,   "void set_aperture(float s);"    },

    // Raw units (1/8 EV steps)
    {LibGetRawISO,      "int get_rawiso();"             },
    {LibGetRawShutter,  "int get_rawshutter();"         },
    {LibGetRawAperture, "int get_rawaperture();"        },
    {LibSetRawISO,      "void set_rawiso(int raw);"     },
    {LibSetRawShutter,  "void set_rawshutter(int raw);" },
    {LibSetRawAperture, "void set_rawaperture(int raw);"},
    
    /** Exposure compensation (in EV) */
    {LibGetAE,          "float get_ae();"               },
    {LibGetFlashAE,     "float get_flash_ae();"         },
    {LibSetAE,          "void set_ae(float ae);"        },
    {LibSetFlashAE,     "void set_flash_ae(float ae);"  },

    /** White balance */
    {LibGetKelvin,      "int get_kelvin();"             },
    {LibGetGreen,       "int get_green();"              },
    {LibSetKelvin,      "void set_kelvin();"            },
    {LibSetGreen,       "void set_green();"             },

    /** MLU, HTP, misc settings */
    {LibGetMLU,         "int get_mlu();"                },
    {LibGetHTP,         "int get_htp();"                },

    /** Interaction with menus */
    { LibMenuOpen,       "void menu_open();"            },  // open ML menu
    { LibMenuSelect,     "void menu_select(char* tab, char* entry);"}, // select a menu tab and entry (e.g. Overlay, Focus Peak)
    { LibMenuClose,      "void menu_close();"           },  // close ML menu
    
    /** Image analysis */
    { LibGetPixelYUV,    "void get_pixel_yuv(int x, int y, int* Y, int* U, int* V);" },          // get the YUV components of a pixel from LiveView (720x480)
    { LibGetSpotYUV,     "void get_spot_yuv(int x, int y, int size, int* Y, int* U, int* V);" }, // spotmeter: average pixels from a (small) box and return average YUV
    { LibYUV2RGB,        "void yuv2rgb(int Y, int U, int V, int* R, int* G, int* B);"},          // convert from YUV to RGB
    { LibRGB2YUV,        "void rgb2yuv(int R, int G, int B, int* Y, int* U, int* V);"},          // convert from RGB to YUV
    { LibGetHistoRange,  "float get_histo_range(int from, int to);"},                            // percent of values between <from> and <to> in histogram data
    
    /** Audio stuff */
    { LibAudioGetLevel,  "int audio_get_level(int channel, int type);" }, // channel: 0/1; type: INSTANT, AVG, PEAK, PEAK_FAST
    { LibAudioLevelToDB, "int audio_level_to_db(int level)"            }, // conversion from 16-bit signed to dB
    
    /** Powersaving */
    { LibDisplayOn,     "void display_on();"           },
    { LibDisplayOff,    "void display_off();"          },
    { LibDisplayIsOn,   "int display_is_on();"         },

    { LibLVPause,       "void lv_pause();"             }, // pause LiveView without dropping the mirror
    { LibLVResume,      "void lv_resume();"            },
#endif
    { NULL,         NULL }
};

void PlatformLibraryInit()
{

    struct ParseState Parser;
    char *Identifier;
    struct ValueType *ParsedType;
    void *Tokens;
    const char *IntrinsicName = TableStrRegister("time lib");
    const char *StructDefinition = "struct tm { int hour; int minute; int second; int year; int month; int day; }";
    Tokens = LexAnalyse(IntrinsicName, StructDefinition, strlen(StructDefinition), NULL);
    LexInitParser(&Parser, StructDefinition, Tokens, IntrinsicName, TRUE);
    TypeParse(&Parser, &ParsedType, &Identifier, NULL);
    HeapFreeMem(Tokens);

    LibraryAdd(&GlobalTable, "platform library", &PlatformLibrary[0]);

    static int LEFT = BGMT_PRESS_LEFT;
    static int RIGHT = BGMT_PRESS_RIGHT;
    static int UP = BGMT_PRESS_UP;
    static int DOWN = BGMT_PRESS_DOWN;

    static int WHEEL_UP = BGMT_WHEEL_UP;
    static int WHEEL_DOWN = BGMT_WHEEL_DOWN;
    static int WHEEL_LEFT = BGMT_WHEEL_LEFT;
    static int WHEEL_RIGHT = BGMT_WHEEL_RIGHT;

    static int SET = BGMT_PRESS_SET;
    static int MENU = BGMT_MENU;
    static int PLAY = BGMT_PLAY;
    static int ERASE = BGMT_TRASH;
    static int INFO = BGMT_INFO;
    static int Q = 
        #ifdef BGMT_Q
            BGMT_Q;
        #else
            0;
        #endif
    static int LV = BGMT_LV;
    static int ZOOM_IN = BGMT_PRESS_ZOOMIN_MAYBE;
    static int ZOOM_OUT = BGMT_PRESS_ZOOMOUT_MAYBE;
    static int SHOOT_HALF = BGMT_PRESS_HALFSHUTTER;
    static int SHOOT_FULL = BGMT_PRESS_FULLSHUTTER;
    
    #define READONLY_VAR(x) VariableDefinePlatformVar(NULL, #x, &IntType, (union AnyValue *)&x,      FALSE);
    READONLY_VAR(LEFT)
    READONLY_VAR(RIGHT)
    READONLY_VAR(UP)
    READONLY_VAR(DOWN)
    READONLY_VAR(WHEEL_LEFT)
    READONLY_VAR(WHEEL_RIGHT)
    READONLY_VAR(WHEEL_UP)
    READONLY_VAR(WHEEL_DOWN)
    READONLY_VAR(SET)
    READONLY_VAR(MENU)
    READONLY_VAR(PLAY)
    READONLY_VAR(ERASE)
    READONLY_VAR(INFO)
    READONLY_VAR(Q)
    READONLY_VAR(LV)
    READONLY_VAR(ZOOM_IN)
    READONLY_VAR(ZOOM_OUT)
    READONLY_VAR(SHOOT_HALF)
    READONLY_VAR(SHOOT_FULL)

    READONLY_VAR(lv)
    READONLY_VAR(recording)
}
