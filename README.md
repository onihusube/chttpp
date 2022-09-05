# chttpp - Client for http for C++

A C++20 header-only cross platform HTTP client library.

The underlying libraries are winhttp (Windows) or lbcurl (Linux, Mac).

Non-Windows platforms require libcurl installation.

## Example

### GET request

```cpp
#include "chttpp.hpp"

int main() {
  auto res = chttpp::get("https://example.com");

  if (res) {
    std::cout << res.status_code()   << '\n';
    std::cout << res.response_body() << '\n';
  }
}
```

### POST request

```cpp
#include "chttpp.hpp"

int main() {
  auto res = chttpp::post("https://example.com", "post data", { .content_type = "text/plain" });

  if (res) {
    std::cout << res.status_code()   << '\n';
    std::cout << res.response_body() << '\n';
  }
}
```

#### Predefined mime type object

For common mime type specifications, another method is available.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"  // opt in

int main() {
  using namespace chttpp::mime_types;   // using

  auto res = chttpp::post("https://example.com", "post data",  { .content_type = text/plain }); // Specification by Objects and Operators

  if (res) {
    std::cout << res.status_code()   << '\n';
    std::cout << res.response_body() << '\n';
  }
}
```

This advantage is

- No need to worry about the correct mime type string
- Correctness of mime type specification is verified at compile time

```cpp
int main() {
  using namespace chttpp::mime_types;

  auto res = chttpp::post("https://example.com", "post data",  { .content_type = text/mp4 }); // Compile error!
}
```

※ Some versions of MSVC 2019 may not be available due to a bug around `consteval`.

### Other Requests

- HEAD : `chttpp::head(url)`
- OPTIONS : `chttpp::options(url)`
- TRACE : `chttpp::trace(url)`
- PUT : `chttpp::put(url, body, mime_type)`
- DELETE : `chttpp::delete_(url, body, mime_type)`

In these cases, the request can still be made through a consistent interface.

### Adding Headers

Headers and other configurations are specified in the last argument.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"

int main() {
  auto res = chttpp::post("https://example.com", "post data", { .content_type = text/plain 
                                                                .headers = {
                                                                  {"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"}
                                                                }
                                                              });

  if (res) {
    std::cout << res.status_code()   << '\n';
    std::cout << res.response_body() << '\n';
  }
}
```

Multiple headers can be added at once.

```cpp
#include "chttpp.hpp"

int main() {
  auto res = chttpp::post("https://example.com", "post data", { .headers = {
                                                                  {"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"},
                                                                  {"Content-Type", "text/plain"},
                                                                  {"Content-Language", "ja-JP"}
                                                                }
                                                              });
}
```

This uses POST(`chttpp::post`), but the same is true for other methods.

### List of Configurations

※Some are still under development.

```cpp
chttpp::post("url", data, {
                            // Content-Type header value (Only requests with body)
                            .content_type = ..., 
                            // Request headers
                            .headers = { {"header name", "value"}, {..., ...}, ... }, 
                            // URL parameter
                            .params = { {"param name", "param value"}, {..., ...}, ... },
                            // timeout in milliseconds (Use udl in chrono)
                            .timeout = 1000ms,
                            // HTTP Authentication settings (Basic authentication)
                            .auth = {
                              .username = "username",
                              .password = "password"
                            },
                            // Proxy settings
                            .proxy = {
                              .url = "proxy address",
                              .auth = {
                                .username = "proxy username",
                                .password = "proxy password"
                              }
                            }
                          })
```

### Consumption of request results

The type of the request result is `http_result<E>`, a monadic type (`E` is the error code type of the underlying library).

`.then()` is available for continuation processing after a successful request.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"

int main() {
  using namespace chttpp::mime_types;

  auto res = chttpp::post("https://example.com", "field1=value1&field2=value2", { .content_type = text/mp4 })
              .then([](auto&& http_res) { 
                auto&& [body, headers, status] = std::move(http_res);
                // status is std::uint16_t
                if (status == 200) {

                  // body is std::vector<char>
                  std::cout << std::string_view(body.data(), body.size()) << '\n';

                  // headers is std::unordered_map<std::string, std::string>
                  for (auto [name, value] : headers) {
                    std::cout << name << " : " << value << '\n';
                  }

                  return body;
                }
              })
              .then([](auto&& body) {
                // It can be further continued while changing the type.
              });
}
```

Errors in the underlying library can be handled by `.catch_error()`.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"

int main() {
  using namespace chttpp::mime_types;

  auto res = chttpp::post("https://example.com", "field1=value1&field2=value2", { .content_type = text/mp4 })
              .then([](auto&& http_res) { 
                // Not called on errors in the underlying library.
              })
              .catch_error([](auto ec) {
                // ec is error code (integer type)
                std::cout << ec << '\n';
                // 
              });
}
```


## API
