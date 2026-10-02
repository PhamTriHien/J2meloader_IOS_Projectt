#ifdef __ANDROID__
// Android sends an app's stdout/stderr to /dev/null; pipe them into logcat so the core's
// diagnostics (and MIDlet System.out) are visible with `adb logcat -s J2ME`.
#include <android/log.h>
#include <cstdio>
#include <string>
#include <thread>
#include <unistd.h>

namespace {

struct StdioToLogcat {
    StdioToLogcat() {
        int fds[2];
        if (pipe(fds) != 0) return;
        setvbuf(stdout, nullptr, _IOLBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);
        std::thread([fd = fds[0]] {
            char buf[1024];
            std::string line;
            ssize_t n;
            while ((n = read(fd, buf, sizeof(buf))) > 0) {
                line.append(buf, static_cast<size_t>(n));
                size_t nl;
                while ((nl = line.find('\n')) != std::string::npos) {
                    line[nl] = '\0';
                    __android_log_write(ANDROID_LOG_INFO, "J2ME", line.c_str());
                    line.erase(0, nl + 1);
                }
                // logcat truncates long entries; flush partial lines before that happens
                if (line.size() > 3000) {
                    __android_log_write(ANDROID_LOG_INFO, "J2ME", line.c_str());
                    line.clear();
                }
            }
        }).detach();
    }
} g_stdioToLogcat;

} // namespace
#endif
