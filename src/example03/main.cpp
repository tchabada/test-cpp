#include "machine.hpp"

#include <cstdint>
#include <iostream>

struct ContinueEvent : fsm::Event
{
};

enum class UpdateType : uint8_t
{
    Major,
    Minor
};

struct UpdateEvent : fsm::Event
{
    consteval UpdateEvent(UpdateType tickType)
        : _updateType{tickType}
    {
    }

    UpdateType _updateType;
};

struct MyContext
{
};

struct Stopped;
struct Started;
struct State1;
struct State2;

using MyMachine = fsm::Machine<MyContext, Stopped, Started, State1, State2>;

struct Stopped : public fsm::State<Stopped, MyMachine>
{
    Stopped() {}

    void entry() { std::cout << "STOPPED entry" << std::endl; }
    void exit() { std::cout << "STOPPED exit" << std::endl; }

    void react(fsm::Event const&) {}

    void react(const ContinueEvent&)
    {
        std::cout << "STOPPED react on ContinueEvent" << std::endl;
        transit<Started>();
    }

    void react(UpdateEvent updateEvent)
    {
        std::string updateType = updateEvent._updateType == UpdateType::Major ? "Major" : "Minor";
        std::cout << "STOPPED react on UpdateEvent " << updateType << std::endl;
    }
};

struct Started : public fsm::State<Started, MyMachine>
{
    Started() {}

    void entry() { std::cout << "STARTED entry" << std::endl; }
    void exit() { std::cout << "STARTED exit" << std::endl; }

    void react(fsm::Event const&) {}

    void react(const ContinueEvent&)
    {
        std::cout << "STARTED react on ContinueEvent" << std::endl;
        transit<State1>();
    }

    void react(UpdateEvent updateEvent)
    {
        std::string updateType = updateEvent._updateType == UpdateType::Major ? "Major" : "Minor";
        std::cout << "STOPPED react on UpdateEvent " << updateType << std::endl;
    }
};

struct State1 : public fsm::State<State1, MyMachine>
{
    State1() {}

    void entry() { std::cout << "STATE1 entry" << std::endl; }
    void exit() { std::cout << "STATE1 exit" << std::endl; }

    void react(fsm::Event const&) {}

    void react(const ContinueEvent&)
    {
        std::cout << "STATE1 react on ContinueEvent" << std::endl;
        transit<State2>();
    }

    void react(UpdateEvent updateEvent)
    {
        std::string updateType = updateEvent._updateType == UpdateType::Major ? "Major" : "Minor";
        std::cout << "STOPPED react on UpdateEvent " << updateType << std::endl;
    }
};

struct State2 : public fsm::State<State2, MyMachine>
{
    State2() {}

    void entry() { std::cout << "STATE2 entry" << std::endl; }
    void exit() { std::cout << "STATE2 exit" << std::endl; }

    void react(fsm::Event const&) {}

    void react(const ContinueEvent&)
    {
        std::cout << "STATE2 react on ContinueEvent" << std::endl;
        transit<Stopped>();
    }

    void react(UpdateEvent updateEvent)
    {
        std::string updateType = updateEvent._updateType == UpdateType::Major ? "Major" : "Minor";
        std::cout << "STOPPED react on UpdateEvent " << updateType << std::endl;
    }
};

int main()
{
    MyContext context;
    MyMachine machine{context};

    machine.start();

    machine.dispatch(UpdateEvent{UpdateType::Major});
    machine.dispatch(ContinueEvent{});

    machine.dispatch(UpdateEvent{UpdateType::Major});
    machine.dispatch(ContinueEvent{});

    fsm::MachineDispatcher<MyMachine> dispatcher{machine};

    dispatcher.post(UpdateEvent{UpdateType::Minor});
    dispatcher.post(ContinueEvent{});

    dispatcher.post(UpdateEvent{UpdateType::Minor});
    dispatcher.post(ContinueEvent{});

    dispatcher.processEvents();

    return 0;
}