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

### Query parameter

```cpp
#include "chttpp.hpp"

int main() {
  // Embedding in URL
  chttpp::get("https://example.com/?param1=value1&param2=value2");

  // Or specify in config
  chttpp::get("https://example.com/", { .params = {
                                          {"param1", "value1"},
                                          {"param2", "value2"}
                                        }
                                      });
}
```

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

`http_headers.hpp` has a predefined header name object. This can be used to avoid header name errors.

```cpp
#include "chttpp.hpp"
#include "http_headers.hpp" // opt-in

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
#include "http_headers.hpp"

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
                              .password = "password",
                              .scheme = chttpp::cfg::authentication_scheme::basic
                            },
                            // Proxy settings
                            .proxy = {
                              .address = "address:port",
                              .scheme = chttpp::cfg::proxy_scheme::http
                              .auth = {
                                .username = "proxy username",
                                .password = "proxy password",
                                .scheme = chttpp::cfg::authentication_scheme::basic
                              }
                            }
                          })
```

All configs can be omitted, but not reordered.

#### namespace for configs

The namespace regarding configurations is as follows

```cpp
namespace chttpp {

  // Namespace for accessing enums representing configuration values
  namespace cfg {

    enum class http_version {
      http1_1,
      http2,
    };

    enum class authentication_scheme {
      none,
      basic,
    };

    enum class proxy_scheme {
      http,
      https,
      socks4,
      socks4a,
      socks5,
      socks5h,
    };
  }

  // Namespace for each configuration item (short names allow direct access to enumeration values)

  // Authentication configs
  namespace cfg_auth {
    using enum chttpp::cfg::authentication_scheme;
  }

  // http version configs
  namespace cfg_ver {
    using enum chttpp::cfg::http_version;
  }

  // proxy schme configs
  namespace cfg_prxy {
    using enum chttpp::cfg::proxy_scheme;
  }
}
```

For example

```cpp
chttpp::post("url", data, { .auth = {
                              .username = "username",
                              .password = "password",
                              .scheme = chttpp::cfg_auth::basic
                            } });

chttpp::post("url", data, { .version = chttpp::cfg_ver::http2 });
chttpp::post("url", data, { .version = chttpp::cfg_ver::http1_1 });

chttpp::post("url", data, { .proxy = {
                              .address = "address:port",
                              .scheme = chttpp::cfg_prxy::http
                              .auth = {
                                .username = "proxy username",
                                .password = "proxy password",
                                .scheme = chttpp::cfg_auth::basic
                              }
                            } });

chttpp::post("url", data, { .proxy = {
                              .address = "address:port",
                              .scheme = chttpp::cfg_prxy::socks5
                              .auth = {
                                .scheme = chttpp::cfg_auth::none
                              }
                            } });
```

#### Numeric specification of the http version

The http version can also be specified numerically. At this time, an incorrect value will result in a compilation error.

```cpp
// valid example
chttpp::post("url", data, { .version = 1.1 });
chttpp::post("url", data, { .version = 2 });
chttpp::post("url", data, { .version = 2.0 });

// Invalid example (compile error)
chttpp::post("url", data, { .version = 1 });
chttpp::post("url", data, { .version = 11 });
chttpp::post("url", data, { .version = 0 });
chttpp::post("url", data, { .version = 1.0 });
```

### Consumption of request results

The result of the request is returned as an object of type `http_result`. This is a monadic type object that holds either success or failure (and exception) status.

You can use `operator bool()` (or `.has_value()`) to determine success or failure and `.value()` to get the response.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"

int main() {
  using namespace chttpp::mime_types;

  auto res = chttpp::post("https://example.com", "request body", { .content_type = text/plain });

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
#include "http_headers.hpp"

int main() {
  using namespace chttpp::mime_types;
  using namespace chttpp::headers;

  auto res = chttpp::post("https://example.com", "request body", { .content_type = text/plain });

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
|`.status_code()`|`chttpp::detail::http_status_code`|http status code of response|`0`|
|`.response_body()`|`std::string_view`|response body string|empty string|
|`.response_data()`|`std::span<char>`|response body bytes|empty span|
|`.response_header()`|`std::stirng_view`|value of specified header name|empty string|
|`.response_headers()`|`chttpp::detail::header_ref`|response header range|empty range|

`.response_body()`/`.response_data()` have overloads, and element types can be specified by template parameters.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"
#include "http_headers.hpp"

int main() {
  using namespace chttpp::mime_types;
  using namespace chttpp::headers;

  auto res = chttpp::post("https://example.com", "request body", { .content_type = text/plain });

  // Refer to the response body as a UTF-8 string.
  std::u8string_view u8str = res.response_body<char8_t>();

  // Refer to response data as a byte array by std::byte.
  std::span<std::byte> bytes = res.response_data<std::byte>();
}
```

`.response_body<CharT>()` allows `CharT` to specify built-in character types .

In `.response_data<D>()`, `D` can be specified as a scalar or trivially copyable aggregate type.

There is also a function that outputs the status of `http_result`.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"
#include "http_headers.hpp"

int main() {
  using namespace chttpp::mime_types;
  using namespace chttpp::headers;

  auto res = chttpp::post("https://example.com", "request body", { .content_type = text/plain });

  // Outputs a uniform status message regardless of success or failure.
  std::cout << res.status_message() << '\n';
  // On success : "HTTP/1.1 200 OK", etc
  // On failure : underlying library error messages
  // On exception : After "Exception : ", print it if the exception is a string, or print .what() if the exception is a std::exception.
  //                Otherwise print "Unstringable exception".
}
```

#### Pipe continuation processing

You can chain any process by pipe operators (`operator|`) with the result as `string_view`.

```cpp
#include "chttpp.hpp"
#include "mime_types.hpp"
#include "http_headers.hpp"

// Convert to json type by an arbitrary library
auto to_json(std::string_view response) -> json_type {
  if (not response.empty()) {
    // On success
    ...
  } else {
    // On failure
  }
}

int main() {
  using namespace chttpp::mime_types;
  using namespace chttpp::headers;

  auto res = chttpp::post("https://example.com", "request body", { .content_type = text/plain });

  // For example, parse the response as json
  auto json = res | to_json;

  // You can also chain a range adapter
  for (auto e : res | std::views::split(...)
                    | std::views::transform(...)
                    | std::views::fillter(...))
  {
    ...
  }
}
```

The right-hand side of `|` must be callable by `std::string_view`, but its return type is arbitrary.

If `http_result` object is not in a successful state, an empty `std::string_view` is passed.

#### Monadic operation

Three monadic interfaces are available for simple response handling.

```cpp
#include "chttpp.hpp"
#include "http_headers.hpp"
#include "mime_types.hpp"

int main() {
  using namespace chttpp::mime_types;
  using namespace chttpp::headers;

  chttpp::post("https://example.com", "request body", { .content_type = text/plain })
    .then([](chttpp::http_response&& response) {
      // On success

      auto&& [body, headers, status] = std::move(response);
      // body : std::vector<char>
      // headers : std::unordered_map<std::string, std::string>
      // status : chttpp::http_status_code

      std::cout << headers[http_status] << '\n';  // For example, "HTTP/1.1 200 OK" etc.

      std::string_view str{body};
      std::cout << str << '\n';   // Output of response body

      if (status.OK()) {
        // Processing at 200 responses
        ...
      }
    })
    .catch_error([](chttpp::error_code error_code) {
      // On failure

      std::cout << error_code.value() << " : ";
      std::cout << error_code.message() << '\n';
    })
    .catch_exception([](chttpp::exptr_wrapper&& exptr) {
      // On exception

      std::cout << exptr << '\n';
    });
}
```

In this case, only one of these three functions will be called depending on the status of `http_result`.

##### `then()`

`then()` is a function that specifies callback processing to be called if the request is successful.

Since *rvalue* of `chttpp::http_response` is passed to the callback function, it must be an argument type that can accept this value.

```cpp
// Examples of valid callback argument types
chttpp::post(...)
  .then([](chttpp::http_response&& response) {...});

chttpp::post(...)
  .then([](chttpp::http_response response) {...});

chttpp::post(...)
  .then([](auto&& response) {...});

chttpp::post(...)
  .then([](const auto& response) {...});
```

A callback function can return any return value and does not have to return. 

If the return type of the callback is `void`, the callback function is passed `const chttpp::http_response&`.

```cpp
chttpp::post(...)
  .then([](auto&& response) {
    ...

    return /*anything*/;
  });

chttpp::post(...)
  .then([](auto&& response) {
    // response : const chttpp::http_response&

    ...
    // Nothing is returned
  });
```

Note that the return type of the callback is `std::remove_cvref`, so if a reference is returned, it is copied.

Therefore, if you receive an argument as `const &`, you cannot return its `http_response` object (because the `http_response` object is not copyable).

```cpp
chttpp::post(...)
  .then([](auto&& response) {
    ...

    return response;  // ok (implicit move)
  });

chttpp::post(...)
  .then([](const auto& response) {
    ...

    return response;  // ng (attempting to copy)
  });
```

The value returned by the callback can be used to chain `then()`.

```cpp
chttpp::post(...)
  .then([](auto&& response) {
    std::cout << response.status_code.value() << '\n';

    return std::make_pair(std::move(response.body), std::move(response.headers));
  })
  .then([](auto&& pair) {
    for (const auto [name, value] : pair) {
      std::cout << name << " : " << value << '\n';
    }

    return std::move(pair.first);
  })
  .then([](auto&& body) {
    std::cout << std::string_view{body} << '\n';

    return "complete!"sv;
  })
  .then([](auto str) {
    std::cout << str << '\n';
  });
```

Subsequent `then()`s can be treated in the same way as the first `then()` (which passes `http_response`) (same constraints, etc.).

If the return type is `void` and the callback does not take ownership of the previous value (i.e., if it receives an argument with `const&`), the value is unchanged and the same object is passed to the subsequent `then()`.

```cpp
chttpp::post(...)
  .then([](const auto& response) {
    std::cout << response.status_code.value() << '\n';
  })
  .then([](auto&& response) {
    for (const auto [name, value] : response.headers) {
      std::cout << name << " : " << value << '\n';
    }
    // Since nothing is returned, response is const http_response&.
  })
  .then([](const chttpp::http_response& response) {
    std::cout << std::string_view{response.body} << '\n';

    return "complete!"sv;
  })
  .then([](auto str) {
    std::cout << str << '\n';
  });
```

Everything you receive as `response` in this example points to the same object (not even moved).

##### `catch_error()`

`catch_error()` is a function that specifies the callback process to be called if the request fails.

The callback is passed the *rvalue* of `chttpp::error_code`, so the callback must be able to accept this value.

```cpp
// Examples of valid callback argument types
chttpp::post(...)
  .catch_error([](chttpp::error_code&& ec) {...});

chttpp::post(...)
  .catch_error([](chttpp::error_code ec) {...});

chttpp::post(...)
  .catch_error([](auto&& ec) {...});

chttpp::post(...)
  .catch_error([](auto ec) {...});

chttpp::post(...)
  .catch_error([](const auto& ec) {...});
```

A callback function can return any return value and does not have to return.

If the callback function returns nothing (return type `void`), subsequent calls to `catch_error()` will not be allowed.

If any value is returned, it is passed (with ownership) to the next `catch_error()` callback.

```cpp
chttpp::post(...)
  .catch_error([](auto ec) {
    return ec.message();
  })
  .catch_error([](auto&& str) {
    std::cout << str;
  });

chttpp::post(...)
  .catch_error([](auto ec) {
    // Nothing is returned
  })
  .catch_error([](auto&&) {});  // error!
```

Note that the return type of the callback is `std::remove_cvref`, so if a reference is returned, it is copied.


`chttpp::error_code` is a type that holds the error code of the underlying library with type erasure.

This error condition is not an HTTP error (i.e., a response in the 400s), but rather an error in the communication itself or in the OS/library.

`chttpp::error_code` provides a simple interface for retrieving error information.

```cpp
chttpp::post(...)
  .catch_error([](chttpp::error_code ec) {
    // Get error message
    std::pmr::string err_msg = ec.message();

    // Get raw error code (type is the type of the underlying library error code)
    auto err_val = ec.value();

    // Get error codition (true on error)
    bool is_err = bool(ec);
  })
```

##### `catch_exception()`

##### `match()`

### Another http client - agent

## API
