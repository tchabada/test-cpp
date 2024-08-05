#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "example03/machine.hpp"

using namespace testing;

struct Toggle : fsm::Event
{
};

struct Update : fsm::Event
{
};

struct MyContext
{
};

struct Started;
struct Stopped;

using MyMachine = fsm::Machine<MyContext, Stopped, Started>;

struct Started : public fsm::State<Started, MyMachine>
{
    void react(const fsm::Event&) {}

    MOCK_METHOD(void, entry, ());
    MOCK_METHOD(void, exit, ());
    MOCK_METHOD(void, react, (const Toggle&));
    MOCK_METHOD(void, react, (const Update&));
};

struct Stopped : public fsm::State<Stopped, MyMachine>
{
    void react(const fsm::Event&) {}

    MOCK_METHOD(void, entry, ());
    MOCK_METHOD(void, exit, ());
    MOCK_METHOD(void, react, (const Toggle&));
    MOCK_METHOD(void, react, (const Update&));
};

TEST(FsmTest, testConstruct)
{
    MyContext context;
    MyMachine machine{context};
}

TEST(FsmTest, testTransition)
{
    MyContext context;
    MyMachine machine{context};

    EXPECT_CALL(machine.state<Stopped>(), entry()).Times(2);
    EXPECT_CALL(machine.state<Stopped>(), exit()).Times(1);
    EXPECT_CALL(machine.state<Stopped>(), react(An<const Update&>())).Times(1);
    EXPECT_CALL(machine.state<Stopped>(), react(An<const Toggle&>()))
        .WillOnce([&machine](const Toggle&) { machine.state<Stopped>().transit<Started>(); });

    EXPECT_CALL(machine.state<Started>(), entry()).Times(1);
    EXPECT_CALL(machine.state<Started>(), exit()).Times(1);
    EXPECT_CALL(machine.state<Started>(), react(An<const Update&>())).Times(1);
    EXPECT_CALL(machine.state<Started>(), react(An<const Toggle&>()))
        .WillOnce([&machine](const Toggle&) { machine.state<Started>().transit<Stopped>(); });

    machine.start();
    ASSERT_TRUE(machine.currentIndex() == MyMachine::stateIndex<Stopped>());

    machine.dispatch(Update{});
    machine.dispatch(Toggle{});
    ASSERT_TRUE(machine.currentIndex() == MyMachine::stateIndex<Started>());

    machine.dispatch(Update{});
    machine.dispatch(Toggle{});
    ASSERT_TRUE(machine.currentIndex() == MyMachine::stateIndex<Stopped>());

    EXPECT_CALL(machine.state<Stopped>(), react(An<const Update&>())).Times(1);

    fsm::MachineDispatcher<MyMachine> dispacther{machine};
    dispacther.post(Update{});
    dispacther.processEvents();
}
