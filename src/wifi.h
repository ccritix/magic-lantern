#ifndef WIFI_H
#define WIFI_H
#define AF_INET 1
#define SOCK_STREAM 1
#define NS_INADDRSZ 4

//thanks coon
typedef enum WlanSettingsCipherMode_t
{
  NONE = 0,
  WEP = 1,
  AES = 4
} WlanSettingsCipherMode_t;
typedef enum WlanSettingsAuthMode_t
{
  OPEN = 0,
  SHARED = 1,
  WPA2PSK = 5,
  BOTH = 6,
} WlanSettingsAuthMode_t;

typedef enum WlanSettingsMode_t
{
  ADHOC_WIFI = 1,
  INFRA = 2,
  ADHOC_G = 3
} WlanSettingsMode_t;
struct WlanSettings_t
{
  int a;
  int mode; // adhoc-wifi, adhoc-g, infra
  int modeSomething;       // adhoc-wifi = 1, adhoc-g = 2
  char pSSID[36];
  int channel;
  int authMode;     // open, shared, wpa2psk, both
  int cipherMode; // WEP or AES
  int g;
  int h;
  char pKey[63];
};

int socket_create(int domain, int type, int protocol);
int socket_bind(int sockfd, void *addr, int addrlen);
int socket_listen(int sockfd, int backlogl);
int socket_accept(int sockfd, void *addr, int addrlen);
int socket_connect(int sockfd, void *addr, int addrlen);
int socket_recv(int sockfd, void *buf, int len, int flags);
int socket_send(int sockfd, void *buf, int len, int flags);
int socket_shutdown(int sockfd, int flag);
int socket_setsockopt(int socket, int level, int option_name, const void *option_value, int option_len);
void socket_close(int sockfd);

int wifi_connect(const char * ssid, const char * passphrase, const char *ip,struct WlanSettings_t *wifisettings);
void wifi_menu_init();

int inet_pton4(const char *src, char *dst);


#endif //WIFI_H