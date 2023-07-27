
#include "host_and_port.hpp"

#include "bee/string_util.hpp"

using std::string;

namespace async {

HostAndPort::HostAndPort(string&& host, int port)
    : _host(std::move(host)), _port(port)
{}

HostAndPort::HostAndPort(const string& host, int port)
    : HostAndPort(string(host), port)
{}

const std::string& HostAndPort::host() const { return _host; }
int HostAndPort::port() const { return _port; }

bee::OrError<HostAndPort> HostAndPort::of_string(const string& str)
{
  auto parts = bee::split(str, ":");
  if (parts.size() != 2) {
    return bee::Error::fmt("Invalid host and port: $", str);
  }
  return HostAndPort(parts[0], std::stoi(parts[1]));
}

string HostAndPort::to_string() const { return F("$:$", _host, _port); }

const char* HostAndPort::type_name() { return "async::HostAndPort"; }

} // namespace async
