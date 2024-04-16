#ifndef STATIC_FACTORY_H
#define STATIC_FACTORY_H

#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace detail
{

template <typename base_type, typename T>
constexpr bool is_related_v =
  std::is_base_of_v<base_type, T> || std::is_convertible_v<T, base_type>;

template <typename Class, template <typename...> class Template>
struct is_specialization_of : std::false_type
{
};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template<Args...>, Template> : std::true_type
{
};

template <typename Class, template <typename...> class Template>
constexpr auto is_specialization_of_v = is_specialization_of<Class, Template>::value;

template <typename T>
struct get_template_arg_type_of;

template <template <typename...> typename C, typename... Args>
struct get_template_arg_type_of<C<Args...>>
{
  using arguments           = std::tuple<Args...>;
  static const size_t nargs = sizeof...(Args);
  template <size_t i>
  struct arg
  {
    using type = typename std::tuple_element<i, arguments>::type;
  };
};

template <typename Key, typename Value>
class unordered_flat_map
{
  public:
  using value_type = Value;
  using key_type   = Key;

  private:
  std::vector<std::pair<key_type, value_type>> m_data;

  public:
  void insert(const key_type& key, const value_type& value)
  {
    auto it = std::find_if(m_data.begin(),
      m_data.end(),
      [&key](const auto& p)
      {
        return p.first == key;
      });
    if(it == m_data.end())
    {
      m_data.emplace_back(key, value);
    }
    else
    {
      it->second = value;
    }
  }

  value_type& operator[](const key_type& key)
  {
    auto it = std::find_if(m_data.begin(),
      m_data.end(),
      [&key](const auto& p)
      {
        return p.first == key;
      });
    if(it == m_data.end())
    {
      m_data.emplace_back(key, value_type());
      return m_data.back().second;
    }
    else
    {
      return it->second;
    }
  }

  auto find(const key_type& key)
  {
    return std::find_if(m_data.begin(),
      m_data.end(),
      [&key](const auto& p)
      {
        return p.first == key;
      });
  }

  auto begin()
  {
    return m_data.begin();
  }

  auto end()
  {
    return m_data.end();
  }
};

} // namespace detail

/**
 * \brief The static_factory class provides a static factory pattern implementation.
 *
 * The static_factory class allows registration of types and functions, and provides
 * methods to create instances of registered types. It supports creating instances
 * as BaseType, raw pointers, shared pointers, and unique pointers.
 *
 * \tparam BaseType The base type of the registered types.
 * \tparam KeyType The type of the key used for registration. (default is std::string)
 */
template <typename BaseType, typename KeyType = std::string>
class static_factory
{
  public:
  using base_type = BaseType;
  using key_type  = KeyType;

  template <typename Func>
  static void register_function(const key_type& key, Func&& func)
  {
    register_function_impl(key, std::function(std::forward<Func>(func)));
  }

  template <typename ConcreteType, typename... Args>
  static void register_type(const key_type& key)
  {
    static_assert(detail::is_related_v<base_type, ConcreteType>, "Invalid type");

    std::lock_guard lock(g_mutex);

    auto hash = g_hash_function(key);

    if constexpr(std::is_convertible_v<ConcreteType, base_type>)
    {
      get_registry<base_type, Args...>()[hash] = [](Args&&... args)
      {
        return ConcreteType(std::forward<Args>(args)...);
      };
    }
    else if constexpr(std::is_base_of_v<base_type, ConcreteType>)
    {
      {
        get_registry<base_type*, Args...>()[hash] = [](Args&&... args)
        {
          return new ConcreteType(std::forward<Args>(args)...);
        };
      }

      {
        get_registry<std::shared_ptr<base_type>, Args...>()[hash] = [](Args&&... args)
        {
          return std::make_shared<ConcreteType>(std::forward<Args>(args)...);
        };
      }

      {
        get_registry<std::unique_ptr<base_type>, Args...>()[hash] = [](Args&&... args)
        {
          return std::make_unique<ConcreteType>(std::forward<Args>(args)...);
        };
      }
    }
    else
    {
      static_assert(false, "Invalid type");
    }
  }

  template <typename ConcreteType, typename... Args>
  static void register_type()
  {
    register_type<ConcreteType, Args...>(typeid(ConcreteType).hash_code());
  }

  //
  // try to make instance of base_type using the key and args
  // throws std::runtime_error if no valid registry is found
  //
  template <typename... Args>
  static base_type make(const key_type& key, Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    auto hash = g_hash_function(key);

    auto it = get_registry<base_type, Args...>().find(hash);

    if(it == get_registry<base_type, Args...>().end())
    {
      throw std::runtime_error("Registry not found");
    }

    return it->second(std::forward<Args>(args)...);
  };

  //
  // try to make instance of base_type using the resgistered ConcreteType
  // throws std::runtime_error if no valid registry is found
  //
  template <typename ConcreteType, typename... Args>
    requires(std::is_convertible_v<ConcreteType, base_type>)
  static base_type make(Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    auto hash = typeid(ConcreteType).hash_code();

    auto it = get_registry<base_type, Args...>().find(hash);

    if(it == get_registry<base_type, Args...>().end())
    {
      throw std::runtime_error("Registry not found");
    }

    return it->second(std::forward<Args>(args)...);
  }

  //
  // loop through all the registered keys and try to make instance of base_type using the provided args
  // throws std::runtime_error if no valid registry is found
  //
  template <typename... Args>
  static base_type try_make(Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    std::exception_ptr eptr;
    for(auto&& [_, func] : get_registry<base_type, Args...>())
    {
      try
      {
        return func(std::forward<Args>(args)...);
      }
      catch(...)
      {
        eptr = std::current_exception();
      }
    }

    if(eptr)
    {
      std::rethrow_exception(eptr);
    }
    else
    {
      if constexpr(std::is_default_constructible_v<base_type>)
      {
        return base_type();
      }
      else
      {
        throw std::runtime_error("no valid registery is found");
      }
    }
  }

  //
  // make raw ptr
  //

  //
  // try to make a raw pointer of base_type using the key and args
  // returns nullptr if no valid registry is found
  //
  template <typename... Args>
  static base_type* make_ptr(const key_type& key, Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    auto hash = g_hash_function(key);

    auto it = get_registry<base_type*, Args...>().find(hash);

    if(it == get_registry<base_type*, Args...>().end())
    {
      throw std::runtime_error("Registry not found");
    }

    return it->second(std::forward<Args>(args)...);
  }

  //
  // try to make a raw pointer using ConcreteType and cast it to ConcreteType*
  // returns nullptr if no valid registry is found
  //
  template <typename ConcreteType, typename... Args>
  static ConcreteType* make_ptr(Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    auto hash = typeid(ConcreteType).hash_code();

    auto it = get_registry<base_type*, Args...>().find(hash);

    if(it == get_registry<base_type*, Args...>().end())
    {
      throw std::runtime_error("Registry not found");
    }

    return static_cast<ConcreteType*>(it->second(std::forward<Args>(args)...));
  }

  //
  // loop through all the registered keys and try to make a raw pointer of base_type using the provided args
  // returns nullptr if no valid registry is found
  //
  template <typename... Args>
  static base_type* try_make_ptr(Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    std::exception_ptr eptr;
    for(auto&& [_, func] : get_registry<base_type*, Args...>())
    {
      try
      {
        auto obj = func(std::forward<Args>(args)...);
        if(obj)
        {
          return obj;
        }
      }
      catch(...)
      {
        eptr = std::current_exception();
      }
    }

    if(eptr)
    {
      std::rethrow_exception(eptr);
    }
    else
    {
      return nullptr;
    }
  }

  //
  // make shared_ptr
  //

  //
  // try to make a shared pointer of base_type using the key and args
  // returns nullptr if no valid registry is found
  //
  template <typename... Args>
  static std::shared_ptr<base_type> make_shared(const key_type& key, Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    auto hash = g_hash_function(key);

    auto it = get_registry<std::shared_ptr<base_type>, Args...>().find(hash);

    if(it == get_registry<std::shared_ptr<base_type>, Args...>().end())
    {
      throw std::runtime_error("Registry not found");
    }

    return it->second(std::forward<Args>(args)...);
  }

  //
  // try to make a shared pointer using ConcreteType and cast it to
  // std::shared_ptr<ConcreteType> returns nullptr if no valid registry is found
  //
  template <typename ConcreteType, typename... Args>
  static std::shared_ptr<ConcreteType> make_shared(Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    auto hash = typeid(ConcreteType).hash_code();

    auto it = get_registry<std::shared_ptr<base_type>, Args...>().find(hash);

    if(it == get_registry<std::shared_ptr<base_type>, Args...>().end())
    {
      throw std::runtime_error("Registry not found");
    }

    return std::static_pointer_cast<ConcreteType>(it->second(std::forward<Args>(args)...));
  }

  //
  // loop through all the registered keys and try to make a shared pointer of base_type using the provided args
  // returns nullptr if no valid registry is found
  //
  template <typename... Args>
  static std::shared_ptr<base_type> try_make_shared(Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    std::exception_ptr eptr;

    for(auto&& [_, func] : get_registry<std::shared_ptr<base_type>, Args...>())
    {
      try
      {
        auto obj = func(std::forward<Args>(args)...);
        if(obj)
        {
          return obj;
        }
      }
      catch(...)
      {
        eptr = std::current_exception();
      }
    }

    if(eptr)
    {
      std::rethrow_exception(eptr);
    }
    else
    {
      return nullptr;
    }
  }

  //
  // make unique_ptr
  //

  //
  // try to make a unique pointer of base_type using the key and args
  // returns nullptr if no valid registry is found
  //
  template <typename... Args>
  static std::unique_ptr<base_type> make_unique(const key_type& key, Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    auto hash = g_hash_function(key);

    auto it = get_registry<std::unique_ptr<base_type>, Args...>().find(hash);

    if(it == get_registry<std::unique_ptr<base_type>, Args...>().end())
    {
      throw std::runtime_error("Registry not found");
    }

    return it->second(std::forward<Args>(args)...);
  }

  //
  // try to make a unique pointer using ConcreteType and cast it to
  // std::unique_ptr<ConcreteType> returns nullptr if no valid registry is found
  //
  template <typename ConcreteType, typename... Args>
  static std::unique_ptr<ConcreteType> make_unique(Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    auto hash = typeid(ConcreteType).hash_code();

    auto it = get_registry<std::unique_ptr<base_type>, Args...>().find(hash);

    if(it == get_registry<std::unique_ptr<base_type>, Args...>().end())
    {
      throw std::runtime_error("Registry not found");
    }

    auto obj = it->second(std::forward<Args>(args)...);

    return std::unique_ptr<ConcreteType>(static_cast<ConcreteType*>(obj.release()));
  }

  //
  // loop through all the registered keys and try to make a unique pointer of base_type using the provided args
  // returns nullptr if no valid registry is found
  //
  template <typename... Args>
  static std::unique_ptr<base_type> try_make_unique(Args&&... args)
  {
    std::lock_guard lock(g_mutex);

    std::exception_ptr eptr;
    for(auto&& [_, func] : get_registry<std::unique_ptr<base_type>, Args...>())
    {
      try
      {
        auto obj = func(std::forward<Args>(args)...);
        if(obj)
        {
          return obj;
        }
      }
      catch(...)
      {
        eptr = std::current_exception();
      }
    }

    if(eptr)
    {
      std::rethrow_exception(eptr);
    }
    else
    {
      return nullptr;
    }
  }

  private:
  static_factory() = delete;

  template <typename ReturnType, typename... Args>
  static void register_function_impl(const key_type& key,
    std::function<ReturnType(Args...)>&& func)
  {
    static_assert(detail::is_related_v<base_type, ReturnType>, "Invalid function return type");

    std::lock_guard lock(g_mutex);

    auto hash = g_hash_function(key);

    if constexpr(std::is_convertible_v<ReturnType, base_type>)
    {
      get_registry<base_type, Args...>()[hash] = std::move(func);
    }
    else if constexpr(std::is_convertible_v<ReturnType, base_type*>)
    {
      get_registry<base_type*, Args...>()[hash] = std::move(func);
    }
    else if constexpr(detail::is_specialization_of_v<ReturnType, std::shared_ptr> &&
      std::is_base_of_v<base_type, typename detail::get_template_arg_type_of<ReturnType>::template arg<0>::type>)
    {
      get_registry<std::shared_ptr<base_type>, Args...>()[hash] = std::move(func);
    }
    else if constexpr(detail::is_specialization_of_v<ReturnType, std::unique_ptr> &&
      std::is_base_of_v<base_type, typename detail::get_template_arg_type_of<ReturnType>::template arg<0>::type>)
    {
      get_registry<std::unique_ptr<base_type>, Args...>()[hash] = std::move(func);
    }
    else
    {
      static_assert(false, "Invalid function return type");
    }
  }

  template <typename RetType, typename... Args>
  static auto& get_registry()
  {
    return g_registry<RetType, std::decay_t<Args>...>;
  }

  static constexpr auto g_hash_function = std::hash<key_type>();
  template <typename RetType, typename... Args>
  static detail::unordered_flat_map<size_t, typename std::function<RetType(Args...)>> g_registry;

  static std::mutex g_mutex;
};

template <typename base_type, typename key_type>
template <typename RetType, typename... Args>
detail::unordered_flat_map<size_t, typename std::function<RetType(Args...)>>
  static_factory<base_type, key_type>::g_registry; // TODO: replace with std::flat_map

template <typename base_type, typename key_type>
std::mutex static_factory<base_type, key_type>::g_mutex;

#endif // STATIC_FACTORY_H