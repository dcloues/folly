#ifndef LOG_H
#define LOG_H

#ifdef LOG_ENABLED
void hlog(char *str, ...);
void hlog_init(char *);
void hlog_shutdown();
#else
#define hlog(...)
#define hlog_init(...)
#define hlog_shutdown()
#endif

#endif

