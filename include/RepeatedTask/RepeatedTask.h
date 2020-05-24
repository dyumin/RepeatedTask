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
    explicit RepeatedTask(RepeatedTask<Fp1, Rep1, Period1>&& other)
    {
        auto tmp = other.release();

        m_f = std::move(std::get<0>(tmp));
        m_period = std::get<1>(tmp);

        if (!std::get<2>(tmp))
        {
            std::unique_lock<decltype(m_mutex)> lock(m_mutex);
            start(lock);
        }
    }

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-unconventional-assign-operator"
    template<class Fp1, class Rep1, class Period1>
    typename std::enable_if<std::is_same<RepeatedTask<Fp1, Rep1, Period1>, RepeatedTask>::value, RepeatedTask&>::type
    operator=(RepeatedTask<Fp1, Rep1, Period1>&& other)
    {
        if (&other == this)
            return *this;

        return operator=<Fp1, Rep1, Period1, std::true_type>(std::forward<RepeatedTask<Fp1, Rep1, Period1>>(other));
    }

    template<class Fp1, class Rep1, class Period1, class tag = std::false_type>
    typename std::enable_if<!std::is_same<RepeatedTask<Fp1, Rep1, Period1>, RepeatedTask>::value || tag::value, RepeatedTask&>::type
    operator=(RepeatedTask<Fp1, Rep1, Period1>&& other)
    {
        auto tmp = other.release();

        std::unique_lock<decltype(m_mutex)> lock(m_mutex);

        m_f = std::move(std::get<0>(tmp));
        m_period = std::get<1>(tmp);

        const bool stopped = m_stopped;
        const bool otherStopped = std::get<2>(tmp);
        if (stopped && !otherStopped)
            start(lock);
        else if (!stopped && otherStopped)
            stop(lock);

        return *this;
    }
#pragma clang diagnostic pop

    std::tuple<callback_type, period_type, bool> release()
    {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex);
        const bool stopped = m_stopped;
        if (!stopped)
            stop(lock);

        auto tmp = std::make_tuple(std::move(m_f), m_period, stopped);

#ifdef __cpp_if_constexpr
        if constexpr (std::is_copy_constructible_v<callback_type> /*  */
            && std::is_default_constructible_v<callback_type>
            && (std::is_copy_assignable_v<callback_type> || std::is_move_assignable_v<callback_type>))
            m_f = callback_type();
#else
        // Do the same old style
        default_initializer::destroy(m_f);
#endif
        return tmp;
    }

    // noncopyable
    explicit RepeatedTask(const RepeatedTask&) = delete;
    template<class Fp1, class Rep1, class Period1>
    explicit RepeatedTask(const RepeatedTask<Fp1, Rep1, Period1>&) = delete;

    RepeatedTask& operator=(const RepeatedTask&) = delete;
    template<class Fp1, class Rep1, class Period1>
    RepeatedTask& operator=(const RepeatedTask<Fp1, Rep1, Period1>&) = delete;

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

#ifndef __cpp_if_constexpr
    struct default_initializer final
    {
        template<class T>
        static void destroy(
            T& f,
            typename std::enable_if<
                std::is_copy_constructible<T>::value
             && std::is_default_constructible<T>::value
             && (std::is_copy_assignable<T>::value || std::is_move_assignable<T>::value)
            >::type* = 0 // todo: type* ?
            )
        {
            f = T();
        }

        template<class T>
        static void destroy(
            T& f,
            typename std::enable_if<
              !(std::is_copy_constructible<T>::value
             && std::is_default_constructible<T>::value
             && (std::is_copy_assignable<T>::value || std::is_move_assignable<T>::value))
            >::type* = 0
        ) noexcept
        {}
    };
#endif

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
