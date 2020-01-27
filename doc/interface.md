# The Imprimis Interface Specification

#### Note that this is a draft and subject to expansion and modification.

## Chapters

#### 1. Usage
* 1.1 What is an interface
* 1.2 Augmenting an interface

# 1. Usage

## 1.1 What is an interface

An interface describes the behavior and capabilities of the game and its engine. They allow for a structured self-documenting object-oriented codebase with more predictable behaviors and outputs.

In C++, interfaces are declared in a header file (.h) and defined in a source code file (.cpp).

## 1.2 Augmenting an interface

Below is a code example of an interface declaration and definition.

Take note of the implicit and explicit references to ``ExampleInterface`` in both files.

```cpp
// game.h

namespace game
{
    class ExampleInterface
    {
    private:
        int var1, var2;
        const char *var3;
        void func1(int a, int b);
    public:
        ExampleInterface(int c);
        bool func2(const char *d, bool e);
    };
}


// example.cpp

namespace game
{
    ExampleInterface::ExampleInterface(int c)
    {
        var1 = c;
        var2 = c+1;
        // ...
        return;
    }

    void ExampleInterface::func1(int a, int b)
    {
        // ...
        return;
    }

    bool ExampleInterface::func2(const char *d, bool e)
    {
        var3 = d;
        // ...
        return e;
    }
}
```