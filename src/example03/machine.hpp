#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <tuple>
#include <type_traits>

#define NO_VARIANT

#ifdef NO_VARIANT
// C++14
namespace detail
{
template <class T, class Tuple> struct Index
{
    static_assert(!std::is_same<Tuple, std::tuple<>>::value, "type not found");
};

template <class T, class... Types> struct Index<T, std::tuple<T, Types...>>
{
    static constexpr std::size_t value = 0;
};

template <class T, class U, class... Types> struct Index<T, std::tuple<U, Types...>>
{
    static constexpr std::size_t value = 1 + Index<T, std::tuple<Types...>>::value;
};

template <typename Tuple, typename F, size_t... Is>
constexpr void applyImpl(Tuple&& t, F&& f, std::index_sequence<Is...>)
{
    auto l = {((void)std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(t))), false)...};
    static_cast<void>(l);
    // [](...) {}(((void)std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(t))), false)...);
}

template <typename Tuple, typename F> constexpr void apply(Tuple&& t, F&& f)
{
    applyImpl(std::forward<Tuple>(t), std::forward<F>(f),
              std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>{}>{});
}

template <class Tuple, class F, std::size_t... Is>
constexpr void visitImpl(Tuple&& t, const std::size_t i, F&& f, std::index_sequence<Is...>)
{
    auto l = {(i == Is && (std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(t))), false))...};
    static_cast<void>(l);
    // [](...) {}((i == Is && ((void)std::forward<F>(f)(std::get<Is>(std::forward<Tuple>(t))), false))...);
}

template <class Tuple, class F> constexpr void visit(Tuple&& t, const std::size_t i, F&& f)
{
    visitImpl(std::forward<Tuple>(t), i, std::forward<F>(f),
              std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>{}>{});
}
} // namespace detail

struct Event
{
};

template <typename T, typename M> class State
{
    friend T;
    template <typename... States> friend class Machine;

public:
    State(const State&)            = delete;
    State& operator=(const State&) = delete;

    State(State&&)            = default;
    State& operator=(State&&) = default;

    virtual ~State() = default;

    template <typename S> void transit() { mMachine->M::template transit<T, S>(); }

private:
    State() = default;

    void setMachine(M* machine) { mMachine = machine; }

private:
    M* mMachine = nullptr;
};

template <typename... States> class Machine final
{
    static_assert(sizeof...(States) > 0, "states are empty");

    using TupleType = std::tuple<States...>;

public:
    template <typename... Params>
    Machine(Params&&... params)
        : mStates{std::forward<Params>(params)...}
    {
        detail::apply(mStates, [this](auto& s) { s.setMachine(this); });
    }

    ~Machine() = default;

    void start() { std::get<0>(mStates).entry(); }

    template <typename From, typename To> void transit()
    {
        static_assert(!std::is_same<From, To>::value, "transition to same state");

        std::get<detail::Index<From, TupleType>::value>(mStates).exit();
        mCurrentIndex = detail::Index<To, TupleType>::value;
        std::get<detail::Index<To, TupleType>::value>(mStates).entry();
    }

    template <typename E> void dispatch(const E& event)
    {
        detail::visit(mStates, mCurrentIndex, [this, &event](auto& s) { s.react(event); });
    }

    template <typename S> S& get() { return std::get<S>(mStates); }

private:
    TupleType mStates;
    size_t    mCurrentIndex{0};
};
#else
// C++17
#include <variant>

struct Event
{
};

template <typename T, typename M> class State
{
    friend T;
    template <typename... States> friend class Machine;

public:
    State(const State&)            = delete;
    State& operator=(const State&) = delete;

    State(State&&)            = default;
    State& operator=(State&&) = default;

    virtual ~State() = default;

    template <typename S> void transit() { mMachine->Machine::template transit<S>(); }

private:
    State() = default;

    void setMachine(M* machine) { mMachine = machine; }

private:
    M* mMachine = nullptr;
};

template <typename... States> class Machine final
{
    static_assert(sizeof...(States) > 0, "states are empty");

public:
    template <typename... Params>
    Machine(Params&&... params)
        : mStates{std::forward<Params>(params)...}
    {
        auto l = [this](auto& e) { e.setMachine(this); };
        std::apply([&l](auto&&... x) { (l(x), ...); }, mStates);
    }

    void start()
    {
        std::visit([](auto s) { s->entry(); }, mCurrentState);
    }

    template <typename To> void transit()
    {
        std::visit([](auto s) { s->exit(); }, mCurrentState);
        mCurrentState = &std::get<To>(mStates);
        std::visit([](auto s) { s->entry(); }, mCurrentState);
    }

    template <typename E> void dispatch(const E& event)
    {
        std::visit([&event](auto s) { s->react(event); }, mCurrentState);
    }

private:
    std::tuple<States...>    mStates;
    std::variant<States*...> mCurrentState{&std::get<0>(mStates)};
};
#endif

template <typename M> class MachineDispatcher
{
public:
    explicit MachineDispatcher(M& machine)
        : mMachine{machine}
    {
    }

    template <typename E> void post(const E& event)
    {
        std::lock_guard<std::mutex> lock{mMutex};

        mEvents.emplace_back([event, this]() { mMachine.dispatch(event); });
    }

    void processEvents()
    {
        Events pendingEvents;

        {
            std::lock_guard<std::mutex> lock{mMutex};

            mEvents.swap(pendingEvents);
        }

        for (auto& e : pendingEvents)
        {
            e();
        }
    }

private:
    M&         mMachine;
    std::mutex mMutex;

    using Events = std::deque<std::function<void()>>;
    Events mEvents;
};