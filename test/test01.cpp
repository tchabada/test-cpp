#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "example01/machine.hpp"

struct ToggleEvent : fsm::Event
{
};

struct UpdateEvent : fsm::Event
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
    static void
    react(const fsm::Event&)
    {
    }

    MOCK_METHOD(void, entry, ());
    MOCK_METHOD(void, exit, ());
    MOCK_METHOD(void, react, (const ToggleEvent&));
    MOCK_METHOD(void, react, (const UpdateEvent&));
};

struct Stopped : public fsm::State<Stopped, MyMachine>
{
    static void
    react(const fsm::Event&)
    {
    }

    MOCK_METHOD(void, entry, ());
    MOCK_METHOD(void, exit, ());
    MOCK_METHOD(void, react, (const ToggleEvent&));
    MOCK_METHOD(void, react, (const UpdateEvent&));
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
    EXPECT_CALL(machine.state<Stopped>(), react(testing::An<const UpdateEvent&>())).Times(1);
    EXPECT_CALL(machine.state<Stopped>(), react(testing::An<const ToggleEvent&>()))
        .WillOnce([&machine](const ToggleEvent&) {
            machine.state<Stopped>().transit<Started>();
        });

    EXPECT_CALL(machine.state<Started>(), entry()).Times(1);
    EXPECT_CALL(machine.state<Started>(), exit()).Times(1);
    EXPECT_CALL(machine.state<Started>(), react(testing::An<const UpdateEvent&>())).Times(1);
    EXPECT_CALL(machine.state<Started>(), react(testing::An<const ToggleEvent&>()))
        .WillOnce([&machine](const ToggleEvent&) {
            machine.state<Started>().transit<Stopped>();
        });

    machine.start();
    ASSERT_TRUE(machine.currentIndex() == MyMachine::stateIndex<Stopped>());

    machine.dispatch(UpdateEvent{});
    machine.dispatch(ToggleEvent{});
    ASSERT_TRUE(machine.currentIndex() == MyMachine::stateIndex<Started>());

    machine.dispatch(UpdateEvent{});
    machine.dispatch(ToggleEvent{});
    ASSERT_TRUE(machine.currentIndex() == MyMachine::stateIndex<Stopped>());

    EXPECT_CALL(machine.state<Stopped>(), react(testing::An<const UpdateEvent&>())).Times(1);

    fsm::MachineDispatcher<MyMachine> dispacther{machine};
    dispacther.post(UpdateEvent{});
    dispacther.processEvents();
}
