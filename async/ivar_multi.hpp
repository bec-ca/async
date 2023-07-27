#pragma once

#include <type_traits>

#include "async.hpp"

#include "bee/copy.hpp"
#include "bee/unit.hpp"

namespace async {

template <deferable_value T = void> struct IvarMulti {
 public:
  using ptr = std::shared_ptr<IvarMulti>;

  using value_type = T;
  using lvalue_type = std::add_lvalue_reference_t<value_type>;

  using const_value_type = const value_type;
  using const_lvalue_type = std::add_lvalue_reference_t<const_value_type>;

  IvarMulti() {}

  IvarMulti(const IvarMulti& other) = delete;
  IvarMulti(IvarMulti&& other) = default;

  static ptr create() { return ptr(new IvarMulti()); }

  const_lvalue_type value() const
  {
    if constexpr (!std::is_void_v<T>) { return *_value; }
  }

  Deferred<value_type> deferred_value()
  {
    if (_value.has_value()) {
      if constexpr (std::is_void_v<T>) {
        return {};
      } else {
        return *_value;
      }
    }
    auto ivar = Ivar<value_type>::create();
    _ivars.push_back(ivar);
    return ivar_value(ivar);
  }

  template <class... Args> void fill(Args&&... args)
  {
    _value.emplace(std::forward<Args>(args)...);
    _fan_out();
  }

  bool is_determined() const { return _value.has_value(); }

 private:
  void _fan_out()
  {
    for (auto& ivar : _ivars) {
      if constexpr (std::is_void_v<T>) {
        ivar->fill();
      } else {
        ivar->fill(bee::copy(*_value));
      }
    }
    _ivars.clear();
  }

  std::vector<typename Ivar<value_type>::ptr> _ivars;
  std::optional<bee::unit_if_void_t<T>> _value;
};

} // namespace async
