void io_trace_install();
void io_trace_uninstall();
void io_trace_cleanup();

/* return its (next) index, for syncing with dm-spy (from 0 to N-1, consecutive) */
uint32_t io_trace_log_get_index();

/* log message given by index (will be called from 0 to what the above returned - 1) */
/* note: msg_buffer and msg_size can be either used directly, or passed to debug_format_msg */
/* extra care required if you want to mix these two methods */
int io_trace_log_message(uint32_t msg_index, char * msg_buffer, int msg_size);
