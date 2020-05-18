#include <iostream>


#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <mutex>

template<class F, class Rep, class Period>
class RepeatedTaskImpl final
{
public:
    typedef F callback_type;
    typedef std::chrono::duration<Rep, Period> period_type;

    RepeatedTaskImpl() noexcept = default;

    explicit RepeatedTaskImpl(callback_type f, const period_type& period) noexcept
        : m_f(std::forward<callback_type>(f)), m_period(period)
    {
    }

    ~RepeatedTaskImpl()
    {
        std::unique_lock lock(m_mutex);
        if (!m_stopped)
        {
            m_shouldStop.store(true, std::memory_order_release);
            m_cv.wait(lock, [&]()
            {
                return !m_stopped;
            });
        }
    }

    // noncopyable
    RepeatedTaskImpl(const RepeatedTaskImpl&) = delete;
    RepeatedTaskImpl& operator=(const RepeatedTaskImpl&) = delete;

public:
    void run()
    {
        std::unique_lock lock(m_mutex);
        m_cv.notify_one();

        auto timePoint = std::chrono::steady_clock::now() + m_period;

        while (!m_cv.wait_until(lock, timePoint, [&]()
        {
            return m_shouldStop.load(std::memory_order_acquire);
        }))
        {
            m_f();
            timePoint += m_period;
        }

        m_cv.notify_one();
        m_stopped = true;
    }

private:
    const callback_type m_f;
    period_type m_period;

    std::condition_variable m_cv;
    std::mutex m_mutex;

    std::atomic<bool> m_shouldStop = ATOMIC_VAR_INIT(false);
    bool m_stopped = false;
};

template<class F, class Rep, class Period>
class RepeatedTask final
{
private:
    typedef RepeatedTaskImpl<F, Rep, Period> impl;
public:
    typedef typename impl::callback_type callback_type;
    typedef typename impl::period_type period_type;

    RepeatedTask() noexcept = default;

    explicit RepeatedTask(F f, const std::chrono::duration<Rep, Period>& period) noexcept
        : m_impl(std::make_unique<impl>(std::forward<F>(f), period)), m_thread(std::thread(&impl::run, m_impl.get()))
    {
    }

    ~RepeatedTask() noexcept
    {
        m_impl.reset();
        if (m_thread.joinable())
            m_thread.join();
    }

private:
    std::unique_ptr<impl> m_impl;
    std::thread m_thread;
};

int ff()
{
    return 0;
}

int main()
{
    std::mutex m;

    std::condition_variable cv;

    std::unique_ptr<std::condition_variable> cd(new std::condition_variable);

//    std::condition_variable cv2 = std::move(cv);

//    auto lock = std::lock_guard(m);

    auto f = [&]()
    {
        std::unique_lock lock(m);
        std::cout << std::chrono::system_clock::now().time_since_epoch().count() << std::endl;
    };

    RepeatedTask task(f, std::chrono::milliseconds(100));

    std::this_thread::sleep_for(std::chrono::minutes(10));

    return 0;
}
