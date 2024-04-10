#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include <static_factory.hpp>

class BaseClass
{
  public:
  virtual ~BaseClass()         = default;
  virtual int getValue() const = 0;
};

class ConcreteClassA : public BaseClass
{
  public:
  int getValue() const override
  {
    return 42;
  }
};

class ConcreteClassB : public BaseClass
{
  public:
  int getValue() const override
  {
    return 84;
  }
};

TEST_CASE("raw pointers")
{
  static_factory<BaseClass>::register_type<ConcreteClassA>("ClassA");
  static_factory<BaseClass>::register_type<ConcreteClassB>("ClassB");

  SECTION("create Objects")
  {
    auto objA = static_factory<BaseClass>::make_ptr("ClassA");
    auto objB = static_factory<BaseClass>::make_ptr("ClassB");

    REQUIRE(objA->getValue() == 42);
    REQUIRE(objB->getValue() == 84);
  }

  SECTION("try Create Objects")
  {
    auto obj = static_factory<BaseClass>::try_make_ptr();

    REQUIRE(obj->getValue() == 42);
  }
}

TEST_CASE("shared Pointers")
{
  static_factory<BaseClass>::register_type<ConcreteClassA>("ClassA");
  static_factory<BaseClass>::register_type<ConcreteClassB>("ClassB");

  SECTION("create Shared Objects")
  {
    auto objA = static_factory<BaseClass>::make_shared("ClassA");
    auto objB = static_factory<BaseClass>::make_shared("ClassB");

    REQUIRE(objA->getValue() == 42);
    REQUIRE(objB->getValue() == 84);
  }

  SECTION("try Create Shared Objects")
  {
    auto obj = static_factory<BaseClass>::try_make_shared();

    REQUIRE(obj->getValue() == 42);
  }
}

TEST_CASE("factory: unique Pointers")
{
  static_factory<BaseClass>::register_type<ConcreteClassA>("ClassA");
  static_factory<BaseClass>::register_type<ConcreteClassB>("ClassB");

  SECTION("factory: create Unique Objects")
  {
    auto objA = static_factory<BaseClass>::make_unique("ClassA");
    auto objB = static_factory<BaseClass>::make_unique("ClassB");

    REQUIRE(objA->getValue() == 42);
    REQUIRE(objB->getValue() == 84);
  }

  SECTION("factory: try Create Unique Objects")
  {
    auto obj = static_factory<BaseClass>::try_make_unique();

    REQUIRE(obj->getValue() == 42);
  }
}

struct dog
{
  std::string name;
  void eat()
  {
  }
  void bark()
  {
  }
};

struct cat
{
  std::string name;
  void eat()
  {
  }
  void meow()
  {
  }
};

using pet = std::variant<dog, cat>;

TEST_CASE("variant")
{
  static_factory<pet>::register_type<dog>("dog");
  static_factory<pet>::register_type<cat>("cat");
  static_factory<pet>::register_function("my_cat",
    []()
    {
      cat anber;
      anber.name = "Anber";
      return anber;
    });

  SECTION("create Objects")
  {
    auto objA  = static_factory<pet>::make("dog");
    auto objB  = static_factory<pet>::make("cat");
    auto anber = static_factory<pet>::make("my_cat");

    REQUIRE(std::holds_alternative<dog>(objA));
    REQUIRE(std::holds_alternative<cat>(objB));
    REQUIRE(std::holds_alternative<cat>(anber));
    REQUIRE(std::get<cat>(anber).name == "Anber");
  }

  SECTION("try Create Objects")
  {
    auto obj = static_factory<pet>::try_make();

    REQUIRE(std::holds_alternative<dog>(obj));
  }
}

int main(int argc, char* argv[])
{
  return Catch::Session().run(argc, argv);
}