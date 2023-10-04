#pragma once

#include <type_traits>

namespace mixins
{
template <typename MostDerived>
class Top
{
public:
    using SelfType = MostDerived;

    decltype(auto)
    self() &
    {
        return static_cast<SelfType&>(*this);
    }

    decltype(auto)
    self() &&
    {
        return static_cast<SelfType&&>(*this);
    }

    decltype(auto)
    self() const&
    {
        return static_cast<SelfType const&>(*this);
    }

    decltype(auto)
    self() const&&
    {
        return static_cast<SelfType const&&>(*this);
    }
};

class Deferred
{
public:
    Deferred() = delete;
};

namespace detail
{
template <typename Concrete, template <class> class H, template <class> class... Tail>
class ChainInherit
{
public:
    using Result = typename ChainInherit<Concrete, Tail...>::Type;
    using Type   = H<Result>;
};

template <typename Concrete, template <class> class H>
class ChainInherit<Concrete, H>
{
public:
    using Type = H<Concrete>;
};

template <typename Concrete, template <class> class... Mixins>
using MixinImpl = typename ChainInherit<Top<Concrete>, Mixins...>::Type;
}  // namespace detail

template <typename Concrete, template <class> class... Mixins>
class Mixin : public detail::MixinImpl<Concrete, Mixins...>
{
    template <typename...>
    struct TypeList;

public:
    template <typename... Rest,
              typename = std::enable_if_t<!std::is_same_v<TypeList<Mixin>, TypeList<std::decay_t<Rest>...>>>>
    constexpr explicit Mixin(Rest&&... rest)
      : detail::MixinImpl<Concrete, Mixins...>(static_cast<decltype(rest)>(rest)...)
    {
    }
};

template <template <class...> class Mixin, typename... Args>
class Curry
{
public:
    template <typename Base>
    using Type = Mixin<Args..., Base>;
};

#define DEFAULT_MIXIN_CTOR(CLS, BASE)              \
    template <typename... Args>                    \
    constexpr explicit CLS(Args&&... args)         \
      : BASE(static_cast<decltype(args)>(args)...) \
    {                                              \
    }                                              \
    static_assert(true, "require semicolon")

template <typename Interface, typename Base = Deferred>
class Provides
  : public Base
  , public Interface
{
public:
    template <typename B>
    using Type = typename Curry<Provides, Interface>::template Type<B>;

    DEFAULT_MIXIN_CTOR(Provides, Base);
};

}  // namespace mixins
