#ifndef JVM_THREAD_H
#define JVM_THREAD_H

#include <functional>
#include <memory>
#include <utility>
#if !defined(_WIN32) && !defined(_WIN64)
#include <pthread.h>
#else
#include <thread>
#endif

// JvmThread: A thread abstraction that allocates a 4MB stack on iOS / Darwin
// (where default pthread stack is only 512KB and causes stack overflow in recursive bytecode)
class JvmThread {
public:
    JvmThread() : m_joinable(false) {}

    template<typename F, typename... Args>
    explicit JvmThread(F&& f, Args&&... args) : m_joinable(false) {
        auto task = std::make_unique<std::function<void()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
#if !defined(_WIN32) && !defined(_WIN64)
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        // 4MB stack size for Java bytecode execution and nested native calls
        pthread_attr_setstacksize(&attr, 4 * 1024 * 1024);
        auto rawTask = task.release();
        int rc = pthread_create(&m_thread, &attr, [](void* arg) -> void* {
            auto fn = reinterpret_cast<std::function<void()>*>(arg);
            if (fn) {
                (*fn)();
                delete fn;
            }
            return nullptr;
        }, rawTask);
        pthread_attr_destroy(&attr);
        if (rc == 0) {
            m_joinable = true;
        } else {
            delete rawTask;
        }
#else
        m_thread = std::thread([t = std::move(task)]() {
            if (t && *t) (*t)();
        });
        m_joinable = m_thread.joinable();
#endif
    }

    ~JvmThread() {
        if (joinable()) {
            detach();
        }
    }

    JvmThread(const JvmThread&) = delete;
    JvmThread& operator=(const JvmThread&) = delete;

    JvmThread(JvmThread&& other) noexcept {
        *this = std::move(other);
    }

    JvmThread& operator=(JvmThread&& other) noexcept {
        if (this != &other) {
            if (joinable()) {
                detach();
            }
#if !defined(_WIN32) && !defined(_WIN64)
            m_thread = other.m_thread;
            m_joinable = other.m_joinable;
            other.m_joinable = false;
#else
            m_thread = std::move(other.m_thread);
            m_joinable = other.m_joinable;
            other.m_joinable = false;
#endif
        }
        return *this;
    }

    bool joinable() const {
#if !defined(_WIN32) && !defined(_WIN64)
        return m_joinable;
#else
        return m_thread.joinable();
#endif
    }

    void join() {
#if !defined(_WIN32) && !defined(_WIN64)
        if (m_joinable) {
            pthread_join(m_thread, nullptr);
            m_joinable = false;
        }
#else
        if (m_thread.joinable()) {
            m_thread.join();
            m_joinable = false;
        }
#endif
    }

    void detach() {
#if !defined(_WIN32) && !defined(_WIN64)
        if (m_joinable) {
            pthread_detach(m_thread);
            m_joinable = false;
        }
#else
        if (m_thread.joinable()) {
            m_thread.detach();
            m_joinable = false;
        }
#endif
    }

    // Helper for detached threads (fire-and-forget with 4MB stack)
    template<typename F, typename... Args>
    static void spawnDetached(F&& f, Args&&... args) {
        JvmThread t(std::forward<F>(f), std::forward<Args>(args)...);
        t.detach();
    }

private:
#if !defined(_WIN32) && !defined(_WIN64)
    pthread_t m_thread{};
#else
    std::thread m_thread;
#endif
    bool m_joinable = false;
};

#endif // JVM_THREAD_H
