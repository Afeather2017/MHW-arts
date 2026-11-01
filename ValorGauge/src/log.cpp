// Split out of src/dllmain.cpp (git 6c630fc) by tools/split_dllmain.py.
// Original segments: L64-L64, L490-L510. Bodies are verbatim; see that file's history
// for the per-feature commentary and docs/ for the topic manuals.

#include "prelude.h"
#include "all.h"

std::mutex g_logMutex;

void Log(const char* message) {
#ifdef VALOR_PROBE
    constexpr const char* kLogFile = "nativePC/plugins/ValorProbe.log";
#else
    constexpr const char* kLogFile = "nativePC/plugins/ValorGauge.log";
#endif
    std::scoped_lock lock(g_logMutex);
    std::ofstream out(kLogFile, std::ios::app);
    if (out) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char ts[40];
        snprintf(ts, sizeof(ts), "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute,
                 st.wSecond, st.wMilliseconds);
        out << ts << message << '\n';
    }
}

void Log(const std::string& message) {
    Log(message.c_str());
}
