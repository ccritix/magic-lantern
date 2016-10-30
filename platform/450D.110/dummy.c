// some dummy stubs
#include "dryos.h"
#include "timer.h"

uint32_t shamem_read(uint32_t addr) { return 0; } // or maybe return MEM(addr)
void _EngDrvOut(uint32_t addr, uint32_t value) { MEM(addr) = value; }

int new_LiveViewApp_handler = 0xff123456;

//void free_space_show_photomode(){}

int audio_meters_are_drawn() { return 0; }
void volume_up(){};
void volume_down(){};
void out_volume_up(){};
void out_volume_down(){};

void load_fonts() { }

int is_mvr_buffer_almost_full() { return 0; }
void movie_indicators_show(){}
void bitrate_mvr_log(){}

int time_indic_x =  720 - 160;
int time_indic_y = 0;

void free_space_show(){};
void fps_show(){};

struct memChunk * GetNextMemoryChunk(struct memSuite * suite, struct memChunk * chunk) { return 0; }

// Additional unmapped dummy stubs
struct memSuite * CreateMemorySuite(void* first_chunk_address, size_t first_chunk_size, uint32_t flags) { return 0; }
struct memChunk * CreateMemoryChunk(void* address, size_t size, uint32_t flags) { return 0; }
void DeleteMemorySuite(struct memSuite * suite) { }
int sound_recording_enabled() { return 0; }
void SRM_AllocateMemoryResourceFor1stJob(void (*callback)(void** dst_ptr, void* raw_buffer, uint32_t raw_buffer_size), void** dst_ptr) { }
int AddMemoryChunk(struct memSuite * suite, struct memChunk * chunk) { return 0; }
void CancelDateTimer();
int SetTimerAfter(int delay_ms, timerCbr_t timer_cbr, timerCbr_t overrun_cbr, void* priv) { return 0; }
void SRM_FreeMemoryResourceFor1stJob(void* raw_buffer, int unk1_zero, int unk2_zero) { }
int GetSizeOfMaxRegion(int* max_region) { return 0; }
int64_t FIO_SeekSkipFile( FILE* stream, int64_t position, int whence ) { return 0; }

int SetHPTimerNextTick(int last_expiry, int offset, timerCbr_t timer_cbr, timerCbr_t overrun_cbr, void *priv) { return 0; }
int SetHPTimerAfterNow(int delay_us, timerCbr_t timer_cbr, timerCbr_t overrun_cbr, void* priv) { return 0; }
void CancelTimer(int timer_id){}
void* edmac_copy_rectangle_adv(void* dst, void* src, int src_width, int src_x, int src_y, int dst_width, int dst_x, int dst_y, int w, int h){}
void edmac_memcpy_res_lock(){}
int task_max = 256;
void bv_toggle(void* priv, int delta) {}
int take_screenshot( char* filename, uint32_t mode ) { return 1; }

