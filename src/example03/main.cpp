#include "machine.hpp"

#include <iostream>

struct Continue : Event
{
};

struct Context
{
};

struct Stopped;
struct Started;
struct State1;
struct State2;

using MyMachine = Machine<Context, Stopped, Started, State1, State2>;

struct Data
{
};

struct Stopped : public State<Stopped, MyMachine>
{
    Stopped() {}

    void entry() { std::cout << "STOPPED entry" << std::endl; }
    void exit() { std::cout << "STOPPED exit" << std::endl; }

    void react(Event const&) {}
    void react(const Continue&)
    {
        std::cout << "STOPPED react on Continue" << std::endl;
        transit<Started>();
    };
};

struct Started : public State<Started, MyMachine>
{
    Started() {}

    void entry() { std::cout << "STARTED entry" << std::endl; }
    void exit() { std::cout << "STARTED exit" << std::endl; }

    void react(Event const&) {}
    void react(const Continue&)
    {
        std::cout << "STARTED react on Continue" << std::endl;
        transit<State1>();
    };
};

struct State1 : public State<State1, MyMachine>
{
    State1() {}

    void entry() { std::cout << "STATE1 entry" << std::endl; }
    void exit() { std::cout << "STATE1 exit" << std::endl; }

    void react(Event const&) {}
    void react(const Continue&)
    {
        std::cout << "STATE1 react on Continue" << std::endl;
        transit<State2>();
    };
};

struct State2 : public State<State2, MyMachine>
{
    State2() {}

    void entry() { std::cout << "STATE2 entry" << std::endl; }
    void exit() { std::cout << "STATE2 exit" << std::endl; }

    void react(Event const&) {}
    void react(const Continue&)
    {
        std::cout << "STATE2 react on Continue" << std::endl;
        transit<Stopped>();
    };
};

int main()
{
    Context   context;
    MyMachine machine{context, Stopped{}, Started{}, State1{}, State2{}};
    machine.start();

    machine.dispatch(Continue{});
    machine.dispatch(Continue{});

    MachineDispatcher<MyMachine> dispatcher{machine};
    dispatcher.post(Continue{});
    dispatcher.post(Continue{});
    dispatcher.processEvents();

    if (machine.currentIndex() == machine.stateIndex<Stopped>())
    {
        std::cout << "STOPPED" << std::endl;
    }

    return 0;
}