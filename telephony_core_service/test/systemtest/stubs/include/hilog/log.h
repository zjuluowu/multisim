#pragma once
// Logging-only sink; discard all format arguments to avoid identity leakage.
enum { LOG_CORE, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL };
inline int HILOG_IMPL(int, int, unsigned int, const char*, const char*, ...) { return 0; }

// Fork diagnostic call shape; deliberately discards all arguments like HILOG_IMPL.
inline void HILOG_COMM_INFO(const char *, ...) {}
inline void HILOG_COMM_ERROR(const char *, ...) {}
