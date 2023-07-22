# chttpp - Client for http for C++

A C++20 header-only cross platform HTTP client library.

The underlying libraries are winhttp (Windows) or lbcurl (Linux, Mac).

Non-Windows platforms require libcurl installation.

## Getting Started

1. Clone this repository or copy `include` directory (recursively).
2. Add `include` directory to your project's include path.
3. Add `#include <chttpp.hpp>` or `import <chttpp.hpp>` to the source code you wish to use.
4. In addition, Non-Windows platforms require libcurl installation.

### Compliler

- GCC 12.1 or later
    - `-std=c++2a`
- MSVC 2022 or later
    - `/std:c++latest`
- Clang ??

## Example

### GET request

```cpp
#include "chttpp.hpp"

int main() {
  auto res = chttpp::get("https://example.com");

  std::cout << res.status_code()   << '\n';
  std::cout << res.response_body() << '\n';
}
```

### POST request

```cpp
#include "chttpp.hpp"

int main() {
  auto res = chttpp::post("https://example.com", "post data", { .content_type = "text/plain" });

  std::cout << res.status_code()   << '\n';
  std::cout << res.response_body() << '\n';
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

  std::cout << res.status_code()   << '\n';
  std::cout << res.response_body() << '\n';
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

- HEAD : `chttpp::head(url, config)`
- OPTIONS : `chttpp::options(url, config)`
- TRACE : `chttpp::trace(url, config)`
- PUT : `chttpp::put(url, body, config)`
- DELETE : `chttpp::delete_(url, body, config)`

In these cases, the request can still be made through a consistent interface.

### Request body

The request body can be any value that can be serialized into a byte array.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"

int main() {
  int array[] = {1, 2, 3, 4};
  chttpp::post("https://example.com", array, { .content_type = application/octet_stream }); // ok

  std::vector vec = {1, 2, 3, 4};
  chttpp::post("https://example.com", vec, { .content_type = application/octet_stream });   // ok

  std::span<const int> sp = vec;
  chttpp::post("https://example.com", sp, { .content_type = application/octet_stream });    // ok

  std::string json = R"({ id : 1, json : "test json"})";
  chttpp::post("https://example.com", json, { .content_type = application/json });          // ok

  std::string_view strview = json;
  chttpp::post("https://example.com", strview, { .content_type = application/json });       // ok

  double v = 3.14;
  chttpp::post("https://example.com", v, { .content_type = application/octet_stream });     // ok
}
```

This is implicitly converted by `chttpp::as_byte_seq` CPO.

This CPO takes values of types `contiguous_range`, *Trivially Copyable*, etc. as a byte array.

Besides, customization points by user-defined functions of the same name are available.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"

class Test {
  std::vector<char> m_bytes;

public:

  ...

  // adopt as_byte_seq CPO by Hidden friends (free function is also acceptable)
  friend auto as_byte_seq(const Test& self) -> std::span<const char> {
    return self.m_bytes;
  }

  // or non static member function (one or the other!)
  auto as_byte_seq() const -> std::span<const char> {
    return m_bytes;
  }
};

int main() {
  Test dat = ...;
  chttpp::post("https://example.com", dat, { .content_type = application/octet_stream }); // ok
}
```

#### Automatic setting of `Content-Type`

`content_type` can be omitted, in which case it is automatically obtained from the request body type.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"

int main() {
  std::vector vec = {1, 2, 3, 4};
  chttpp::post("https://example.com", vec);   // content_type = application/octet_stream

  std::span<const int> sp = vec;
  chttpp::post("https://example.com", sp);    // content_type = application/octet_stream

  std::string json = R"({ id : 1, json : "test json"})";
  chttpp::post("https://example.com", str);   // content_type = text/plain
}
```

Default mimetype may be of little use.

This is done by the `chttpp::query_content_type` variable template, which can also be customized to specify the appropriate mimetype.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"

class Test {
  std::vector<char> m_bytes;

public:

  ...

  friend auto as_byte_seq(const Test& self) -> std::span<const char> {
    return self.m_bytes;
  }

  // Static member variable `ContentType`
  static constexpr std::string_view ContentType = video/mp4;
};

// or partial specilization (one or the other!)
template<>
inline cosntexpr std::string_view chttpp::query_content_type<Test> = video/mp4;

int main() {
  Test dat = ...;
  chttpp::post("https://example.com", dat); // content_type = video/mp4
}
```

### Adding Headers

Headers and other configurations are specified in the last argument.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"

int main() {
  auto res = chttpp::post("https://example.com", 
                          "post data", 
                          { .content_type = text/plain,
                            .headers = {
                              {"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"}
                            }
                          });

  std::cout << res.status_code()   << '\n';
  std::cout << res.response_body() << '\n';
}
```

Multiple headers can be added at once.

```cpp
#include "chttpp.hpp"

int main() {
  auto res = chttpp::post("https://example.com", 
                          "post data",
                          { .headers = {
                              {"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"},
                              {"Content-Type", "text/plain"},
                              {"Content-Language", "ja-JP"}
                            }
                          });
}
```

This uses POST(`chttpp::post`), but the same is true for other methods.

#### Predefined header name object

`http_deaders.hpp` has a predefined header name object. This can be used to avoid header name errors.

```cpp
#include "chttpp.hpp"
#include "http_deaders.hpp" // opt-in

int main() {
  using namespace chttpp::headers;   // using

  auto res = chttpp::post("https://example.com", 
                          "post data",
                          // When setting request headers
                          { .headers = {
                              { user_agent, "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"},
                              { content_type, "text/plain"},
                              { content_language, "ja-JP"}
                            }
                          });
  
  auto res_header = res.response_header();

  // When obtaining response headers
  std::cout << res_header[content_length] << "\n";
  std::cout << res_header[set_cookie] << "\n";
}
```

This makes the header name specified lower case (HTTP/2 and HTTP/3 only allows lowercase header names).

This object name is defined by lowercasing the original header name and replacing the `-` with `_` (not all header names are defined).

In addition, when setting request headers, values can also be specified by `=`.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"
#include "http_deaders.hpp"

int main() {
  using namespace chttpp::mime_types;
  using namespace chttpp::headers;

  auto res = chttpp::post("https://example.com", 
                          "post data",
                          { .headers = {
                              user_agent = "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)",
                              content_type = text/plain,
                              content_language = "ja-JP"
                            }
                          });
}
```

### List of Configurations

```cpp
chttpp::post("url", data, {
                            // Content-Type header value (Only requests with body)
                            .content_type = ..., 
                            // Request headers
                            .headers = { {"header name", "value"}, {..., ...}, ... },
                            // URL parameter
                            .params = { {"param name", "param value"}, {..., ...}, ... },
                            // HTTP ver (HTTP1.1 or HTTP/2)
                            .version = chttpp::cfg::http_version::http2,
                            // timeout in milliseconds (Use udl in chrono)
                            .timeout = 1000ms,
                            // HTTP Authentication settings (Basic authentication)
                            .auth = {
                              .username = "username",
                              .password = "password"
                            },
                            // Proxy settings
                            .proxy = {
                              .address = "address:port",
                              .scheme = chttpp::cfg::proxy_scheme::http
                              .auth = {
                                .username = "proxy username",
                                .password = "proxy password"
                              }
                            }
                          })
```

All configs can be omitted, but not reordered.

### Consumption of request results

The result of the request is returned as an object of type `http_result`. This is a monadic type object that holds either success or failure (and exception) status.

You can use `operator bool()` to determine success or failure and `.value()` to get the response.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"

int main() {
  using namespace chttpp::mime_types;

  auto res = chttpp::post("https://example.com", "field1=value1&field2=value2", { .content_type = text/plain });

  if (res) {
    // success
    auto& response = res.value();
    ...
  } else {
    // failure (Errors in the underlying library)
    auto err = res.error();
    ...
  }
}
```

In this context, failure refers to the failure of the HTTP access itself, and it does not indicate a response with an HTTP status code in the 400 series.

This interface follows `std::optional` and `std::expected` and is not very user-friendly. Therefore, an interface is provided that can attempt to retrieve the response regardless of the state of the result object.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"
#include "http_deaders.hpp"

int main() {
  using namespace chttpp::mime_types;
  using namespace chttpp::headers;

  auto res = chttpp::post("https://example.com", "field1=value1&field2=value2", { .content_type = text/plain });

  // Get HTTP status code
  std::cout << res.status_code().value() << '\n';

  // Get the response body as a string
  std::cout << res.response_body() << '\n';

  // Get the response body as a byte array
  std::span<char> data = res.response_data();

  // Get the response header value
  std::cout << res.response_header(set_cookie) << '\n';       // use predefined header name object
  std::cout << res.response_header("content-length") << '\n'; // use header name string

  // Iterate all response headers
  for (auto [name, value] : res.response_headers()) {
    std::cout << name << " : " << value << '\n';
  }
}
```

These functions return an empty string or error value (`.status_code()`) or an empty object (`.response_data()`/`.response_headers()`) if the `http_result` object is in failure state.

|function|return type|on success|on failure|
|---|---|---|---|
|`.status_code()`|`chttpp::detail::http_status_code`|http status code of response|`std::uint16_t(-1)`|
|`.response_body()`|`std::string_view`|response body string|empty string|
|`.response_data()`|`std::span<char>`|response body bytes|empty span|
|`.response_header()`|`std::stirng_view`|value of specified header name|empty string|
|`.response_headers()`|`chttpp::detail::header_ref`|response header range|empty range|

`.response_body()`/`.response_data()` have overloads, and element types can be specified by template parameters.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"
#include "http_deaders.hpp"

int main() {
  using namespace chttpp::mime_types;
  using namespace chttpp::headers;

  auto res = chttpp::post("https://example.com", "field1=value1&field2=value2", { .content_type = text/plain });

  // Refer to the response body as a UTF-8 string.
  std::u8string_view u8str = res.response_body<char8_t>();

  // Refer to response data as a byte array by std::byte.
  std::span<std::byte> bytes = res.response_data<std::byte>();
}
```

`.response_body<CharT>()` allows `CharT` to specify built-in character types .

In `.response_data<D>()`, `D` can be specified as a scalar or trivially copyable aggregate type.

Some functions output (error) messages anyway, regardless of the state of `http_result`.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"
#include "http_deaders.hpp"

int main() {
  using namespace chttpp::mime_types;
  using namespace chttpp::headers;

  auto res = chttpp::post("https://example.com", "field1=value1&field2=value2", { .content_type = text/plain });

  // Outputs a uniform error message regardless of success or failure.
  std::cout << res.error_message() << '\n';
  // On success : "HTTP1.1 200", etc
  // On failure : underlying library error messages
  // On exception : If the exception is a string, output it, if std::exception, output .what()
}
```

#### Monadic operation

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
                  for (const auto& [name, value] : headers) {
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

  auto res = chttpp::post("https://example.com", "field1=value1&field2=value2", { .content_type = text/plain })
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

### Another http clietn - agent

## API
