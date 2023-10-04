#include "machine.hpp"

#include <iostream>

struct ContinueEvent : fsm::Event
{
};

enum class UpdateType : uint8_t
{
    kMajor,
    kMinor
};

struct UpdateEvent : fsm::Event
{
    consteval explicit UpdateEvent(UpdateType tickType)
      : updateType_{tickType}
    {
    }

    UpdateType updateType_;
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
    Stopped() = default;

    static void
    entry()
    {
        std::cout << "STOPPED entry" << '\n';
    }

    static void
    exit()
    {
        std::cout << "STOPPED exit" << '\n';
    }

    static void
    react(fsm::Event const&)
    {
    }

    void
    react(const ContinueEvent&)
    {
        std::cout << "STOPPED react on ContinueEvent" << '\n';
        transit<Started>();
    }

    static void
    react(UpdateEvent updateEvent)
    {
        std::string updateType = updateEvent.updateType_ == UpdateType::kMajor ? "Major" : "Minor";
        std::cout << "STOPPED react on UpdateEvent " << updateType << '\n';
    }
};

struct Started : public fsm::State<Started, MyMachine>
{
    Started() = default;

    static void
    entry()
    {
        std::cout << "STARTED entry" << '\n';
    }

    static void
    exit()
    {
        std::cout << "STARTED exit" << '\n';
    }

    static void
    react(fsm::Event const&)
    {
    }

    void
    react(const ContinueEvent&)
    {
        std::cout << "STARTED react on ContinueEvent" << '\n';
        transit<State1>();
    }

    static void
    react(UpdateEvent updateEvent)
    {
        std::string updateType = updateEvent.updateType_ == UpdateType::kMajor ? "Major" : "Minor";
        std::cout << "STOPPED react on UpdateEvent " << updateType << '\n';
    }
};

struct State1 : public fsm::State<State1, MyMachine>
{
    State1() = default;

    static void
    entry()
    {
        std::cout << "STATE1 entry" << '\n';
    }

    static void
    exit()
    {
        std::cout << "STATE1 exit" << '\n';
    }

    static void
    react(fsm::Event const&)
    {
    }

    void
    react(const ContinueEvent&)
    {
        std::cout << "STATE1 react on ContinueEvent" << '\n';
        transit<State2>();
    }

    static void
    react(UpdateEvent updateEvent)
    {
        std::string updateType = updateEvent.updateType_ == UpdateType::kMajor ? "Major" : "Minor";
        std::cout << "STOPPED react on UpdateEvent " << updateType << '\n';
    }
};

struct State2 : public fsm::State<State2, MyMachine>
{
    State2() = default;

    static void
    entry()
    {
        std::cout << "STATE2 entry" << '\n';
    }

    static void
    exit()
    {
        std::cout << "STATE2 exit" << '\n';
    }

    static void
    react(fsm::Event const&)
    {
    }

    void
    react(const ContinueEvent&)
    {
        std::cout << "STATE2 react on ContinueEvent" << '\n';
        transit<Stopped>();
    }

    static void
    react(UpdateEvent updateEvent)
    {
        std::string updateType = updateEvent.updateType_ == UpdateType::kMajor ? "Major" : "Minor";
        std::cout << "STOPPED react on UpdateEvent " << updateType << '\n';
    }
};

int
main()
{
    MyContext context;
    MyMachine machine{context};

    machine.start();

    machine.dispatch(UpdateEvent{UpdateType::kMajor});
    machine.dispatch(ContinueEvent{});

    machine.dispatch(UpdateEvent{UpdateType::kMajor});
    machine.dispatch(ContinueEvent{});

    fsm::MachineDispatcher<MyMachine> dispatcher{machine};

    dispatcher.post(UpdateEvent{UpdateType::kMinor});
    dispatcher.post(ContinueEvent{});

    dispatcher.post(UpdateEvent{UpdateType::kMinor});
    dispatcher.post(ContinueEvent{});

    dispatcher.processEvents();

    return 0;
}