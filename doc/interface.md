# The Libprimis Interface Specification

#### Note that this is a draft and subject to expansion and modification.

## Chapters

#### 1. Usage
* 1.1 What is an interface
* 1.2 Augmenting an interface

#### 2. Standard Interfaces
* 2.1 Game
    * 2.1.1 Player
    * 2.1.2 Weapon
    * 2.1.3 Projectile

# 1. Usage

## 1.1 What is an interface

An interface describes the behavior and capabilities of the game and its engine. They allow for a structured self-documenting object-oriented codebase with more predictable behaviors and outputs.

In C++, interfaces are declared in a header file (.h) and defined in a source code file (.cpp).

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

## 1.2 Augmenting an interface

To augment an interface, you need to understand the following:

* Action of your method (Get, Set, Replace, Add, etc.)
* Target of your method (What is ultimately changed)
* Object of your method (What property is being acted upon)

To give an example, let's say you want to ``Set`` a ``Player``'s ``Weapon``.

As the target of our method is a ``Player`` object, we will be placing it inside a ``Player`` interface with the prefix ``Set`` and the suffix ``Weapon``.

This logic helps keep the codebase self-documenting and the interfaces predictable. You will then be calling ``PlayerObject::set_weapon("pistol");`` to access the method.

# 2. Standard Interfaces

Standard interfaces are hard-coded in C++ and define the engine's capabilities.

## 2.1 Game

The ``Game`` interface keeps track of gameplay elements, such as players, weapons, projectiles as well as their states, properties and methods.

### 2.1.1 Player