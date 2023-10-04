#include "example03/mixins.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

template <typename Base>
struct NormalMixin : Base
{
    DEFAULT_MIXIN_CTOR(NormalMixin, Base);

    void
    f() const
    {
    }
};

template <typename Base>
struct MixinWithParameter : Base
{
    template <typename... Args>
    explicit MixinWithParameter(int x, Args&&... rest)
      : Base(std::forward<Args>(rest)...)
      , x_{x}
    {
    }

    int
    getInt() const
    {
        this->self().f();
        return x_;
    }

private:
    int x_;
};

template <typename T, typename Base>
struct TemplatedMixinWithParameter : Base
{
    template <typename... Args>
    explicit TemplatedMixinWithParameter(const T& x, Args&&... rest)
      : Base(std::forward<Args>(rest)...)
      , x_{x}
    {
    }

    const T&
    get() const
    {
        return x_;
    };

private:
    T x_;
};

struct Concrete
  : mixins::Mixin<Concrete, NormalMixin, MixinWithParameter, mixins::Curry<TemplatedMixinWithParameter, float>::Type>
{
    Concrete(int x, float y)
      : Mixin(x, y)
    {
    }
};

struct MyMessage
{
};

struct AbstractSocket
{
    virtual void receive(const MyMessage&) = 0;
    virtual void send(const MyMessage&)    = 0;
    virtual void update()                  = 0;

    virtual ~AbstractSocket() = default;
};

template <typename Base>
struct Sender : Base
{
    DEFAULT_MIXIN_CTOR(Sender, Base);
    MOCK_METHOD(void, send, (const MyMessage&), (override));
    ~Sender() override = default;
};

template <typename Base>
struct Receiver : Base
{
    DEFAULT_MIXIN_CTOR(Receiver, Base);
    MOCK_METHOD(void, receive, (const MyMessage&), (override));
    ~Receiver() override = default;
};

template <typename Base>
struct Updater : Base
{
    DEFAULT_MIXIN_CTOR(Updater, Base);
    MOCK_METHOD(void, update, (), (override));
    ~Updater() override = default;
};

template <typename Base>
struct IsSocket
  : Base
  , AbstractSocket
{
    DEFAULT_MIXIN_CTOR(IsSocket, Base);
};

struct Socket1 : mixins::Mixin<Socket1, Sender, Receiver, Updater, IsSocket>
{
};

struct Socket2 : mixins::Mixin<Socket2, Sender, Receiver, Updater, mixins::Provides<AbstractSocket>::Type>
{
};

struct Socket3
  : mixins::Mixin<Socket3, Sender, Receiver, Updater, mixins::Curry<mixins::Provides, AbstractSocket>::Type>
{
};

class MixinsTest : public testing::Test
{
public:
    void
    SetUp() override
    {
    }
};

TEST_F(MixinsTest, testMixins)
{
    using testing::_;

    Concrete c{1, 1.0F};

    ASSERT_EQ(c.getInt(), 1);
    ASSERT_EQ(c.get(), 1.0F);

    Socket1 s1;

    EXPECT_CALL(s1, send(_)).Times(1);
    EXPECT_CALL(s1, receive(_)).Times(1);
    EXPECT_CALL(s1, update()).Times(1);

    s1.send(MyMessage{});
    s1.receive(MyMessage{});

    Socket2 s2;

    EXPECT_CALL(s2, send(_)).Times(1);
    EXPECT_CALL(s2, receive(_)).Times(1);
    EXPECT_CALL(s2, update()).Times(1);

    s2.send(MyMessage{});
    s2.receive(MyMessage{});

    Socket3 s3;

    EXPECT_CALL(s3, send(_)).Times(1);
    EXPECT_CALL(s3, receive(_)).Times(1);
    EXPECT_CALL(s3, update()).Times(1);

    s3.send(MyMessage{});
    s3.receive(MyMessage{});

    std::vector<AbstractSocket*> sockets{&s1, &s2, &s3};

    for (auto* s : sockets) {
        s->update();
    }
}