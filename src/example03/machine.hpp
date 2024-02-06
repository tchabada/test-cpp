#pragma once

#include <cstddef>
#include <functional>
#include <iostream>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <vector>

#define NO_VARIANT

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
} // namespace detail

#ifdef NO_VARIANT
// C++14
namespace detail
{
template <typename Tuple, typename F, size_t... Is>
constexpr void forEachImpl(Tuple&& t, F&& f, std::index_sequence<Is...>)
{
    auto&&                      func = std::forward<F>(f);
    std::initializer_list<bool> l    = {((void)func(std::get<Is>(std::forward<Tuple>(t))), false)...};
    static_cast<void>(l);
    // [](...) {}(((void)func(std::get<Is>(std::forward<Tuple>(t))), false)...);
}

template <typename Tuple, typename F> constexpr void forEach(Tuple&& t, F&& f)
{
    forEachImpl(std::forward<Tuple>(t), std::forward<F>(f),
                std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>{}>{});
}

template <class Tuple, class F, std::size_t... Is>
constexpr void visitImpl(Tuple&& t, const std::size_t i, F&& f, std::index_sequence<Is...>)
{
    auto&&                      func = std::forward<F>(f);
    std::initializer_list<bool> l    = {(i == Is && (func(std::get<Is>(std::forward<Tuple>(t))), false))...};
    static_cast<void>(l);
    // [](...) {}((i == Is && ((void)func(std::get<Is>(std::forward<Tuple>(t))), false))...);
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
    friend M;

public:
    State(const State&)            = delete;
    State& operator=(const State&) = delete;

    State(State&&)            = default;
    State& operator=(State&&) = default;

    virtual ~State() = default;

    template <typename S> void transit() { mMachine->M::template transit<T, S>(); }

protected:
    decltype(auto) context() { return mMachine->context(); }
    decltype(auto) context() const { return mMachine->context(); }

private:
    State() = default;

    void setMachine(M* machine) { mMachine = machine; }

private:
    M* mMachine = nullptr;
};

template <typename Context, typename... States> class Machine final
{
    static_assert(sizeof...(States) > 0, "states are empty");

    using TupleType = std::tuple<States...>;

public:
    template <typename... Params>
    explicit Machine(Context& data,
                     Params&&... params) noexcept(std::is_nothrow_constructible<TupleType, Params&&...>::value)
        : mContext{data}
        , mStates{std::forward<Params>(params)...}
    {
        detail::forEach(mStates, [this](auto& s) { s.setMachine(this); });
    }

    Machine(const Machine&)            = delete;
    Machine& operator=(const Machine&) = delete;
    Machine(Machine&&)                 = delete;
    Machine& operator=(Machine&&)      = delete;

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

    template <typename S> S& state() { return std::get<S>(mStates); }

    size_t                                        currentIndex() const { return mCurrentIndex; }
    template <typename S> static constexpr size_t stateIndex() { return detail::Index<S, TupleType>::value; }

    template <typename F> void visit(F&& f) const { detail::visit(mStates, mCurrentIndex, std::forward<F>(f)); }

    Context&       context() { return mContext; }
    const Context& context() const { return mContext; }

private:
    Context&  mContext;
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
    friend M;

public:
    State(const State&)            = delete;
    State& operator=(const State&) = delete;

    State(State&&)            = default;
    State& operator=(State&&) = default;

    virtual ~State() = default;

    template <typename S> void transit() { mMachine->Machine::template transit<S>(); }

protected:
    decltype(auto) context() { return mMachine->context(); }
    decltype(auto) context() const { return mMachine->context(); }

private:
    State() = default;

    void setMachine(M* machine) { mMachine = machine; }

private:
    M* mMachine = nullptr;
};

template <typename Context, typename... States> class Machine final
{
    static_assert(sizeof...(States) > 0, "states are empty");

    using TupleType = std::tuple<States...>;

public:
    template <typename... Params>
    explicit Machine(Context& data,
                     Params&&... params) noexcept(std::is_nothrow_constructible<TupleType, Params&&...>::value)
        : mContext{data}
        , mStates{std::forward<Params>(params)...}
    {
        auto l = [this](auto& e) { e.setMachine(this); };
        std::apply([&l](auto&&... x) { (l(x), ...); }, mStates);
    }

    Machine(const Machine&)            = delete;
    Machine& operator=(const Machine&) = delete;
    Machine(Machine&&)                 = delete;
    Machine& operator=(Machine&&)      = delete;

    ~Machine() = default;

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

    template <typename S> S& state() { return std::get<S>(mStates); }

    constexpr size_t                              currentIndex() const { return mCurrentState.index(); }
    template <typename S> static constexpr size_t stateIndex() { return detail::Index<S, TupleType>::value; }

    template <typename F> void visit(F&& f) const { std::visit(std::forward<F>(f), mCurrentState); }

    Context&       context() { return mContext; }
    const Context& context() const { return mContext; }

private:
    Context&                 mContext;
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

    using Events = std::vector<std::function<void()>>;
    Events mEvents;
};