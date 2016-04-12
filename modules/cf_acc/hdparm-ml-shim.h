#undef putchar
#define sprintf(s, fmt, ...) { snprintf(s, 1000, fmt, ## __VA_ARGS__); }
