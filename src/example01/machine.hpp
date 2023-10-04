#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <tuple>
#include <type_traits>
#include <vector>

// #define NO_VARIANT

#ifndef NO_VARIANT
#include <variant>
#endif

namespace fsm
{

namespace detail
{
template <class T, class Tuple>
struct Index
{
    static_assert(!std::is_same_v<Tuple, std::tuple<>>, "type not found");
};

template <class T, class... Types>
struct Index<T, std::tuple<T, Types...>>
{
    static constexpr std::size_t kValue = 0;
};

template <class T, class U, class... Types>
struct Index<T, std::tuple<U, Types...>>
{
    static constexpr std::size_t kValue = 1 + Index<T, std::tuple<Types...>>::kValue;
};
}  // namespace detail

struct Event
{
};

#ifdef NO_VARIANT
namespace detail
{
template <typename Tuple, typename F, size_t... Is>
constexpr void
forEachImpl(Tuple&& t, F&& f, std::index_sequence<Is...>)
{
    auto&&                      func = std::forward<F>(f);
    std::initializer_list<bool> l    = {((void)func(std::get<Is>(std::forward<Tuple>(t))), false)...};
    static_cast<void>(l);
    // [](...) {}(((void)func(std::get<Is>(std::forward<Tuple>(t))), false)...);
}

template <typename Tuple, typename F>
constexpr void
forEach(Tuple&& t, F&& f)
{
    forEachImpl(std::forward<Tuple>(t),
                std::forward<F>(f),
                std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>{}>{});
}

template <class Tuple, class F, std::size_t... Is>
constexpr void
visitImpl(Tuple&& t, const std::size_t i, F&& f, std::index_sequence<Is...>)
{
    auto&&                      func = std::forward<F>(f);
    std::initializer_list<bool> l    = {(i == Is && (func(std::get<Is>(std::forward<Tuple>(t))), false))...};
    static_cast<void>(l);
    // [](...) {}((i == Is && ((void)func(std::get<Is>(std::forward<Tuple>(t))), false))...);
}

template <class Tuple, class F>
constexpr void
visit(Tuple&& t, const std::size_t i, F&& f)
{
    visitImpl(std::forward<Tuple>(t),
              i,
              std::forward<F>(f),
              std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>{}>{});
}
}  // namespace detail

template <typename T, typename M>
class State
{
    friend T;
    friend M;

public:
    State(const State&)            = default;
    State& operator=(const State&) = delete;
    State(State&&)                 = default;
    State& operator=(State&&)      = delete;

    virtual ~State() = default;

    template <typename S>
    void
    transit()
    {
        machine_->M::template transit<T, S>();
    }

protected:
    decltype(auto)
    context()
    {
        return machine_->context();
    }

    decltype(auto)
    context() const
    {
        return machine_->context();
    }

private:
    State() = default;

    void
    setMachine(M* machine)
    {
        machine_ = machine;
    }

private:
    M* machine_ = nullptr;
};

template <typename Context, typename... States>
class Machine final
{
    static_assert(sizeof...(States) > 0, "states are empty");

    using TupleType = std::tuple<States...>;

public:
    template <typename... Params>
    explicit Machine(Context& context,
                     Params&&... params) noexcept(std::is_nothrow_constructible_v<TupleType, Params&&...>)
      : context_{context}
      , states_{std::forward<Params>(params)...}
    {
        detail::forEach(states_, [this](auto& s) {
            s.setMachine(this);
        });
    }

    template <typename T = TupleType, typename = std::enable_if_t<std::is_copy_constructible_v<T>>>
    explicit Machine(const Machine& other)
      : context_{other.context_}
      , states_{other.states_}
      , currentIndex_{other.currentIndex_}
    {
        detail::forEach(states_, [this](auto& s) -> void {
            s.setMachine(this);
        });
    }

    template <typename T = TupleType, typename = std::enable_if_t<std::is_move_constructible_v<T>>>
    explicit Machine(Machine&& other)
      : context_{other.context_}
      , states_{std::move(other.states_)}
      , currentIndex_{other.currentIndex_}
    {
        detail::forEach(states_, [this](auto& s) -> void {
            s.setMachine(this);
        });
    }

    Machine& operator=(const Machine&) = delete;
    Machine& operator=(Machine&&)      = delete;

    ~Machine() = default;

    void
    start()
    {
        std::get<0>(states_).entry();
    }

    template <typename From, typename To>
    void
    transit()
    {
        static_assert(!std::is_same_v<From, To>, "transition to same state");

        std::get<detail::Index<From, TupleType>::kValue>(states_).exit();
        currentIndex_ = detail::Index<To, TupleType>::kValue;
        std::get<detail::Index<To, TupleType>::kValue>(states_).entry();
    }

    template <typename E>
    void
    dispatch(const E& event)
    {
        detail::visit(states_, currentIndex_, [&event](auto& s) {
            s.react(event);
        });
    }

    template <typename S>
    S&
    state()
    {
        return std::get<S>(states_);
    }

    size_t
    currentIndex() const
    {
        return currentIndex_;
    }
    template <typename S>
    static constexpr size_t
    stateIndex()
    {
        return detail::Index<S, TupleType>::kValue;
    }

    template <typename F>
    void
    visit(F&& f) const
    {
        detail::visit(states_, currentIndex_, std::forward<F>(f));
    }

    Context&
    context()
    {
        return context_;
    }

    const Context&
    context() const
    {
        return context_;
    }

private:
    Context&  context_;
    TupleType states_;
    size_t    currentIndex_{0};
};
#else
template <typename T, typename M>
class State
{
    friend T;
    friend M;

public:
    State(const State&)            = default;
    State& operator=(const State&) = delete;
    State(State&&)                 = default;
    State& operator=(State&&)      = delete;

    virtual ~State() = default;

    template <typename S>
    void
    transit()
    {
        machine_->Machine::template transit<S>();
    }

protected:
    decltype(auto)
    context()
    {
        return machine_->context();
    }
    decltype(auto)
    context() const
    {
        return machine_->context();
    }

private:
    State() = default;

    void
    setMachine(M* machine)
    {
        machine_ = machine;
    }

private:
    M* machine_ = nullptr;
};

template <typename Context, typename... States>
class Machine final
{
    static_assert(sizeof...(States) > 0, "states are empty");

    using TupleType = std::tuple<States...>;

public:
    template <typename... Params>
    explicit Machine(Context& data,
                     Params&&... params) noexcept(std::is_nothrow_constructible_v<TupleType, Params&&...>)
      : context_{data}
      , states_{std::forward<Params>(params)...}
    {
        auto l = [this](auto& e) {
            e.setMachine(this);
        };

        std::apply(
            [&l](auto&&... x) {
                (l(x), ...);
            },
            states_);
    }

    template <typename T = TupleType, typename = std::enable_if_t<std::is_copy_constructible_v<T>>>
    explicit Machine(const Machine& other)
      : context_{other.context_}
      , states_{other.states_}
    {
        auto l = [this](auto& e) {
            e.setMachine(this);
        };

        std::apply(
            [&l](auto&&... x) {
                (l(x), ...);
            },
            states_);
    }

    template <typename T = TupleType, typename = std::enable_if_t<std::is_move_constructible_v<T>>>
    explicit Machine(Machine&& other)
      : context_{other.context_}
      , states_{std::move(other.states_)}
    {
        auto l = [this](auto& e) {
            e.setMachine(this);
        };

        std::apply(
            [&l](auto&&... x) {
                (l(x), ...);
            },
            states_);
    }

    ~Machine() = default;

    void
    start()
    {
        std::visit(
            [](auto s) {
                s->entry();
            },
            currentState_);
    }

    template <typename To>
    void
    transit()
    {
        std::visit(
            [](auto s) {
                s->exit();
            },
            currentState_);

        currentState_ = &std::get<To>(states_);

        std::visit(
            [](auto s) {
                s->entry();
            },
            currentState_);
    }

    template <typename E>
    void
    dispatch(const E& event)
    {
        std::visit(
            [&event](auto s) {
                s->react(event);
            },
            currentState_);
    }

    template <typename S>
    S&
    state()
    {
        return std::get<S>(states_);
    }

    constexpr size_t
    currentIndex() const
    {
        return currentState_.index();
    }

    template <typename S>
    static constexpr size_t
    stateIndex()
    {
        return detail::Index<S, TupleType>::kValue;
    }

    template <typename F>
    void
    visit(F&& f) const
    {
        std::visit(std::forward<F>(f), currentState_);
    }

    Context&
    context()
    {
        return context_;
    }
    const Context&
    context() const
    {
        return context_;
    }

private:
    Context&                 context_;
    std::tuple<States...>    states_;
    std::variant<States*...> currentState_{&std::get<0>(states_)};
};
#endif

template <typename M>
class MachineDispatcher
{
public:
    explicit MachineDispatcher(M& machine)
      : machine_{machine}
    {
    }

    template <typename E>
    void
    post(const E& event)
    {
        std::lock_guard<std::mutex> lock{mutex_};

        events_.emplace_back([event, this]() {
            machine_.dispatch(event);
        });
    }

    void
    processEvents()
    {
        Events pendingEvents;

        {
            std::lock_guard<std::mutex> lock{mutex_};

            events_.swap(pendingEvents);
        }

        for (auto& event : pendingEvents) {
            event();
        }
    }

private:
    M&         machine_;
    std::mutex mutex_;

    using Events = std::vector<std::function<void()>>;
    Events events_;
};
}  // namespace fsm