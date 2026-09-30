#pragma once
// Logging-only sink; discard all format arguments to avoid identity leakage.
enum { LOG_CORE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
inline int HILOG_IMPL(int, int, unsigned int, const char*, const char*, ...) { return 0; }
