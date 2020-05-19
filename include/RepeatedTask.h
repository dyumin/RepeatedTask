#ifndef EE8852D8_7930_4C63_AA9D_1A06652BC13C
#define EE8852D8_7930_4C63_AA9D_1A06652BC13C

#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>

template<class Fp, class Rep, class Period>
class RepeatedTask
{
public:
    typedef Fp callback_type;
    typedef std::chrono::duration<Rep, Period> period_type;

    RepeatedTask() noexcept = default;

    explicit RepeatedTask(callback_type f, const period_type& period) noexcept
        : m_f(std::forward<callback_type>(f))
        , m_period(period)
    {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);
        m_thread = std::thread(&RepeatedTask::run, this);
        m_cv.wait(lock, [&]()
        {
            return !m_stopped;
        });
    }

    ~RepeatedTask() noexcept
    {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);
        if (!m_stopped)
        {
            m_shouldStop = true;
            m_cv.wait(lock, [&]()
            {
                return m_stopped;
            });
        }

        if (m_thread.joinable())
            m_thread.join();
    }

    // noncopyable
    RepeatedTask(const RepeatedTask&) = delete;
    RepeatedTask& operator=(const RepeatedTask&) = delete;

private:
    void run()
    {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);
        m_stopped = false;
        m_cv.notify_one();

        auto timePoint = std::chrono::steady_clock::now() + m_period;

        while (!m_cv.wait_until(lock, timePoint, [&]()
        {
            return m_shouldStop;
        }))
        {
            m_f();
            timePoint += m_period;
        }

        m_stopped = true;
        m_cv.notify_one();
    }

private:
    const callback_type m_f;
    period_type m_period;

    std::condition_variable m_cv;
    std::mutex m_mutex;

    bool m_shouldStop = false;
    bool m_stopped = true;

    std::thread m_thread;
};

#endif //EE8852D8_7930_4C63_AA9D_1A06652BC13C
