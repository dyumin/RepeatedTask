#ifndef EE8852D8_7930_4C63_AA9D_1A06652BC13C
#define EE8852D8_7930_4C63_AA9D_1A06652BC13C

#include <mutex>
#include <thread>
#include <chrono>
#include <condition_variable>

template<class Fp, class Rep = std::chrono::nanoseconds::rep, class Period = std::chrono::nanoseconds::period>
class RepeatedTask
{
public:
    typedef Fp callback_type;
    typedef std::chrono::duration<Rep, Period> period_type;

    template<class, class, class>  // all template instantiations are friends to each other
    friend class RepeatedTask;

    RepeatedTask() noexcept = default;

    template<class Fp1, class Rep1 = std::chrono::nanoseconds::rep, class Period1 = std::chrono::nanoseconds::period>
    RepeatedTask(Fp1&& f, const std::chrono::duration<Rep1, Period1>& period)
        : m_f(std::forward<Fp1>(f))
        , m_period(period)
    {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);
        start(lock);
    }

    ~RepeatedTask() noexcept
    {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);
        if (!m_stopped)
            stop(lock);
    }

    template<class Fp1, class Rep1, class Period1>
    explicit RepeatedTask(RepeatedTask<Fp1, Rep1, Period1>&& other) noexcept(noexcept(m_f = std::move(other.m_f)))
    {
        std::unique_lock<decltype(other.m_mutex)> otherLock(other.m_mutex);
        const bool otherStopped = other.m_stopped;
        if (!otherStopped)
            other.stop(otherLock);

        m_f = std::move(other.m_f);
        m_period = other.m_period;

        if (!otherStopped)
        {
            std::unique_lock<decltype(m_mutex)> lock(m_mutex);
            start(lock);
        }
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-unconventional-assign-operator"
    template<class Fp1, class Rep1, class Period1>
    typename std::enable_if<std::is_same<RepeatedTask<Fp1, Rep1, Period1>, RepeatedTask>::value, RepeatedTask&>::type
    operator=(RepeatedTask<Fp1, Rep1, Period1>&& other) noexcept(noexcept(operator=<Fp1, Rep1, Period1, std::true_type>(std::forward<RepeatedTask<Fp1, Rep1, Period1>>(other))))
    {
        if (&other == this)
            return *this;

        return operator=<Fp1, Rep1, Period1, std::true_type>(std::forward<RepeatedTask<Fp1, Rep1, Period1>>(other));
    }

    template<class Fp1, class Rep1, class Period1, class tag = std::false_type>
    typename std::enable_if<!std::is_same<RepeatedTask<Fp1, Rep1, Period1>, RepeatedTask>::value || tag::value, RepeatedTask&>::type
    operator=(RepeatedTask<Fp1, Rep1, Period1>&& other) noexcept(noexcept(m_f = std::move(other.m_f)))
    {
        std::unique_lock<decltype(other.m_mutex)> otherLock(other.m_mutex);
        const bool otherStopped = other.m_stopped;
        if (!otherStopped)
            other.stop(otherLock);

        std::unique_lock<decltype(m_mutex)> lock(m_mutex);

        m_f = std::move(other.m_f);
        m_period = other.m_period;

        const bool stopped = m_stopped;
        if (stopped && !otherStopped)
            start(lock);
        else if (!stopped && otherStopped)
            stop(lock);

        return *this;
    }
#pragma clang diagnostic pop

private:
    template <class Lock>
    void start(Lock& lock)
    {
        m_thread = std::thread(&RepeatedTask::run, this);
        m_cv.wait(lock, [&]()
        {
            return !m_stopped;
        });
    }

    template <class Lock>
    void stop(Lock& lock) noexcept
    {
        m_shouldStop = true;
        m_cv.notify_one();
        m_cv.wait(lock, [&]()
        {
            return m_stopped;
        });

        if (m_thread.joinable())
            m_thread.join();
    }

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
    callback_type m_f;
    period_type m_period;

    std::condition_variable m_cv;
    std::mutex m_mutex;

    bool m_shouldStop = false;
    bool m_stopped = true;

    std::thread m_thread;
};

#ifdef __cpp_deduction_guides

template<class Fp1, class Rep1 = std::chrono::nanoseconds::rep, class Period1 = std::chrono::nanoseconds::period>
RepeatedTask(Fp1&&, const std::chrono::duration<Rep1, Period1>&) -> RepeatedTask<Fp1, Rep1, Period1>;

#endif

#endif //EE8852D8_7930_4C63_AA9D_1A06652BC13C
