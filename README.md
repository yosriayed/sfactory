# static_factory

A header only static implementation for factory pattern. 

## Usage samples

#### Polymorphic types
```c++
class Pet
{
  public:
  virtual ~Pet()                    = default;
  virtual std::string speak() const = 0;
};

class Dog : public Pet
{
  public:
  std::string speak() const override
  {
    return "Woof!";
  }
};

class Cat : public Pet
{
  public:
  std::string speak() const override
  {
    return "Meow!";
  }
};

int main()
{
  using pet_factory = static_factory<Pet, std::string>;

  // Register types

  pet_factory::register_type<Dog>("Dog");
  pet_factory::register_type<Cat>("Cat");

  // Create raw pointers
  {
    Pet* dog = pet_factory::make_ptr("Dog");
    Pet* cat = pet_factory::make_ptr("Cat");

    // try to loop through all the registered keys and return the first one that is not nullptr
    Pet* pet = pet_factory::try_make_ptr();
    assert(pet->speak() == "Woof!");

    delete dog;
    delete cat;
    delete pet;
  }

  // Create unique pointers
  {
    std::unique_ptr<Pet> dog = pet_factory::make_unique("Dog");
    std::unique_ptr<Pet> cat = pet_factory::make_unique("Cat");

    // try to loop through all the registered keys and return the first one that is not nullptr
    std::unique_ptr<Pet> pet = pet_factory::try_make_unique();
    assert(pet->speak() == "Woof!");
  }

  // Create shared pointers
  {
    std::shared_ptr<Pet> dog = pet_factory::make_shared("Dog");
    std::shared_ptr<Pet> cat = pet_factory::make_shared("Cat");

    // try to loop through all the registered keys and return the first one that is not nullptr
    std::shared_ptr<Pet> pet = pet_factory::try_make_shared();
    assert(pet->speak() == "Woof!");
  }

  return 0;
}
```
#### Component types

This is where the base_type is constructible from registered types

```cpp

#include "static_factory.hpp"
#include <variant>

struct Dog
{
  Dog(const std::string& n = "") :name{n}{}

  std::string name;
  void eat()
  {
  }
  void bark()
  {
  }
};

struct Cat
{
  Cat(const std::string& n = "") :name{n}{}
  std::string name;
  void eat()
  {
  }
  void meow()
  {
  }
};


using Pet = std::variant<Dog, Cat>;

int main()
{
  using pet_factory = static_factory<Pet, std::string>;

  // Register types
  pet_factory::register_type<Dog, std::string>("Dog");
  pet_factory::register_type<Cat, std::string>("Cat");

  // register a custom function

  pet_factory::register_function("MyCat",
    []()
    {
      Cat my_cat;
      my_cat.name = "Anber";
      return my_cat;
    });

  Pet dog = pet_factory::make("Dog", std::string("Rex"));
  assert(std::holds_alternative<Dog>(dog));

  Pet cat = pet_factory::make("Cat", std::string("Whiskers"));
  assert(std::holds_alternative<Cat>(cat));
  Pet anber = pet_factory::make("MyCat");
  assert(std::holds_alternative<Cat>(anber));
  assert(std::get<Cat>(anber).name == "Anber");

  return 0;
}

```

## Limitations

Passing arguments to the `make` methods is explicit. In the same sense of explicit constructors: No implicit conversions are made.

For example, calling `Pet dog = pet_factory::make("Dog", "Rex");` in the previous example will throw because no factory is registered with the argument `const char*`. And calling "make_ptr" will return a nullptr instead of throwing.

A one could argue this is a feature and not a limitation.