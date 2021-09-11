#include "dryos.h"
#include "wifi.h"

static int wifi_toggle_state;
/*
extern int _socket_create(int domain, int type, int protocol);
int socket_create(int domain, int type, int protocol)
{
    return _socket_create(domain, type, protocol);
}
extern int _socket_bind(int sockfd, void *addr, int addrlen);
int socket_bind(int sockfd, void *addr, int addrlen)
{
    return _socket_bind(sockfd, addr, addrlen);
}
extern int _socket_listen(int sockfd, int backlogl);
int socket_listen(int sockfd, int backlogl)
{
    return _socket_listen(sockfd, backlogl);
}
extern int _socket_accept(int sockfd, void *addr, int addrlen);
int socket_accept(int sockfd, void *addr, int addrlen)
{
    return _socket_accept(sockfd, addr, addrlen);
}
extern int _socket_connect(int sockfd, void *addr, int addrlen);
int socket_connect(int sockfd, void *addr, int addrlen)
{
    return _socket_connect(sockfd, addr, addrlen);
}

extern int _socket_recv(int sockfd, void *buf, int len, int flags);
int socket_recv(int sockfd, void *buf, int len, int flags)
{
    return _socket_recv(sockfd, buf, len, flags);
}
extern int _socket_send(int sockfd, void *buf, int len, int flags);
int socket_send(int sockfd, void *buf, int len, int flags)
{
    return _socket_send(sockfd, buf, len, flags);
}
extern int _socket_shutdown(int sockfd, int flag);
int socket_shutdown(int sockfd, int flag)
{
    return _socket_shutdown(sockfd, flag);
}
extern int _socket_setsockopt(int socket, int level, int option_name, const void *option_value, int option_len);
int socket_setsockopt(int socket, int level, int option_name, const void *option_value, int option_len)
{
    return _socket_setsockopt(socket, level, option_name, option_value, option_len);
}
//canon has a  socket descriptor 'translator' which converts the ICU descriptors into the ones which the LIME core can work from
extern int _socket_convertfd(int sockfd);
//all of these has a translator for the  socket descriptor
extern void _socket_close_caller(int sockfd);
void socket_close(int sockfd)
{
    int converted = _socket_convertfd(sockfd);
    _socket_close_caller(converted);
}
*/

extern int wlanconnect(struct WlanSettings_t *wifisettings);
extern int nif_setup(uint32_t interface);
extern int rt_edit(uint32_t index, uint32_t ip, uint32_t subnet, uint32_t gateway);
int wifi_connect(const char *ssid, const char *passphrase, const char *ip, struct WlanSettings_t *wifisettings)
{
        DryosDebugMsg(0, 15, "In wifi connect: \n");

    memset(wifisettings, 0, sizeof(struct WlanSettings_t));
    strcpy(wifisettings->pSSID, ssid);
    strcpy(wifisettings->pKey, passphrase);
    wifisettings->a = 0;
    wifisettings->mode = INFRA;
    wifisettings->modeSomething = 2;
    wifisettings->channel = 2;
    wifisettings->authMode = BOTH;
    wifisettings->cipherMode = AES;
    wifisettings->g = 0;
    wifisettings->h = 6;
    //Turn lime core on
//    call("NwLimeInit");
//    call("NwLimeOn");
        DryosDebugMsg(0, 15, "Apelez wlanpoweron \n");
    call("wlanpoweron");
        DryosDebugMsg(0, 15, "Apelez wlanup\n");
    call("wlanup");
        DryosDebugMsg(0, 15, "Apelez wlanchk \n");
    call("wlanchk");
        DryosDebugMsg(0, 15, "DONE apelate \n");
    if (wlanconnect(wifisettings) != 0)
    {
        printf("Failed to connect to wifi\n");
        return 0;
    }
    // call("wlanipset", ip);
    if(nif_setup(0) != 0){
        printf("nif setup failed\n");
        return 0;
    }
    if(rt_edit(0, 0x0d64a8c0, 0x00ffffff, 0x0164a8c0) != 0 ){
        printf("Failed to modify route table\n");
        return 0;
    }
    while (MEM(0x1d90c) == 0xe073631f)
    {
        msleep(100);
    }              //wait for lime core to power on
    msleep(10000); //wait for the Lime core to init.
    return 1;
}
/*
int inet_pton4(const char *src, char *dst)
{
    static const char digits[] = "0123456789";
    int saw_digit, octets, ch;
#define NS_INADDRSZ 4
    u_char tmp[NS_INADDRSZ], *tp;

    saw_digit = 0;
    octets = 0;
    *(tp = tmp) = 0;
    while ((ch = *src++) != '\0')
    {
        const char *pch;

        if ((pch = strchr(digits, ch)) != NULL)
        {
            u_int new = *tp * 10 + (pch - digits);

            if (saw_digit && *tp == 0)
                return (0);
            if (new > 255)
                return (0);
            *tp = new;
            if (!saw_digit)
            {
                if (++octets > 4)
                    return (0);
                saw_digit = 1;
            }
        }
        else if (ch == '.' && saw_digit)
        {
            if (octets == 4)
                return (0);
            *++tp = 0;
            saw_digit = 0;
        }
        else
            return (0);
    }
    if (octets < 4)
        return (0);
    memcpy(dst, tmp, NS_INADDRSZ);
    return (1);
}
*/
static MENU_SELECT_FUNC(wifi_toggle)
{
//    wifi_toggle_state = !wifi_toggle_state;
    struct WlanSettings_t wifisettings;
    DryosDebugMsg(0, 15, "In menu wifi, wifi_toggle_state: %d\n", wifi_toggle_state);

    if (!wifi_toggle_state)
    {
 //       wifi_toggle_state = wifi_connect("Cristi", "Dlstuahj1", "192.168.1.13", &wifisettings);
        wifi_toggle_state = 1;
        DryosDebugMsg(0, 15, "Wifi state1: %d\n", wifi_toggle_state);
    }
    else
    {
 //       call("wlanpoweroff");
 //       call("wlandown");
        wifi_toggle_state = 0;
        DryosDebugMsg(0, 15, "Wifi state2: %d\n", wifi_toggle_state);
    }
}
static MENU_UPDATE_FUNC(ssid_display)
{
    MENU_SET_VALUE("Cristi");
}
static MENU_UPDATE_FUNC(pass_display)
{
    MENU_SET_VALUE("Dlstuahj1");
}
static MENU_UPDATE_FUNC(ip_display)
{
    MENU_SET_VALUE("192.168.1.13");
}
static MENU_UPDATE_FUNC(subnet_display)
{
    MENU_SET_VALUE("255.255.255.0");
}
static MENU_UPDATE_FUNC(gateway_display)
{
    MENU_SET_VALUE("192.168.1.1");
}
static struct menu_entry wifi_menu[] = {
    {
        .name = "WLAN Options",
        .select = menu_open_submenu,
    .submenu_width = 710,
    .help = "Config ssid,pass,gateway...",
    .children =  (struct menu_entry[]) {
        {
        .name = "WIFI",
        .priv = &wifi_toggle_state,
        .max = 1,
        .select = wifi_toggle
        },
        {
            .name = "SSID",
            .update = ssid_display
        },
        {
            .name = "PASS",
            .update = pass_display
        },
        {
            .name = "IP",
            .update = ip_display
        },
        {
            .name = "Subnet Mask",
            .update = subnet_display
        },
        {
            .name = "Gateway",
            .update = gateway_display
        },

        MENU_EOL,
    },
    }};
void wifi_menu_init()
{
    menu_add("Prefs", wifi_menu, COUNT(wifi_menu));
}

INIT_FUNC(__FILE__, wifi_menu_init);
