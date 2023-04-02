#pragma once

#include "async.hpp"

#include "bee/copy.hpp"

namespace async {

template <class T> struct IvarMulti {
 public:
  using ptr = std::shared_ptr<IvarMulti>;

  IvarMulti() {}

  IvarMulti(const IvarMulti& other) = delete;
  IvarMulti(IvarMulti&& other) = default;

  static ptr create() { return ptr(new IvarMulti()); }

  bool has_value() const { return _value.has_value(); }

  const std::optional<T>& value_opt() const { return _value; }
  const T& value() const { return *_value; }

  Deferred<T> deferred_value()
  {
    if (_value.has_value()) { return *_value; }
    auto ivar = Ivar<T>::create();
    _ivars.push_back(ivar);
    return ivar_value(ivar);
  }

  void resolve(T&& value)
  {
    _value.emplace(std::move(value));
    _fan_out();
  }

  void resolve(const T& value)
  {
    _value.emplace(value);
    _fan_out();
  }

  bool is_resolved() const { return _value.has_value(); }

 private:
  void _fan_out()
  {
    for (auto& ivar : _ivars) { ivar->resolve(bee::copy(*_value)); }
    _ivars.clear();
  }

  std::vector<typename Ivar<T>::ptr> _ivars;
  std::optional<T> _value;
};

} // namespace async
