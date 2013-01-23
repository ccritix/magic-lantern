
#include <dryos.h>
#include <property.h>
#include <menu.h>
#include <bmp.h>
#include <config.h>
#include <consts.h>
#include <lens.h>
#include <version.h>
#include <flexinfo.h>

/* the menu is not so useful for end users, but makes it easy to tweak item positions for developers.
   actually only developer build ML from source, so keep it enabled until its in a more mature state and the next release is coming.
*/
#define FLEXINFO_DEVELOPER_MENU

#ifdef CONFIG_60D
#undef FLEXINFO_DEVELOPER_MENU // squeeze a few K of RAM
#endif

#define BUF_SIZE 128

// those are not camera-specific LP-E6
#define DISPLAY_BATTERY_LEVEL_1 60 //%
#define DISPLAY_BATTERY_LEVEL_2 20 //%

void trim(char* String)
{
    int dest;
    int src=0;
    int len = strlen(String);
    if (len == 0) return;

    // Advance src to the first non-whitespace character.
    while(isspace(String[src])) src++;

    // Copy the string to the "front" of the buffer
    for(dest=0; src<len; dest++, src++) 
    {
        String[dest] = String[src];
    }

    // Working backwards, set all trailing spaces to NULL.
    for(dest=len-1; isspace(String[dest]); --dest)
    {
        String[dest] = '\0';
    }
}

char* GetCardMaker(char drv) 
{
    if (drv=='A') 
#ifdef CARD_A_MAKER
        return  (char*)CARD_A_MAKER;
#else
        return NULL;
#endif
   else 
#ifdef CARD_B_MAKER
        return  (char*)CARD_B_MAKER;
#else
        return NULL;
#endif
}

char* GetCardModel(char drv) 
{   
    if (drv=='A') 
#ifdef CARD_A_MODEL
        return (char*)CARD_A_MODEL;
#else
        return NULL;
#endif
   else 
#ifdef CARD_B_MODEL
        return (char*)CARD_B_MODEL;
#else
        return NULL;
#endif
}

char* GetCardLabel(char drv) 
{   
    if (drv=='A') 
#ifdef CARD_A_LABEL
        return (char*)CARD_A_LABEL;
#else
        return NULL;
#endif
   else 
#ifdef CARD_B_LABEL
        return (char*)CARD_B_LABEL;
#else
        return NULL;
#endif
}


/* 
   this is the definition of the info screen elements.
   it can either be made switchable for photo and LV setting or put in an array.
   the config can get loaded from an user-save and -editable ini file.
   -> ToDo: for now there is only 7D photo screen, add others too
            do we put raw X/Y positions here or keep them im consts.h?
 */
info_elem_t info_config[] =
{
    { .config = { { INFO_TYPE_CONFIG } } },

#if defined(CONFIG_7D)
    /* print ISO range */
    { .string = { { INFO_TYPE_STRING, { ISO_RANGE_POS_X, ISO_RANGE_POS_Y, 2, .name = "ISO Range" }}, INFO_STRING_ISO_MINMAX, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },

    /* entry 2 and 3, WB strings */
    { .string = { { INFO_TYPE_STRING, { WBS_POS_X - 10, WBS_POS_Y, 2, .name = "WB GM" }}, INFO_STRING_WBS_GM, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },
    { .string = { { INFO_TYPE_STRING, { WBS_POS_X + 60, WBS_POS_Y, 2, .name = "WB BA" }}, INFO_STRING_WBS_BA, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },
    
    /* entry 4, battery_icon referenced as anchor */
    { .battery_icon = { { INFO_TYPE_BATTERY_ICON, { DISPLAY_BATTERY_POS_X, DISPLAY_BATTERY_POS_Y, 2, .name = "Battery Icon" }}, DISPLAY_BATTERY_LEVEL_2, DISPLAY_BATTERY_LEVEL_1 } },
    { .battery_perf = { { INFO_TYPE_BATTERY_PERF, { 100, 2, 3, INFO_ANCHOR_LEFT | INFO_ANCHOR_TOP, 4 }}, /* 0=vert,1=horizontal */ 0, /* x size */ 8, /* y size */ 8 } },
    { .string = { { INFO_TYPE_STRING, { 8, 3, 2, INFO_ANCHOR_HCENTER | INFO_ANCHOR_BOTTOM, 4, INFO_ANCHOR_HCENTER | INFO_ANCHOR_TOP }}, INFO_STRING_BATTERY_PCT, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },
    { .string = { { INFO_TYPE_STRING, { -20, 2, 2, INFO_ANCHOR_LEFT | INFO_ANCHOR_TOP, 4 }}, INFO_STRING_BATTERY_ID, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_LARGE } },

    /* entry 8, MLU string */
    { .string = { { INFO_TYPE_STRING, { MLU_STATUS_POS_X, MLU_STATUS_POS_Y, 2, .name = "MLU" }}, INFO_STRING_MLU, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },
    
    /* entry 9, kelvin */
    { .string = { { INFO_TYPE_STRING, { WB_KELVIN_POS_X, WB_KELVIN_POS_Y, 2, .name = "Kelvin" }}, INFO_STRING_KELVIN, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM_SHADOW } },
    
    /* entry 10, pictures */
    { .fill = { { INFO_TYPE_FILL, { 540, 390, 1, 0, 0, 0, 150, 60, .name = "Pics (clear)" }}, INFO_COL_FIELD } },
    { .string = { { INFO_TYPE_STRING, { 550, 402, 2, .name = "Pics" }}, INFO_STRING_PICTURES, COLOR_FG_NONLV, INFO_COL_FIELD, INFO_FONT_CANON } },
     
    // header 
    { .string = { { INFO_TYPE_STRING, { 28, 3, 2, .name = "Date" }}, INFO_STRING_CAM_DATE, COLOR_FG_NONLV, INFO_COL_BG, INFO_FONT_MEDIUM } },
    // how can I align the string to the right (or center)? 
    //{ .string = { { INFO_TYPE_STRING, { 693, 3, 2, .name = "Build" }}, INFO_STRING_BUILD, COLOR_FG_NONLV, INFO_COL_BG, INFO_FONT_MEDIUM } },
    // footer 
    { .string = { { INFO_TYPE_STRING, { 28, 459, 2, .name = "Card label" }}, INFO_STRING_CARD_LABEL_A, COLOR_FG_NONLV, INFO_COL_BG, INFO_FONT_MEDIUM } },
    //{ .string = { { INFO_TYPE_STRING, { 693, 459, 2, .name = "Copyright" }}, INFO_STRING_COPYRIGHT, COLOR_FG_NONLV, INFO_COL_BG, INFO_FONT_MEDIUM } },
#endif

#if defined(CONFIG_5D3)
    /* print ISO range */
    { .string = { { INFO_TYPE_STRING, { ISO_RANGE_POS_X, ISO_RANGE_POS_Y, 2, .name = "ISO Range" }}, INFO_STRING_ISO_MINMAX, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },

    /* entry 2 and 3, WB strings */
    { .string = { { INFO_TYPE_STRING, { WBS_POS_X, WBS_POS_Y, 2, .name = "WB GM" }}, INFO_STRING_WBS_GM, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },
    { .string = { { INFO_TYPE_STRING, { WBS_POS_X + 24, WBS_POS_Y, 2, .name = "WB BA" }}, INFO_STRING_WBS_BA, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },
    
    /* entry 4, battery_icon referenced as anchor */
    { .battery_icon = { { INFO_TYPE_BATTERY_ICON, { DISPLAY_BATTERY_POS_X, DISPLAY_BATTERY_POS_Y, 2, .name = "Battery Icon" }}, DISPLAY_BATTERY_LEVEL_2, DISPLAY_BATTERY_LEVEL_1 } },
    { .battery_perf = { { INFO_TYPE_BATTERY_PERF, { 86, 2, 3, INFO_ANCHOR_LEFT | INFO_ANCHOR_TOP, 4 }}, /* 0=vert,1=horizontal */ 0, /* x size */ 8, /* y size */ 8 } },
    { .string = { { INFO_TYPE_STRING, { 8, 0, 2, INFO_ANCHOR_HCENTER | INFO_ANCHOR_BOTTOM, 4, INFO_ANCHOR_HCENTER | INFO_ANCHOR_TOP }}, INFO_STRING_BATTERY_PCT, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_MEDIUM } },
    { .string = { { INFO_TYPE_STRING, { 4, 2, 2, INFO_ANCHOR_RIGHT | INFO_ANCHOR_TOP, 4 }}, INFO_STRING_BATTERY_ID, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_LARGE } },

    /* entry 8, MLU string */
    { .string = { { INFO_TYPE_STRING, { MLU_STATUS_POS_X, MLU_STATUS_POS_Y, 2, .name = "MLU" }}, INFO_STRING_MLU, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },
    
    /* entry 9, kelvin */
    { .string = { { INFO_TYPE_STRING, { WB_KELVIN_POS_X, WB_KELVIN_POS_Y, 2, .name = "Kelvin" }}, INFO_STRING_KELVIN, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM_SHADOW } },

    /* entry 10, HDR bracketing status */
    { .string = { { INFO_TYPE_STRING, { HDR_STATUS_POS_X, HDR_STATUS_POS_Y, 2, .name = "HDR" }}, INFO_STRING_HDR, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_MEDIUM } },
#endif

#if defined(CONFIG_60D)
    /* print ISO range */
    { .string = { { INFO_TYPE_STRING, { ISO_RANGE_POS_X, ISO_RANGE_POS_Y, 2, .name = "ISO Range" }}, INFO_STRING_ISO_MINMAX, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },

    /* entry 2 and 3, WB strings */
    { .string = { { INFO_TYPE_STRING, { WBS_POS_X, WBS_POS_Y, 2, .name = "WB GM" }}, INFO_STRING_WBS_GM, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_MEDIUM } },
    { .string = { { INFO_TYPE_STRING, { WBS_POS_X + 24, WBS_POS_Y, 2, .name = "WB BA" }}, INFO_STRING_WBS_BA, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_MEDIUM } },
    
    /* entry 4, battery_icon referenced as anchor */
    { .battery_icon = { { INFO_TYPE_BATTERY_ICON, { DISPLAY_BATTERY_POS_X, DISPLAY_BATTERY_POS_Y, 2, .name = "Battery Icon" }}, DISPLAY_BATTERY_LEVEL_2, DISPLAY_BATTERY_LEVEL_1 } },
    { .battery_perf = { { INFO_TYPE_BATTERY_PERF, { 86, 2, 3, INFO_ANCHOR_LEFT | INFO_ANCHOR_TOP, 4 }}, /* 0=vert,1=horizontal */ 0, /* x size */ 8, /* y size */ 8 } },
    { .string = { { INFO_TYPE_STRING, { 8, 0, 2, INFO_ANCHOR_HCENTER | INFO_ANCHOR_BOTTOM, 4, INFO_ANCHOR_HCENTER | INFO_ANCHOR_TOP }}, INFO_STRING_BATTERY_PCT, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_MEDIUM } },
    { .string = { { INFO_TYPE_STRING, { 4, 2, 2, INFO_ANCHOR_RIGHT | INFO_ANCHOR_TOP, 4 }}, INFO_STRING_BATTERY_ID, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_LARGE } },

    /* entry 8, MLU string */
    { .string = { { INFO_TYPE_STRING, { MLU_STATUS_POS_X, MLU_STATUS_POS_Y, 2, .name = "MLU" }}, INFO_STRING_MLU, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },
    
    /* entry 9, kelvin */
    { .string = { { INFO_TYPE_STRING, { WB_KELVIN_POS_X, WB_KELVIN_POS_Y, 2, .name = "Kelvin" }}, INFO_STRING_KELVIN, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM_SHADOW } },

    /* entry 10, HDR bracketing status */
    { .string = { { INFO_TYPE_STRING, { HDR_STATUS_POS_X, HDR_STATUS_POS_Y, 2, .name = "HDR" }}, INFO_STRING_HDR, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_MEDIUM } },
#endif
    
#if defined(CONFIG_600D)
    /* entry 1, max AUTO ISO */
    { .string = { { INFO_TYPE_STRING, { MAX_ISO_POS_X, MAX_ISO_POS_Y, 2, .name = "Max ISO Range"  }}, INFO_STRING_ISO_MAX, COLOR_FG_NONLV, INFO_COL_FIELD, INFO_FONT_MEDIUM } },
    
    /* entry 2 and 3, WB strings */
    { .string = { { INFO_TYPE_STRING, { WBS_GM_POS_X, WBS_GM_POS_Y, 2, .name = "WB GM" }}, INFO_STRING_WBS_GM, COLOR_FG_NONLV, INFO_COL_BG, INFO_FONT_MEDIUM } },
    { .string = { { INFO_TYPE_STRING, { WBS_POS_X, WBS_POS_Y, 2, .name = "WB BA" }}, INFO_STRING_WBS_BA, COLOR_FG_NONLV, INFO_COL_BG, INFO_FONT_MEDIUM } },
    
    /* entry 4, MLU string */
    { .string = { { INFO_TYPE_STRING, { MLU_STATUS_POS_X, MLU_STATUS_POS_Y, 2, .name = "MLU" }}, INFO_STRING_MLU, COLOR_FG_NONLV, INFO_COL_FIELD, INFO_FONT_SMALL } },
    
    /* entry 5, kelvin */
    { .string = { { INFO_TYPE_STRING, { WB_KELVIN_POS_X, WB_KELVIN_POS_Y, 2, .name = "Kelvin" }}, INFO_STRING_KELVIN, COLOR_FG_NONLV, INFO_COL_FIELD, INFO_FONT_MEDIUM_SHADOW } },
    
    /* entry 6, HDR bracketing status */
    { .string = { { INFO_TYPE_STRING, { HDR_STATUS_POS_X, HDR_STATUS_POS_Y, 2, .name = "HDR" }}, INFO_STRING_HDR, COLOR_FG_NONLV, INFO_COL_BG, INFO_FONT_MEDIUM } },
#endif

#if defined(CONFIG_650D)
    /* entry 1, max AUTO ISO */
    { .string = { { INFO_TYPE_STRING, { MAX_ISO_POS_X, MAX_ISO_POS_Y, 2, .name = "Max ISO Range"  }}, INFO_STRING_ISO_MAX, COLOR_FG_NONLV, INFO_COL_FIELD, INFO_FONT_MEDIUM } },
    
    /* entry 2 and 3, WB strings */
    { .string = { { INFO_TYPE_STRING, { WBS_GM_POS_X, WBS_GM_POS_Y, 2, .name = "WB GM" }}, INFO_STRING_WBS_GM, COLOR_FG_NONLV, INFO_COL_BG, INFO_FONT_MEDIUM } },
    { .string = { { INFO_TYPE_STRING, { WBS_POS_X, WBS_POS_Y, 2, .name = "WB BA" }}, INFO_STRING_WBS_BA, COLOR_FG_NONLV, INFO_COL_BG, INFO_FONT_MEDIUM } },
    
    /* entry 4, MLU string */
    { .string = { { INFO_TYPE_STRING, { MLU_STATUS_POS_X, MLU_STATUS_POS_Y, 2, .name = "MLU" }}, INFO_STRING_MLU, COLOR_FG_NONLV, INFO_COL_FIELD, INFO_FONT_SMALL } },
    
    /* entry 5, kelvin */
    { .string = { { INFO_TYPE_STRING, { WB_KELVIN_POS_X, WB_KELVIN_POS_Y, 2, .name = "Kelvin" }}, INFO_STRING_KELVIN, COLOR_FG_NONLV, INFO_COL_FIELD, INFO_FONT_MEDIUM_SHADOW } },
    
    /* entry 6, HDR bracketing status */
    { .string = { { INFO_TYPE_STRING, { HDR_STATUS_POS_X, HDR_STATUS_POS_Y, 2, .name = "HDR" }}, INFO_STRING_HDR, COLOR_FG_NONLV, INFO_COL_BG, INFO_FONT_MEDIUM } },
#endif
    
#if defined(CONFIG_1100D)
    /* print ISO range */
    { .string = { { INFO_TYPE_STRING, { ISO_RANGE_POS_X, ISO_RANGE_POS_Y, 2, .name = "ISO Range" }}, INFO_STRING_ISO_MINMAX, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM } },

    /* entry 2 and 3, WB strings */
    { .string = { { INFO_TYPE_STRING, { WBS_POS_X, WBS_POS_Y, 2, .name = "WB GM" }}, INFO_STRING_WBS_GM, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_MEDIUM } },
    { .string = { { INFO_TYPE_STRING, { WBS_POS_X, WBS_POS_Y + 22, 2, .name = "WB BA" }}, INFO_STRING_WBS_BA, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_MEDIUM } },
 
    /* entry 4, kelvin */
    { .string = { { INFO_TYPE_STRING, { WB_KELVIN_POS_X, WB_KELVIN_POS_Y, 2, .name = "Kelvin" }}, INFO_STRING_KELVIN, COLOR_YELLOW, INFO_COL_FIELD, INFO_FONT_MEDIUM_SHADOW } },

    /* entry 5, HDR bracketing status */
    { .string = { { INFO_TYPE_STRING, { HDR_STATUS_POS_X, HDR_STATUS_POS_Y, 2, .name = "HDR" }}, INFO_STRING_HDR, COLOR_YELLOW, INFO_COL_BG, INFO_FONT_MEDIUM } },
#endif

    { .type = INFO_TYPE_END },
};

char *info_strncpy(char *dst, char *src, uint32_t length)
{
    uint32_t pos = 0;
    
    while(pos < length)
    {
        dst[pos] = src[pos];
        if(!src[pos])
        {
            return dst;
        }
        pos++;
    }
    dst[pos] = 0;
    
    return dst;
}

uint32_t info_xml_get_element(char *config, uint32_t *start_pos, char *buf, uint32_t buf_length)
{
    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t pos = *start_pos;
    char escape_quot = 0;
    
    /* skip any whitespace */
    while(config[pos] && (config[pos] == ' ' || config[pos] == '\t' || config[pos] == '\r' || config[pos] == '\n'))
    {
        pos++;
    }
    
    /* reached the end or no starting tag found? */
    if(!config[pos] || config[pos] != '<')
    {
        strcpy(buf, "");
        return 1;
    }
    
    /* well, then this is our next tag */
    pos++;
    start = pos;
    
    while(config[pos] && (config[pos] != '>' || escape_quot))
    {
        /* ignore any tags within quotation marks, waiting for an closed quot mark of the same type */
        if(config[pos] == '"' || config[pos] == '\'')
        {
            /* nothing escaped yet? */
            if(!escape_quot)
            {
                /* set our current quotation mark type */
                escape_quot = config[pos];
            }
            else if(escape_quot == config[pos])
            {
                /* same quotation mark hit again, unset it */
                escape_quot = 0;
            }
        }
        
        /* blank out any whitespace with a real space - as long it is not in a string */
        if(!escape_quot && (config[pos] == '\t' || config[pos] == '\r' || config[pos] == '\n'))
        {
            config[pos] = ' ';
        }
        pos++;
    }

    /* reached the end or no end tag found? */
    if(!config[pos] || config[pos] != '>')
    {
        strcpy(buf, "");
        return 1;
    }
    
    /* well, then this is our end */
    end = pos - 1;
    *start_pos = pos + 1;
    
    /* empty tags are quite useless and not well-formed */
    if(end < start || (end - start + 1) >= buf_length )
    {
        strcpy(buf, "");
        return 1;
    }
    
    /* copy text */
    info_strncpy(buf, &(config[start]), end - start + 1);
    buf[end - start + 1] = '\0';
    
    return 0;
}

uint32_t info_xml_get_attribute_token(char *attribute_str, char *buf, uint32_t buf_length)
{
    uint32_t start = 0;
    uint32_t end = 0;
    uint32_t pos = 0;
    char escape_quot = 0;
    
    /* skip any character until next whitespace */
    while(attribute_str[pos] && attribute_str[pos] == ' ')
    {
        pos++;
    }
    
    /* reached the end or tag end found? */
    if(!attribute_str[pos] || attribute_str[pos] == '/')
    {
        strcpy(buf, "");
        return 1;
    }
    
    start = pos;
    
    while(attribute_str[pos] && ((attribute_str[pos] != ' ' && attribute_str[pos] != '/' && attribute_str[pos] != '=') || escape_quot ))
    {
        /* ignore any tags within quotation marks, waiting for an closed quot mark of the same type */
        if(attribute_str[pos] == '"' || attribute_str[pos] == '\'')
        {
            /* nothing escaped yet? */
            if(!escape_quot)
            {
                /* set our current quotation mark type */
                escape_quot = attribute_str[pos];
            }
            else if(escape_quot == attribute_str[pos])
            {
                /* same quotation mark hit again, unset it */
                escape_quot = 0;
            }
        }
            
        pos++;
    }

    pos--;    
    end = pos;

    if(end < pos || (end - start + 1) >= buf_length )
    {
        strcpy(buf, "");
        return 1;
    }
    
    /* copy text */
    info_strncpy(buf, &(attribute_str[start]), end - start + 1);
    buf[end - start + 1] = '\0';

    return 0;
}

/* 
 * gets element_str = "xml_token name1=value1 name2 = value2/"
 * and returns value for given attribute name
 */
uint32_t info_xml_get_attribute(char *element_str, char *attribute, char *buf, uint32_t buf_length)
{
    uint32_t pos = 0;

    /* skip any character until next whitespace to skip tag name */
    while(element_str[pos] && element_str[pos] != ' ')
    {
        pos++;
    }
    
    pos++;
    
    /* reached the end or tag end found? */
    if(!element_str[pos] || element_str[pos] == '/')
    {
        strcpy(buf, "");
        return 1;
    }
        
    /* do this until the end was reached */
    while(1)
    {
        char attribute_token[32];
        char value_token[32];
        
        /* skip until next non-whitespace */
        while(element_str[pos] && element_str[pos] == ' ')
        {
            pos++;
        }
        
        /* reached the end or tag end found? */
        if(!element_str[pos] || element_str[pos] == '/')
        {
            strcpy(buf, "");
            return 1;
        }
        
        if(info_xml_get_attribute_token(&(element_str[pos]), attribute_token, sizeof(attribute_token)))
        {
            strcpy(buf, "");
            return 1;
        }

        pos += strlen(attribute_token);
        
        /* skip " = " between attribute and value */
        while(element_str[pos] && (element_str[pos] == ' ' || element_str[pos] == '='))
        {
            pos++;
        }
        
        /* reached the end? */
        if(!element_str[pos])
        {
            strcpy(buf, "");
            return 1;
        }
        
        /* now get the value of this attribute */
        if(info_xml_get_attribute_token(&(element_str[pos]), value_token, sizeof(value_token)))
        {
            strcpy(buf, "");
            return 1;
        }

        /* if this was the token we looked for, return content */
        if(!strcmp(attribute, attribute_token))
        {
            /* trim quotes */
            if(value_token[0] == '"')
            {
                uint32_t len = strlen(value_token);
                info_strncpy(value_token, &(value_token[1]), len - 2);
            }
            
            info_strncpy(buf, value_token, buf_length);
            return 0;
        }
        
        pos += strlen(value_token);
    }
 
    return 1;
}

uint32_t info_xml_parse_pos(info_elem_t *config, char *config_str)
{
    char buf[32];
    
    /* all element have x/y etc */
    if(!info_xml_get_attribute(config_str, "x", buf, sizeof(buf)))
    {
        config->hdr.pos.x = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "y", buf, sizeof(buf)))
    {
        config->hdr.pos.y = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "z", buf, sizeof(buf)))
    {
        config->hdr.pos.z = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "w", buf, sizeof(buf)))
    {
        config->hdr.pos.w = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "h", buf, sizeof(buf)))
    {
        config->hdr.pos.h = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "anchor_flags", buf, sizeof(buf)))
    {
        config->hdr.pos.anchor_flags = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "anchor", buf, sizeof(buf)))
    {
        config->hdr.pos.anchor = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "anchor_flags_self", buf, sizeof(buf)))
    {
        config->hdr.pos.anchor_flags_self = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "user_disable", buf, sizeof(buf)))
    {
        config->hdr.pos.user_disable = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "name", buf, sizeof(buf)))
    {
        info_strncpy(config->hdr.pos.name, buf, sizeof(config->hdr.pos.name));
    }
    
    return 0;
}


uint32_t info_xml_parse_string(info_elem_t *config, char *config_str)
{
    char buf[32];
    
    uint32_t ret = info_xml_parse_pos(config, config_str);
    
    if(ret)
    {
        return ret;
    }
    
    config->type = INFO_TYPE_STRING;
    
    if(!info_xml_get_attribute(config_str, "string_type", buf, sizeof(buf)))
    {
        config->string.string_type = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "fgcolor", buf, sizeof(buf)))
    {
        config->string.fgcolor = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "bgcolor", buf, sizeof(buf)))
    {
        config->string.bgcolor = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "font_type", buf, sizeof(buf)))
    {
        config->string.font_type = atoi(buf);
    }
    
    return 0;
}

uint32_t info_xml_parse_fill(info_elem_t *config, char *config_str)
{
    char buf[32];
    
    uint32_t ret = info_xml_parse_pos(config, config_str);
    
    if(ret)
    {
        return ret;
    }
    
    config->type = INFO_TYPE_FILL;
    
    if(!info_xml_get_attribute(config_str, "color", buf, sizeof(buf)))
    {
        config->fill.color = atoi(buf);
    }
    
    return 0;
}

uint32_t info_xml_parse_battery_icon(info_elem_t *config, char *config_str)
{
    char buf[32];
    
    uint32_t ret = info_xml_parse_pos(config, config_str);
    
    if(ret)
    {
        return ret;
    }
    
    config->type = INFO_TYPE_BATTERY_ICON;
    
    if(!info_xml_get_attribute(config_str, "pct_red", buf, sizeof(buf)))
    {
        config->battery_icon.pct_red = atoi(buf);
    }
    
    if(!info_xml_get_attribute(config_str, "pct_yellow", buf, sizeof(buf)))
    {
        config->battery_icon.pct_yellow = atoi(buf);
    }
    
    
    return 0;
}

uint32_t info_xml_parse_battery_perf(info_elem_t *config, char *config_str)
{
    char buf[32];
    
    uint32_t ret = info_xml_parse_pos(config, config_str);
    
    if(ret)
    {
        return ret;
    }
    
    config->type = INFO_TYPE_BATTERY_PERF;
    
    if(!info_xml_get_attribute(config_str, "horizontal", buf, sizeof(buf)))
    {
        config->battery_perf.horizontal = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "width", buf, sizeof(buf)))
    {
        config->battery_perf.width = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "height", buf, sizeof(buf)))
    {
        config->battery_perf.height = atoi(buf);
    }
    
    return 0;
}

uint32_t info_xml_parse_icon(info_elem_t *config, char *config_str)
{
    char buf[32];
    
    uint32_t ret = info_xml_parse_pos(config, config_str);
    
    if(ret)
    {
        return ret;
    }
    
    config->type = INFO_TYPE_ICON;
    
    if(!info_xml_get_attribute(config_str, "fgcolor", buf, sizeof(buf)))
    {
        config->icon.fgcolor = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "bgcolor", buf, sizeof(buf)))
    {
        config->icon.bgcolor = atoi(buf);
    }
    if(!info_xml_get_attribute(config_str, "filename", buf, sizeof(buf)))
    {
        info_strncpy(config->icon.filename, buf, sizeof(config->icon.filename));
    }
    
    return 0;
}

uint32_t info_load_config(char *filename)
{
	uint32_t allocated_elements = 32;
    uint32_t size = 0;
    uint32_t done = 0;
    uint32_t config_string_pos = 0;
    uint32_t config_element_pos = 0;
    char xml_element[256];
    char attr_buf[64];

    if( FIO_GetFileSize( filename, &size ) != 0 )
    {
        return 1;
	}

	char *xml_config = alloc_dma_memory(size + 1);
    xml_config[size] = '\0';
	if (!xml_config)
	{
        return 1;
	}

	if ((unsigned)read_file(filename, xml_config, size)!=size)
	{
        free_dma_memory(xml_config);
        return 1;
	}
    
    /* read first xml token */
    if(info_xml_get_element(xml_config, &config_string_pos, xml_element, sizeof(xml_element)))
    {
        free_dma_memory(xml_config);
        return 1;
    }
    
    /* should be a flexinfo */
    if(strncmp(xml_element, "flexinfo", 8))
    {
        free_dma_memory(xml_config);
        return 1;
    }
    
    /* attribute tells how many elements are allocated */
    if(!info_xml_get_attribute(xml_element, "elements", attr_buf, sizeof(attr_buf)))
    {
        allocated_elements = atoi(attr_buf) + 3;
    }
    
    /* allocate the new config */
    info_elem_t *new_config = (info_elem_t *)alloc_dma_memory(allocated_elements*sizeof(info_elem_t));
    memset(new_config, 0, allocated_elements*sizeof(info_elem_t));
    
    /* first is config header */
    new_config[config_element_pos].type = INFO_TYPE_CONFIG;
    
    /* config/root element has one configurable attribute. but may be omitted */
    if(!info_xml_get_attribute(xml_element, "name", attr_buf, sizeof(attr_buf)))
    {
        info_strncpy(new_config[config_element_pos].config.name, attr_buf, sizeof(new_config[config_element_pos].config.name));
    }
    
    config_element_pos++;
    
    do
    {
        uint32_t ret = 1;
        info_elem_t *element = &(new_config[config_element_pos]);

        /* read next element */
        if(info_xml_get_element(xml_config, &config_string_pos, xml_element, sizeof(xml_element)))
        {
            free_dma_memory(new_config);
            free_dma_memory(xml_config);
            return 1;
        }
        
        if(!strncmp(xml_element, "string", 6))
        {
            ret = info_xml_parse_string(element, xml_element);
        }
        if(!strncmp(xml_element, "fill", 4))
        {
            ret = info_xml_parse_fill(element, xml_element);
        }
        if(!strncmp(xml_element, "battery_icon", 12))
        {
            ret = info_xml_parse_battery_icon(element, xml_element);
        }
        if(!strncmp(xml_element, "battery_perf", 12))
        {
            ret = info_xml_parse_battery_perf(element, xml_element);
        }
        if(!strncmp(xml_element, "icon", 4))
        {
            ret = info_xml_parse_icon(element, xml_element);
        }
        if(!strncmp(xml_element, "/flexinfo", 9))
        {
            element->type = INFO_TYPE_END;
            done = 1;
            ret = 0;
        }
        
        config_element_pos++;
        if(ret || allocated_elements < config_element_pos)
        {
            free_dma_memory(new_config);
            free_dma_memory(xml_config);
            return ret;
        }
        
    } while (!done);

    free_dma_memory(xml_config);    
    free_dma_memory(new_config);    
    memcpy(info_config, new_config, config_element_pos * sizeof(info_elem_t));
    return 0;
}

uint32_t info_save_config(info_elem_t *config, char *file)
{
    uint32_t pos = 1;
    uint32_t elements = 0;
    
    while(config[elements].type != INFO_TYPE_END)
    {
        elements++;
    }
        
    FILE* f = FIO_CreateFileEx(file);
    if(!f)
    {
        return 1;
    }
    
    my_fprintf(f, "<flexinfo elements=%d>\n", elements - 1);
    
    while(config[pos].type != INFO_TYPE_END)
    {
        my_fprintf(f, "    ");
        switch(config[pos].type)
        {
            case INFO_TYPE_STRING:
                my_fprintf(f, "<string ");
                break;
            case INFO_TYPE_BATTERY_ICON:
                my_fprintf(f, "<battery_icon ");
                break;
            case INFO_TYPE_BATTERY_PERF:
                my_fprintf(f, "<battery_perf ");
                break;
            case INFO_TYPE_FILL:
                my_fprintf(f, "<fill ");
                break;
            case INFO_TYPE_ICON:
                my_fprintf(f, "<icon ");
                break;
        }
        
        /* dump position field data */
        my_fprintf(f, "name=\"%s\" ", config[pos].hdr.pos.name);
        if(config[pos].hdr.pos.x)
        {
            my_fprintf(f, "x=%d ", config[pos].hdr.pos.x);
        }
        if(config[pos].hdr.pos.y)
        {
            my_fprintf(f, "y=%d ", config[pos].hdr.pos.y);
        }
        if(config[pos].hdr.pos.z)
        {
            my_fprintf(f, "z=%d ", config[pos].hdr.pos.z);
        }
        if(config[pos].hdr.pos.w)
        {
            my_fprintf(f, "w=%d ", config[pos].hdr.pos.w);
        }
        if(config[pos].hdr.pos.h)
        {
            my_fprintf(f, "h=%d ", config[pos].hdr.pos.h);
        }
        if(config[pos].hdr.pos.anchor_flags)
        {
            my_fprintf(f, "anchor_flags=%d ", config[pos].hdr.pos.anchor_flags);
        }
        if(config[pos].hdr.pos.anchor)
        {
            my_fprintf(f, "anchor=%d ", config[pos].hdr.pos.anchor);
        }
        if(config[pos].hdr.pos.anchor_flags_self)
        {
            my_fprintf(f, "anchor_flags_self=%d ", config[pos].hdr.pos.anchor_flags_self);
        }
        if(config[pos].hdr.pos.user_disable)
        {
            my_fprintf(f, "user_disable=%d ", config[pos].hdr.pos.user_disable);
        }
        
        switch(config[pos].type)
        {
            case INFO_TYPE_STRING:
                if(config[pos].string.string_type)
                {
                    my_fprintf(f, "string_type=%d ", config[pos].string.string_type);
                }
                if(config[pos].string.fgcolor)
                {
                    my_fprintf(f, "fgcolor=%d ", config[pos].string.fgcolor);
                }
                if(config[pos].string.bgcolor)
                {
                    my_fprintf(f, "bgcolor=%d ", config[pos].string.bgcolor);
                }
                if(config[pos].string.font_type)
                {
                    my_fprintf(f, "font_type=%d ", config[pos].string.font_type);
                }
                break;
                
            case INFO_TYPE_BATTERY_ICON:
                if(config[pos].battery_icon.pct_red)
                {
                    my_fprintf(f, "pct_red=%d ", config[pos].battery_icon.pct_red);
                }
                if(config[pos].battery_icon.pct_yellow)
                {
                    my_fprintf(f, "pct_yellow=%d ", config[pos].battery_icon.pct_yellow);
                }
                break;
                
            case INFO_TYPE_BATTERY_PERF:
                if(config[pos].battery_perf.horizontal)
                {
                    my_fprintf(f, "horizontal=%d ", config[pos].battery_perf.horizontal);
                }
                if(config[pos].battery_perf.width)
                {
                    my_fprintf(f, "width=%d ", config[pos].battery_perf.width);
                }
                if(config[pos].battery_perf.height)
                {
                    my_fprintf(f, "height=%d ", config[pos].battery_perf.height);
                }
                break;
                
            case INFO_TYPE_FILL:
                if(config[pos].fill.color)
                {
                    my_fprintf(f, "color=%d ", config[pos].fill.color);
                }
                break;
                
            case INFO_TYPE_ICON:
                my_fprintf(f, "filename=\"%s\"", config[pos].icon.filename);
                if(config[pos].icon.fgcolor)
                {
                    my_fprintf(f, "fgcolor=%d ", config[pos].icon.fgcolor);
                }
                if(config[pos].icon.bgcolor)
                {
                    my_fprintf(f, "bgcolor=%d ", config[pos].icon.bgcolor);
                }
                break;
            break;
        }
        my_fprintf(f, "/>\n");
        pos++;
    }
    
    my_fprintf(f, "</flexinfo>\n");
    FIO_CloseFile(f);
    
    return 0;
}

/* ********************************************************************************** */


uint32_t info_get_string(char *buffer, uint32_t maxsize, uint32_t string_type)
{
    strcpy(buffer, "");

    switch(string_type)
    {
        case INFO_STRING_ISO:
        {
            snprintf(buffer, maxsize, "(ISO)");
            break;
        }
        case INFO_STRING_ISO_MIN:
        {
            if (lens_info.raw_iso != 0)
            {
                return 1;
            }
            snprintf(buffer, maxsize, "MIN:%d",raw2iso(auto_iso_range >> 8));
            break;
        }
        case INFO_STRING_ISO_MAX:
        {
            if (lens_info.raw_iso != 0)
            {
                return 1;
            }
            snprintf(buffer, maxsize, "MAX:%d",raw2iso(auto_iso_range & 0xFF));
            break;
        }
        case INFO_STRING_ISO_MINMAX:
        {
            if (lens_info.raw_iso != 0)
            {
                return 1;
            }
            snprintf(buffer, maxsize, "[%d-%d]", MAX((get_htp() ? 200 : 100), raw2iso(auto_iso_range >> 8)), raw2iso(auto_iso_range & 0xFF));
            break;
        }
        case INFO_STRING_KELVIN:
        {
            if (lens_info.wb_mode != WB_KELVIN)
            {
                return 1;
            }
            snprintf(buffer, maxsize, "%5d", lens_info.kelvin);
            break;
        }
        case INFO_STRING_WBS_BA:
        {
            int ba = lens_info.wbs_ba;
            if (ba == 0)
            {
                return 1;
            }
            snprintf(buffer, maxsize, "%s%d", ba > 0 ? "A" : "B", ABS(ba));
            break;
        }
        case INFO_STRING_WBS_GM:
        {
            int gm = lens_info.wbs_gm;
            if (gm == 0)
            {
                return 1;
            }
            snprintf(buffer, maxsize, "%s%d", gm > 0 ? "G" : "M", ABS(gm));
            break;
        }
        case INFO_STRING_DATE_DDMMYYYY:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%2d.%2d.%4d", now.tm_mday,(now.tm_mon+1),(now.tm_year+1900));
            break;
        }
        case INFO_STRING_DATE_YYYYMMDD:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%4d.%2d.%2d", (now.tm_year+1900),(now.tm_mon+1),now.tm_mday);
            break;
        }
        case INFO_STRING_CAM_DATE:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            if (date_format == DATE_FORMAT_YYYY_MM_DD)
              snprintf(buffer, maxsize, "%4d.%02d.%02d", (now.tm_year+1900),(now.tm_mon+1),now.tm_mday);
            else if (date_format == DATE_FORMAT_MM_DD_YYYY)  
              snprintf(buffer, maxsize, "%02d.%02d.%4d", (now.tm_mon+1),now.tm_mday,(now.tm_year+1900));
            else  
              snprintf(buffer, maxsize, "%02d.%02d.%4d", now.tm_mday,(now.tm_mon+1),(now.tm_year+1900));
            break;
        }
        case INFO_STRING_DATE_MM:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%2d", (now.tm_mon+1));
            break;
        }
        case INFO_STRING_DATE_DD:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%2d", now.tm_mday);
            break;
        }
        case INFO_STRING_DATE_YY:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%2d", now.tm_year % 100);
            break;
        }
        case INFO_STRING_DATE_YYYY:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%4d", (now.tm_year+1900));
            break;
        }
        case INFO_STRING_TIME:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%02d:%02d", now.tm_hour, now.tm_min);
            break;
        }
        case INFO_STRING_TIME_HH12:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%02d", now.tm_hour % 13);
            break;
        }
        case INFO_STRING_TIME_HH24:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%02d", now.tm_hour);
            break;
        }
        case INFO_STRING_TIME_MM:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%02d", now.tm_min);
            break;
        }
        case INFO_STRING_TIME_SS:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%02d", now.tm_sec);
            break;
        }
        case INFO_STRING_TIME_AMPM:
        {
            struct tm now;
            LoadCalendarFromRTC(&now);
            snprintf(buffer, maxsize, "%s", (now.tm_hour > 12) ? "PM" : "AM");
            break;
        }
        case INFO_STRING_ARTIST:
        {
            snprintf(buffer, maxsize, "%s", artist_name);
            trim(buffer);
            break;
        }
        case INFO_STRING_COPYRIGHT:
        {
            snprintf(buffer, maxsize, "%s", copyright_info);
            trim(buffer);
            break;
        }
        case INFO_STRING_LENS:
        {
            snprintf(buffer, maxsize, "%s", lens_info.name);
            break;
        }
        case INFO_STRING_BUILD:
        {
            snprintf(buffer, maxsize, "%s", build_version);
            break;
        }
#ifdef CONFIG_BATTERY_INFO
        case INFO_STRING_BATTERY_PCT:
        {
            snprintf(buffer, maxsize, "%d%%%%", GetBatteryLevel());
            break;
        }
        case INFO_STRING_BATTERY_ID:
        {
            if (GetBatteryHist() == 0)
            {
                return 1;
            }
            snprintf(buffer, maxsize, "%d", GetBatteryHist());
            break;
        }
#else
        case INFO_STRING_BATTERY_PCT:
        case INFO_STRING_BATTERY_ID:
        {
            /* feature not enabled/available */
            return 1;
            break;
        }
#endif
        case INFO_STRING_CARD_LABEL_A:
            snprintf(buffer, 11, "%s", GetCardLabel('A'));
            buffer[12]='\0';
            trim(buffer);
            break;
        case INFO_STRING_CARD_LABEL_B:
            snprintf(buffer, 11, "%s", GetCardLabel('B'));
            buffer[12]='\0';
            trim(buffer);
            break;
        case INFO_STRING_CARD_MAKER_A:
            snprintf(buffer, maxsize, "%s", GetCardMaker('A'));
            trim(buffer);
            break;
        case INFO_STRING_CARD_MAKER_B:
            snprintf(buffer, maxsize, "%s", GetCardMaker('B'));
            trim(buffer);
            break;
        case INFO_STRING_CARD_MODEL_A:
            snprintf(buffer, maxsize, "%s", GetCardModel('A'));
            trim(buffer);
            break;
        case INFO_STRING_CARD_MODEL_B:
            snprintf(buffer, maxsize, "%s", GetCardModel('B'));
            trim(buffer);
            break;
        case INFO_STRING_CARD_SPACE_A:
        case INFO_STRING_CARD_SPACE_B:
        case INFO_STRING_CARD_FILES_A:
        case INFO_STRING_CARD_FILES_B:
            snprintf(buffer, maxsize, "(n/a)");
            break;
            
        case INFO_STRING_PICTURES:
        {
            if (avail_shot>9999) // we can write 5 digits if necessary
                snprintf(buffer, maxsize, "[%5d]", avail_shot);
#ifdef AVAIL_SHOT_WORKAROUND
            else if (avail_shot>999)
                snprintf(buffer, maxsize, "[%4d]", avail_shot);
#endif
            else
                return 1;            
            break;
        }
        case INFO_STRING_MLU:
        {
            if (get_mlu() == 0)
            {
                return 1;
            }
            snprintf(buffer, maxsize, "MLU");
            break;
        }
        case INFO_STRING_HDR:
#ifdef FEATURE_HDR_BRACKETING
        {
            extern int hdr_enabled, hdr_steps, hdr_stepsize;
            if (!hdr_enabled)
            {
                return 1;
            }
            snprintf(buffer, maxsize,
                "HDR %Xx%d%sEV",
                hdr_steps == 1 ? 10 : hdr_steps, // trick: when steps=1 (auto) it will display A :)
                hdr_stepsize / 8,
                ((hdr_stepsize/4) % 2) ? ".5" : "");
            break;
        }
#else
        {
            /* feature not enabled/available */
            return 1;
            break;
        }
#endif
        /* empty string */
        case INFO_STRING_NONE:
        {
            break;
        }
        /* error */
        default:
            return 1;
    }

    return 0;
}

uint32_t info_measure_string(char *string, uint32_t font_type, int32_t *width, int32_t *height)
{
    uint32_t font = 0;
    switch(font_type)
    {
        case INFO_FONT_SMALL:
        case INFO_FONT_SMALL_SHADOW:
            font = FONT_SMALL;
            break;
        case INFO_FONT_MEDIUM:
        case INFO_FONT_MEDIUM_SHADOW:
            font = FONT_MED;
            break;
        case INFO_FONT_LARGE:
        case INFO_FONT_LARGE_SHADOW:
            font = FONT_LARGE;
            break;
        case INFO_FONT_CANON:
        {
            font = 0;
            *width = 0;
            for (char* c = string; *c; c++)
                *width += bfnt_char_get_width(*c);
            *height = 40;
            break;
        }
        /* error */
        default:
            return 1;
    }
    
    if(font)
    {
        *width = fontspec_font(font)->width * strlen(string);
        *height = fontspec_font(font)->height;
    }

    return 0;
}

uint32_t info_get_anchor_offset(info_elem_t *element, uint32_t flags, int32_t *offset_x, int32_t *offset_y)
{
    switch(flags & INFO_ANCHOR_H_MASK)
    {
        case INFO_ANCHOR_LEFT:
            *offset_x = 0;
            break;
        case INFO_ANCHOR_HCENTER:
            *offset_x = element->hdr.pos.w / 2;
            break;
        case INFO_ANCHOR_RIGHT:
            *offset_x = element->hdr.pos.w;
            break;
        default:
            *offset_x = 0;
            break;
    }

    switch(flags & INFO_ANCHOR_V_MASK)
    {
        case INFO_ANCHOR_TOP:
            *offset_y = 0;
            break;
        case INFO_ANCHOR_VCENTER:
            *offset_y = element->hdr.pos.h / 2;
            break;
        case INFO_ANCHOR_BOTTOM:
            *offset_y = element->hdr.pos.h;
            break;
        default:
            *offset_y = 0;
            break;
    }

    return 0;
}

uint32_t info_get_absolute(info_elem_t *config, info_elem_t *element)
{
    int32_t offset_x = 0;
    int32_t offset_y = 0;
    
    /* in case of absolute positioning, this is the absolute pos else it is the offset from the anchor */
    element->hdr.pos.abs_x = element->hdr.pos.x;
    element->hdr.pos.abs_y = element->hdr.pos.y;

    /* if the element is relatively positioned to some other element, we have to look it up */
    if(element->hdr.pos.anchor != 0)
    {
        /* determine position from referenced element identified by 'anchor' and update pos_x, pos_y (they contain the offset) */
        info_elem_t *anchor = &(config[element->hdr.pos.anchor]);
        
        if(!anchor->hdr.pos.shown)
        {
            element->hdr.pos.shown = 0;
        }
        
        /* calculate anchor offset from top left of anchor item */
        info_get_anchor_offset(anchor, element->hdr.pos.anchor_flags, &offset_x, &offset_y);
        
        /* if any coordinate was specified to be relative (anchored), update it */
        if(element->hdr.pos.anchor_flags & INFO_ANCHOR_H_MASK)
        {
            element->hdr.pos.abs_x += anchor->hdr.pos.abs_x + offset_x;
        }
        if(element->hdr.pos.anchor_flags & INFO_ANCHOR_V_MASK)
        {
            element->hdr.pos.abs_y += anchor->hdr.pos.abs_y + offset_y;
        }
    }

    /* translate position by own anchor offset */
    info_get_anchor_offset(element, element->hdr.pos.anchor_flags_self, &offset_x, &offset_y);
    
    /* if any coordinate was specified to be relative (anchored), update it */
    if(element->hdr.pos.anchor_flags_self & INFO_ANCHOR_H_MASK)
    {
        element->hdr.pos.abs_x -= offset_x;
    }
    if(element->hdr.pos.anchor_flags_self & INFO_ANCHOR_V_MASK)
    {
        element->hdr.pos.abs_y -= offset_y;
    }
    
    return 0;
}
    
uint32_t info_print_string(info_elem_t *config, info_elem_string_t *element, uint32_t run_type)
{
    char str[BUF_SIZE];
    
    /* get absolute position of this element */
    info_get_absolute(config, (info_elem_t *)element);
    
    int pos_x = element->hdr.pos.abs_x;
    int pos_y = element->hdr.pos.abs_y;

    /* nothing to show? mark as not shown */
    if(info_get_string(str, BUF_SIZE, element->string_type))
    {
        element->hdr.pos.shown = 0;
    }

    /* anchor not shown or nothing to print */
    if(!element->hdr.pos.shown)
    {
        return 1;
    }

    /* update the width/height */
    info_measure_string(str, element->font_type, &element->hdr.pos.w, &element->hdr.pos.h);

    /* ToDo: make defineable */
    int col_bg = bmp_getpixel(10,1);
    int col_field = bmp_getpixel(615,375);
    uint32_t fgcolor = element->fgcolor;
    uint32_t bgcolor = element->bgcolor;
    uint32_t fnt;

    /* look up special colors. ToDo: optimize */
    if(bgcolor == INFO_COL_BG)
    {
        bgcolor = col_bg;
    }
    if(bgcolor == INFO_COL_FIELD)
    {
        bgcolor = col_field;
    }
    if(fgcolor == INFO_COL_BG)
    {
        fgcolor = col_bg;
    }
    if(fgcolor == INFO_COL_FIELD)
    {
        fgcolor = col_field;
    }

    /* print string if this was not just a pre-pass run */
    if(run_type == INFO_PRINT)
    {
        switch(element->font_type)
        {
            case INFO_FONT_SMALL:
                fnt = FONT(FONT_SMALL, fgcolor, bgcolor);
                bmp_printf(fnt, pos_x, pos_y, str);
                break;
            case INFO_FONT_MEDIUM:
                fnt = FONT(FONT_MED, fgcolor, bgcolor);
                bmp_printf(fnt, pos_x, pos_y, str);
                break;
            case INFO_FONT_LARGE:
                fnt = FONT(FONT_LARGE, fgcolor, bgcolor);
                bmp_printf(fnt, pos_x, pos_y, str);
                break;
            case INFO_FONT_SMALL_SHADOW:
                fnt = SHADOW_FONT(FONT(FONT_SMALL, fgcolor, bgcolor));
                bmp_printf(fnt, pos_x, pos_y, str);
                break;
            case INFO_FONT_MEDIUM_SHADOW:
                fnt = SHADOW_FONT(FONT(FONT_MED, fgcolor, bgcolor));
                bmp_printf(fnt, pos_x, pos_y, str);
                break;
            case INFO_FONT_LARGE_SHADOW:
                fnt = SHADOW_FONT(FONT(FONT_LARGE, fgcolor, bgcolor));
                bmp_printf(fnt, pos_x, pos_y, str);
                break;
            case INFO_FONT_CANON:
                bfnt_puts(str, pos_x, pos_y, fgcolor, bgcolor);
                break;
            /* error */
            default:
                return 1;
        }
    }

    return 0;
}

uint32_t info_print_fill(info_elem_t *config, info_elem_fill_t *element, uint32_t run_type)
{
    /* get absolute position of this element */
    info_get_absolute(config, (info_elem_t *)element);
    
    /* anchor not shown or nothing to print */
    if(!element->hdr.pos.shown)
    {
        return 1;
    }

    bmp_fill(element->color, element->hdr.pos.abs_x, element->hdr.pos.abs_y, element->hdr.pos.w, element->hdr.pos.h);
    return 0;
}

uint32_t info_print_icon(info_elem_t *config, info_elem_icon_t *element, uint32_t run_type)
{
    /* get absolute position of this element */
    info_get_absolute(config, (info_elem_t *)element);
    
    /* anchor not shown or nothing to print */
    if(!element->hdr.pos.shown)
    {
        return 1;
    }
    
    return 0;
}

uint32_t info_print_battery_perf(info_elem_t *config, info_elem_battery_perf_t *element, uint32_t run_type)
{
    /* get absolute position of this element */
    info_get_absolute(config, (info_elem_t *)element);
    
    /* anchor not shown or nothing to print */
    if(!element->hdr.pos.shown)
    {
        return 1;
    }

    int pos_x = element->hdr.pos.abs_x;
    int pos_y = element->hdr.pos.abs_y;
    int width = element->width;
    int height = element->height;    

    if(element->horizontal)
    {
        element->hdr.pos.w = 3 * width + 8;
        element->hdr.pos.h = height;
    }
    else
    {
        element->hdr.pos.w = width;
        element->hdr.pos.h = 3 * height + 4;
    }
    
#ifdef CONFIG_BATTERY_INFO
    if(run_type == INFO_PRINT)
    {
        int perf = GetBatteryPerformance();
        if(element->horizontal)
        {
            bmp_fill((perf<1 ? COLOR_GRAY50 : COLOR_GREEN2),pos_x,pos_y,width,height);
            bmp_fill((perf<2 ? COLOR_GRAY50 : COLOR_GREEN2),pos_x+4+width,pos_y,width,height);
            bmp_fill((perf<3 ? COLOR_GRAY50 : COLOR_GREEN2),pos_x+8+2*width,pos_y,width,height);
        }
        else
        {
            bmp_fill((perf<3 ? COLOR_GRAY50 : COLOR_GREEN2),pos_x,pos_y,width,height);
            bmp_fill((perf<2 ? COLOR_GRAY50 : COLOR_GREEN2),pos_x,pos_y+2+height,width,height);
            bmp_fill((perf<1 ? COLOR_GRAY50 : COLOR_GREEN2),pos_x,pos_y+4+2*height,width,height);
        }
    }
#else
    /* feature n/a, paint it red */
    bmp_fill(COLOR_RED, element->hdr.pos.abs_x, element->hdr.pos.abs_y, element->hdr.pos.w, element->hdr.pos.h);
#endif
    return 0;
}

uint32_t info_print_battery_icon(info_elem_t *config, info_elem_battery_icon_t *element, uint32_t run_type)
{
    element->hdr.pos.w = 96;
    element->hdr.pos.h = 32;
    
    /* get absolute position of this element */
    info_get_absolute(config, (info_elem_t *)element);

    /* anchor not shown or nothing to print */
    if(!element->hdr.pos.shown)
    {
        return 1;
    }

#if 0 // fights with Canon icon; do not draw, but keep it for positioning the other elements

#ifdef CONFIG_BATTERY_INFO
    int batlev = GetBatteryLevel();
    int col_field = bmp_getpixel(615,455);
    
    int pos_x = element->hdr.pos.abs_x;
    int pos_y = element->hdr.pos.abs_y;
    
    if(run_type == INFO_PRINT)
    {
        uint batcol = 0;
        uint batfil = 0;
        bmp_fill(col_field,pos_x-4,pos_y+14,96,32); // clear the Canon battery icon
        
        if (batlev <= (int)element->pct_red)
        {
            batcol = COLOR_RED;
        }
        else
        {
            batcol = COLOR_WHITE;
        }
        
        bmp_fill(batcol,pos_x+10,pos_y,72,32); // draw the new battery icon
        bmp_fill(batcol,pos_x,pos_y+8,12,16);
        bmp_fill(col_field,pos_x+14,pos_y+4,64,24);
        
        if (batlev <= (int)element->pct_red)
        {
            batcol = COLOR_RED;
        }
        else if (batlev <= (int)element->pct_yellow)
        {
            batcol = COLOR_YELLOW;
        }
        else
        {
            batcol = COLOR_GREEN2;
        }
        
        batfil = batlev*56/100;
        bmp_fill(batcol,pos_x+18+56-batfil,pos_y+8,batfil,16);
    }
#else
    /* feature n/a, paint it red */
    bmp_fill(COLOR_RED, element->hdr.pos.abs_x, element->hdr.pos.abs_y, element->hdr.pos.w, element->hdr.pos.h);
#endif

#endif
    return 0;
}


uint32_t info_get_next_z(info_elem_t *config, uint32_t current)
{
    uint32_t pos = 0;
    uint32_t next = INFO_Z_END;

    while(config[pos].type != INFO_TYPE_END)
    {
        uint32_t z = config[pos].hdr.pos.z;
        
        if(z >= current && z < next )
        {
            next = z;
        }
        pos++;
    }

    return next;
}

uint32_t info_print_element(info_elem_t *config, info_elem_t *element, uint32_t run_type)
{
    switch(element->type)
    {
        case INFO_TYPE_STRING:
            return info_print_string(config, (info_elem_string_t *)element, run_type);
        case INFO_TYPE_BATTERY_ICON:
            return info_print_battery_icon(config, (info_elem_battery_icon_t *)element, run_type);
        case INFO_TYPE_BATTERY_PERF:
            return info_print_battery_perf(config, (info_elem_battery_perf_t *)element, run_type);
        case INFO_TYPE_FILL:
            return info_print_fill(config, (info_elem_fill_t *)element, run_type);
        case INFO_TYPE_ICON:
            return info_print_icon(config, (info_elem_icon_t *)element, run_type);
    }
    
    return 1;
}

uint32_t info_checksum_element(info_elem_t *config)
{
    uint32_t checksum = 0x234AE10A;
    
    for(int y_pos = 0; y_pos < config->hdr.pos.h; y_pos++)
    {
        for(int x_pos = 0; x_pos < config->hdr.pos.w; x_pos++)
        {
            uint32_t value = bmp_getpixel(config->hdr.pos.abs_x + x_pos, config->hdr.pos.abs_x + y_pos);
            checksum ^= value;
            checksum += value;
            checksum = (checksum >> 31) | (checksum << 1);
        }
    }
    
    return checksum;
}

uint32_t info_update_element_checksum(info_elem_t *config)
{
    config->hdr.pos.checksum = info_checksum_element(config);
    return 0;
}

uint32_t info_update_checkums(info_elem_t *config)
{
    uint32_t pos = 1;
    
    while(config[pos].type != INFO_TYPE_END)
    { 
        info_checksum_element(&(config[pos]));
        pos++;
    }
    
    return 0;
}

uint32_t info_checkums_valid(info_elem_t *config)
{
    uint32_t ret = 1;
    uint32_t pos = 1;
    
    while(config[pos].type != INFO_TYPE_END)
    { 
        if(info_checksum_element(&(config[pos])) != config[pos].hdr.pos.checksum)
        {
            ret = 0;
        }
        pos++;
    }
    
    return ret;
}

uint32_t info_print_config(info_elem_t *config)
{
    uint32_t pos = 1;
    int32_t z = 0;

    while(config[pos].type != INFO_TYPE_END)
    {
        /* by default all are set as shown */
        config[pos].hdr.pos.shown = !config[pos].hdr.pos.user_disable;
        pos++;
    }
    
    pos = 1;
    while(config[pos].type != INFO_TYPE_END)
    { 
        /* but check if the elements are invisible. this updates above flag and ensures that elements are only drawn if the anchor (that must come first) is shown */
        info_print_element(config, &(config[pos]), INFO_PRERUN);
        pos++;
    }

    z = info_get_next_z(config, 0);
    while(z != INFO_Z_END)
    {
        pos = 1;
        while(config[pos].type != INFO_TYPE_END)
        {
            if(z == config[pos].hdr.pos.z)
            {
                info_print_element(config, &(config[pos]), INFO_PRINT);
                
                /* if it was shown, update redraw counter */
                if(config[pos].hdr.pos.shown)
                {
                    config[pos].hdr.pos.redraws++;
                }
                
                /* paint border around item and some label when the item was selected */
                uint32_t selected_item = config[0].config.selected_item;
                
                if(config[0].config.show_boundaries || selected_item == pos || config[selected_item].hdr.pos.anchor == pos)
                {
                    int color = COLOR_RED;
                    
                    /* the currently selected item is drawn green and the anchor target is drawn blue */
                    if(selected_item == pos)
                    {
                        color = COLOR_GREEN1;
                    }
                    else if(config[selected_item].hdr.pos.anchor == pos)
                    {
                        color = COLOR_BLUE;
                    }
                    
                    /* very small sized elements will get drawn as blocks */
                    if(config[pos].hdr.pos.w > 4 && config[pos].hdr.pos.h > 4)
                    {
                        draw_line(config[pos].hdr.pos.abs_x, config[pos].hdr.pos.abs_y, config[pos].hdr.pos.abs_x + config[pos].hdr.pos.w, config[pos].hdr.pos.abs_y, color);
                        draw_line(config[pos].hdr.pos.abs_x, config[pos].hdr.pos.abs_y, config[pos].hdr.pos.abs_x, config[pos].hdr.pos.abs_y + config[pos].hdr.pos.h, color);
                        draw_line(config[pos].hdr.pos.abs_x + config[pos].hdr.pos.w, config[pos].hdr.pos.abs_y + config[pos].hdr.pos.h, config[pos].hdr.pos.abs_x, config[pos].hdr.pos.abs_y + config[pos].hdr.pos.h, color);
                        draw_line(config[pos].hdr.pos.abs_x + config[pos].hdr.pos.w, config[pos].hdr.pos.abs_y + config[pos].hdr.pos.h, config[pos].hdr.pos.abs_x + config[pos].hdr.pos.w, config[pos].hdr.pos.abs_y, color);
                        draw_line(config[pos].hdr.pos.abs_x, config[pos].hdr.pos.abs_y, config[pos].hdr.pos.abs_x + config[pos].hdr.pos.w, config[pos].hdr.pos.abs_y + config[pos].hdr.pos.h, color);
                        draw_line(config[pos].hdr.pos.abs_x + config[pos].hdr.pos.w, config[pos].hdr.pos.abs_y, config[pos].hdr.pos.abs_x, config[pos].hdr.pos.abs_y + config[pos].hdr.pos.h, color);
                    }
                    else
                    {
                        bmp_fill(color,config[pos].hdr.pos.abs_x,config[pos].hdr.pos.abs_y,8,8);
                    }
                    
                    if(selected_item == pos)
                    {
                        /* draw anchor line */
                        info_elem_t *anchor = &(config[config[pos].hdr.pos.anchor]);
                        int32_t anchor_offset_x = 0;
                        int32_t anchor_offset_y = 0;
                        int32_t element_offset_x = 0;
                        int32_t element_offset_y = 0;
                        
                        info_get_anchor_offset(anchor, config[pos].hdr.pos.anchor_flags, &anchor_offset_x, &anchor_offset_y);
                        info_get_anchor_offset(&(config[pos]), config[pos].hdr.pos.anchor_flags_self, &element_offset_x, &element_offset_y);
                        
                        draw_line(anchor->hdr.pos.abs_x + anchor_offset_x, anchor->hdr.pos.abs_y + anchor_offset_y, config[pos].hdr.pos.abs_x + element_offset_x, config[pos].hdr.pos.abs_y + element_offset_y, COLOR_WHITE);
                    }
                    
                    /* now put the title bar */
                    char label[64];
                    int offset = 0;
                    int font_height = fontspec_font(FONT_SMALL)->height;
                    
                    strcpy(label, "");
                    
                    /* position properly when the item is at some border */
                    if(font_height > config[pos].hdr.pos.abs_y)
                    {
                        offset = config[pos].hdr.pos.h;
                    }
                    else
                    {
                        offset = -font_height;
                    }
                    
                    /* any name to print? */
                    if(strlen(config[pos].hdr.pos.name) > 0)
                    {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%s ", config[pos].hdr.pos.name);
                        strcpy(&label[strlen(label)], buf);
                    }
                    
                    if(config[0].config.show_boundaries)
                    {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "%d draws ", config[pos].hdr.pos.redraws);
                        strcpy(&label[strlen(label)], buf);
                    }
                    
                    int fnt = FONT(FONT_SMALL, COLOR_WHITE, color);
                    bmp_printf(fnt, COERCE(config[pos].hdr.pos.abs_x, 0, 720), COERCE(config[pos].hdr.pos.abs_y + offset, 0, 480), label);
                }
            }
            pos++;
        }
        /* find the next highest layers */
        z = info_get_next_z(config, z + 1);
    } 
    return 0;
}


uint32_t info_print_screen()
{
    return info_print_config(info_config);
}


#ifdef FLEXINFO_DEVELOPER_MENU

char info_current_menu[64];

void info_menu_item_select(void* priv, int delta)
{
    uint32_t count = 0;
    info_elem_t *config = (info_elem_t *)priv;

    while(config[count].type != INFO_TYPE_END)
    {
        count++;
    }
    
    if((delta < 0 && config[0].config.selected_item > 0) || (delta > 0 && config[0].config.selected_item < count))
    {
        config[0].config.selected_item += delta;
    }
}

void info_menu_item_display(void *priv, int x, int y, int selected)
{
    char menu_line[64];
    info_elem_t *config = (info_elem_t *)priv;
    
    if(config[0].config.selected_item)
    {
        snprintf(menu_line, sizeof(menu_line), "Selected item: #%d", config[0].config.selected_item);
    }
    else
    {
        snprintf(menu_line, sizeof(menu_line), "Selected item: (none)");
    }
    
    if(selected && config[0].config.selected_item)
    {
        strcpy(info_current_menu, menu_line);
    }
    else
    {
        bmp_printf(MENU_FONT, x, y, menu_line);
    }
}

void info_menu_item_posx_select(void* priv, int delta)
{
    info_elem_t *config = (info_elem_t *)priv;
    info_elem_t *item = (info_elem_t *) &config[config[0].config.selected_item];
    
    if((delta < 0 && item->hdr.pos.x > -50) || (delta > 0 && item->hdr.pos.x < 720))
    {
        item->hdr.pos.x += delta;
    }
}

void info_menu_item_posy_select(void* priv, int delta)
{
    info_elem_t *config = (info_elem_t *)priv;
    info_elem_t *item = (info_elem_t *) &config[config[0].config.selected_item];
    
    if((delta < 0 && item->hdr.pos.y > -50) || (delta > 0 && item->hdr.pos.y < 480))
    {
        item->hdr.pos.y += delta;
    }
}

void info_menu_item_posz_select(void* priv, int delta)
{
    info_elem_t *config = (info_elem_t *)priv;
    info_elem_t *item = (info_elem_t *) &config[config[0].config.selected_item];
    
    if((delta < 0 && item->hdr.pos.z > 0) || (delta > 0 && item->hdr.pos.z < 32))
    {
        item->hdr.pos.z += delta;
    }
}

void info_menu_item_posx_display(void *priv, int x, int y, int selected)
{
    char menu_line[64];
    info_elem_t *config = (info_elem_t *)priv;
    
    if(config[0].config.selected_item)
    {
        snprintf(menu_line, sizeof(menu_line), "X-Position: #%d", config[config[0].config.selected_item].hdr.pos.x);
    }
    else
    {
        snprintf(menu_line, sizeof(menu_line), "X-Position: (none)");
    }
    
    if(selected && config[0].config.selected_item)
    {
        strcpy(info_current_menu, menu_line);
    }
    else
    {
        bmp_printf(MENU_FONT, x, y, menu_line);
    }
}

void info_menu_item_posy_display(void *priv, int x, int y, int selected)
{
    char menu_line[64];
    info_elem_t *config = (info_elem_t *)priv;
    
    if(config[0].config.selected_item)
    {
        snprintf(menu_line, sizeof(menu_line), "Y-Position: #%d", config[config[0].config.selected_item].hdr.pos.y);
    }
    else
    {
        snprintf(menu_line, sizeof(menu_line), "Y-Position: (none)");
    }
    
    if(selected && config[0].config.selected_item)
    {
        strcpy(info_current_menu, menu_line);
    }
    else
    {
        bmp_printf(MENU_FONT, x, y, menu_line);
    }
}

void info_menu_item_posz_display(void *priv, int x, int y, int selected)
{
    char menu_line[64];
    info_elem_t *config = (info_elem_t *)priv;
    
    if(config[0].config.selected_item)
    {
        snprintf(menu_line, sizeof(menu_line), "Z-Position: #%d", config[config[0].config.selected_item].hdr.pos.z);
    }
    else
    {
        snprintf(menu_line, sizeof(menu_line), "Z-Position: (none)");
    }
    
    if(selected && config[0].config.selected_item)
    {
        strcpy(info_current_menu, menu_line);
    }
    else
    {
        bmp_printf(MENU_FONT, x, y, menu_line);
    }
    
}




void info_menu_item_anchor_select(void* priv, int delta)
{
    info_elem_t *config = (info_elem_t *)priv;
    info_elem_t *item = (info_elem_t *) &config[config[0].config.selected_item];

    if((delta < 0 && item->hdr.pos.anchor_flags > 0) || (delta > 0 && item->hdr.pos.anchor_flags < 15))
    {
        item->hdr.pos.anchor_flags += delta;
    }
}

void info_menu_item_anchor_display(void *priv, int x, int y, int selected)
{
    char menu_line[64];
    info_elem_t *config = (info_elem_t *)priv;
    const char *text[] = {
        "Absolute",
        "Left", "H-Center", "Right",
        "Top",
        "Top-Left", "Top-Center", "Top-Right", 
        "V-Center",
        "Left-Center", "Center", "Right-Center",
        "Bottom",
        "Bottom-Left", "Bottom-Center", "Bottom-Right"};

    if(config[0].config.selected_item)
    {
        snprintf(menu_line, sizeof(menu_line), "Anchored: %s", text[config[config[0].config.selected_item].hdr.pos.anchor_flags]);
    }
    else
    {
        snprintf(menu_line, sizeof(menu_line), "Anchored: (none)");
    }
    
    if(selected && config[0].config.selected_item)
    {
        strcpy(info_current_menu, menu_line);
    }
    else
    {
        bmp_printf(MENU_FONT, x, y, menu_line);
    }
}

void info_menu_item_anchor_item_select(void* priv, int delta)
{
    uint32_t count = 0;
    info_elem_t *config = (info_elem_t *)priv;
    info_elem_t *item = (info_elem_t *) &config[config[0].config.selected_item];

    while(config[count].type != INFO_TYPE_END)
    {
        count++;
    }
    
    if((delta < 0 && item->hdr.pos.anchor > 0) || (delta > 0 && item->hdr.pos.anchor < count))
    {
        item->hdr.pos.anchor += delta;
    }
}

void info_menu_item_anchor_item_display(void *priv, int x, int y, int selected)
{
    char menu_line[64];
    info_elem_t *config = (info_elem_t *)priv;
    info_elem_t *item = (info_elem_t *) &config[config[0].config.selected_item];
    
    if(config[0].config.selected_item)
    {
        snprintf(menu_line, sizeof(menu_line), "Anchor item: #%d", item->hdr.pos.anchor);
    }
    else
    {
        snprintf(menu_line, sizeof(menu_line), "Anchor item: (none)");
    }
    
    if(selected && config[0].config.selected_item)
    {
        strcpy(info_current_menu, menu_line);
    }
    else
    {
        bmp_printf(MENU_FONT, x, y, menu_line);
    }
}


void info_menu_item_anchor_self_select(void* priv, int delta)
{
    info_elem_t *config = (info_elem_t *)priv;
    info_elem_t *item = (info_elem_t *) &config[config[0].config.selected_item];

    if((delta < 0 && item->hdr.pos.anchor_flags_self > 0) || (delta > 0 && item->hdr.pos.anchor_flags_self < 15))
    {
        item->hdr.pos.anchor_flags_self += delta;
    }
}


void info_menu_item_hide_select(void* priv, int delta)
{
    info_elem_t *config = (info_elem_t *)priv;
    info_elem_t *item = (info_elem_t *) &config[config[0].config.selected_item];
    
    item->hdr.pos.user_disable = !item->hdr.pos.user_disable;
}

void info_menu_item_hide_display(void *priv, int x, int y, int selected)
{
    char menu_line[64];
    info_elem_t *config = (info_elem_t *)priv;
    info_elem_t *item = (info_elem_t *) &config[config[0].config.selected_item];
    
    if(config[0].config.selected_item)
    {
        if(item->hdr.pos.user_disable)
        {
            snprintf(menu_line, sizeof(menu_line), "Show Item");
        }
        else
        {
            snprintf(menu_line, sizeof(menu_line), "Hide Item");
        }
    }
    else
    {
        snprintf(menu_line, sizeof(menu_line), "Anchor item: (none)");
    }
    
    if(selected && config[0].config.selected_item)
    {
        strcpy(info_current_menu, menu_line);
    }
    else
    {
        bmp_printf(MENU_FONT, x, y, menu_line);
    }
}

void info_menu_item_anchor_self_display(void *priv, int x, int y, int selected)
{
    char menu_line[64];
    info_elem_t *config = (info_elem_t *)priv;
    const char *text[] = {
        "Absolute",
        "Left", "H-Center", "Right",
        "Top",
        "Top-Left", "Top-Center", "Top-Right", 
        "V-Center",
        "Left-Center", "Center", "Right-Center",
        "Bottom",
        "Bottom-Left", "Bottom-Center", "Bottom-Right" };

    if(config[0].config.selected_item)
    {
        snprintf(menu_line, sizeof(menu_line), "Own anchor: %s", text[config[config[0].config.selected_item].hdr.pos.anchor_flags_self]);
    }
    else
    {
        snprintf(menu_line, sizeof(menu_line), "Own anchor: (none)");
    }
    
    if(selected && config[0].config.selected_item)
    {
        strcpy(info_current_menu, menu_line);
    }
    else
    {
        bmp_printf(MENU_FONT, x, y, menu_line);
    }
    
}
void info_menu_save_select(void* priv, int delta)
{
    info_save_config(info_config, FLEXINFO_DEFAULT_FILENAME);
}

void info_menu_save_display(void *priv, int x, int y, int selected)
{
    bmp_printf(MENU_FONT, x, y, "Save config");
}


void info_menu_reset_select(void* priv, int delta)
{
    info_load_config(FLEXINFO_DEFAULT_FILENAME);
    info_print_config(info_config);
}

void info_menu_reset_display(void *priv, int x, int y, int selected)
{
    info_elem_t *config = (info_elem_t *)priv;
    
    /* last item in menu draws the edit screen */
    if(config[0].config.selected_item && strlen(info_current_menu) > 0)
    {
        uint32_t font_type = FONT_MED;
        uint32_t menu_x = 12;
        uint32_t menu_y = 14;
        uint32_t width = fontspec_font(font_type)->width * strlen(info_current_menu);
        uint32_t height = fontspec_font(font_type)->height;

        /* draw dummy icon and clear screen */
        menu_draw_icon(x, y, -1, 0);
        bmp_fill(COLOR_GRAY40, 0, 0, 720, 480);
        
        /* border */
        bmp_fill(COLOR_BLACK, menu_x-2, menu_y-2, width+4, height+4);
        bmp_draw_rect(COLOR_WHITE, menu_x-2, menu_y-2, width+4, height+4);
        
        /* title */
        int fnt = FONT(FONT_SMALL, COLOR_BLACK, COLOR_WHITE);
        bmp_printf(fnt, menu_x, menu_y - fontspec_font(FONT_SMALL)->height - 2, "Current Menu:");

        fnt = FONT(font_type, COLOR_WHITE, COLOR_GREEN1);
        bmp_printf(fnt, menu_x, menu_y, info_current_menu);
        strcpy(info_current_menu, "");

        info_print_config(config);
    }
    else
    {
        bmp_printf(MENU_FONT, x, y, "Reset setup");
    }
}

static struct menu_entry info_menus[] = {
    {
        .name = "FlexInfo Settings",
        .select = menu_open_submenu,
        .submenu_width = 700,
        .children =  (struct menu_entry[]) {
            {
                .name = "Show boundaries",
                .priv = &(info_config[0].config.show_boundaries),
                .max = 1,
                .help = "Enable boundary display for all elements.",
            },
            {
                .name = "Selected item",
                .priv = info_config,
                .min = 0,
                .max = COUNT(info_config) - 1,
                .select = info_menu_item_select,
                .display = info_menu_item_display,
                .help = "Select a specific element for editing.",
            },
            {
                .name = "Hide Item",
                .priv = info_config,
                .max = 1,
                .select = info_menu_item_hide_select,
                .display = info_menu_item_hide_display,
                .help = "Show or hide the item.",
            },
            {
                .name = "Pos X",
                .priv = info_config,
                .min = 0,
                .max = 720,
                .select = info_menu_item_posx_select,
                .display = info_menu_item_posx_display,
                .help = "Move item in its X position.",
            },
            {
                .name = "Pos Y",
                .priv = info_config,
                .min = 0,
                .max = 480,
                .select = info_menu_item_posy_select,
                .display = info_menu_item_posy_display,
                .help = "Move item in its Y position.",
            },
            {
                .name = "Pos Z",
                .priv = info_config,
                .min = 0,
                .max = 32,
                .select = info_menu_item_posz_select,
                .display = info_menu_item_posz_display,
                .help = "Move item in its Z position.",
            },
            {
                .name = "Anchor type",
                .priv = info_config,
                .min = 0,
                .max = 9,
                .select = info_menu_item_anchor_select,
                .display = info_menu_item_anchor_display,
                .help = "Select anchor type.",
            },
            {
                .name = "Anchor own",
                .priv = info_config,
                .min = 0,
                .max = 9,
                .select = info_menu_item_anchor_self_select,
                .display = info_menu_item_anchor_self_display,
                .help = "Select own anchor type.",
            },
            {
                .name = "Anchor item",
                .priv = info_config,
                .select = info_menu_item_anchor_item_select,
                .display = info_menu_item_anchor_item_display,
                .help = "Select Anchor item.",
            },
            {
                .name = "Save config",
                .priv = info_config,
                .select = info_menu_save_select,
                .display = info_menu_save_display,
                .help = "Save menu settings",
            },
            {
                /* this item is the last and thus overwrites the whole screen when editing */
                .name = "Reset setup",
                .priv = info_config,
                .select = info_menu_reset_select,
                .display = info_menu_reset_display,
                .help = "Reset menu settings",
            },
            MENU_EOL,
        }
    }
};

static void info_init()
{
    menu_add( "Prefs", info_menus, COUNT(info_menus) );
    info_load_config(FLEXINFO_DEFAULT_FILENAME);
}

static void info_edit_task()
{
    TASK_LOOP
    {
        if (gui_menu_shown() && (info_config[0].config.selected_item || info_config[0].config.show_boundaries))
        {
            info_print_config(info_config);
            msleep(50);
        }
        else
        {
            msleep(500);
        }
    }
}

TASK_CREATE( "info_edit_task", info_edit_task, 0, 0x16, 0x1000 );
INIT_FUNC("info.init", info_init);

#endif
