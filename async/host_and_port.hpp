#pragma once

#include "bee/error.hpp"
#include "yasf/of_stringable_mixin.hpp"

#include <string>

namespace async {

struct HostAndPort : public yasf::OfStringableMixin<HostAndPort> {
 public:
  HostAndPort(std::string&& host, int port);
  HostAndPort(const std::string& host, int port);

  static bee::OrError<HostAndPort> of_string(const std::string& str);
  std::string to_string() const;

  const std::string& host() const;
  int port() const;

  static const char* type_name();

 private:
  std::string _host;
  int _port;
};

} // namespace async
