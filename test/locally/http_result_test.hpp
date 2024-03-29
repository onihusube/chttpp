#pragma once

#include <string_view>
#include <concepts>
#include <ranges>

#include "chttpp.hpp"

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

static_assert(std::ranges::forward_range<chttpp::detail::header_ref>);
static_assert(std::ranges::sized_range<chttpp::detail::header_ref>);

auto hr_ok() -> chttpp::http_result {
  return chttpp::http_result{chttpp::detail::http_response{ {}, {}, { {"http-status-line", "HTTP/1.1 200 OK"}, {"host", "http_result test"} }, chttpp::detail::http_status_code{200} }};
}

auto hr_err() -> chttpp::http_result {
  return chttpp::http_result{::chttpp::underlying::lib_error_code_tratis::no_error_value};
}

decltype(auto) tb_exptr() {
  return hr_ok().then([](auto&&) {
    throw std::runtime_error{"test throw"};
    return 10;
  });
}

auto hr_exptr() -> chttpp::http_result {
  try {
    throw std::runtime_error{"test throw"};
  } catch (...) {
    return chttpp::http_result{chttpp::detail::from_exception_ptr};
  }
}

auto hr_ok_range() -> chttpp::http_result {
  std::pmr::vector<char> bytes = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  return chttpp::http_result{chttpp::detail::http_response{{}, std::move(bytes), {{"http-status-line", "HTTP/1.1 200 OK"}}, chttpp::detail::http_status_code{200}}};
}

void http_result_test() {
  using namespace boost::ut::literals;
  using namespace boost::ut::operators::terse;
  using namespace std::string_view_literals;
  using chttpp::detail::error_code;
  using chttpp::detail::exception_handler;
  using chttpp::detail::http_response;
  using chttpp::detail::exptr_wrapper;

  static_assert(not std::copyable<http_response>);
  static_assert(std::movable<http_response>);

  "then"_test = []
  {
    hr_ok().then([](http_response&& hr) {
        ut::expect(hr.body.empty());
        return hr;
      }).then([](http_response&& hr) {
        ut::expect(hr.headers.size() == 2_ull);
        return hr;
      }).then([](http_response&& hr) {
        ut::expect(hr.status_code.OK());
        return hr;
      }).catch_error([](const auto&) {
        ut::expect(false);
      }).catch_exception([](const auto&) {
        ut::expect(false);
      });
  
      // どっちの引数宣言も通ることを確認
      hr_ok().then([](auto&& hr) {
        ut::expect(true);
        return hr;
      }).then([](http_response&& hr) {
        ut::expect(true);
        return hr;
      });

      hr_ok().then([](const auto&) {
        ut::expect(true);
      }).then([]([[maybe_unused]] auto&& hr) {
        // ここのhrはconst http_response&
        ut::expect(true);
      });

      hr_ok().then([](const auto&) {
        ut::expect(true);
        return "complete"sv;
      }).then([](auto str) {
        ut::expect(str == "complete"sv);
      });
  };

  "then void"_test = []
  {
    hr_ok().then([](const http_response& hr) {
        ut::expect(hr.body.empty());
      }).then([](http_response&& hr) {
        ut::expect(hr.headers.size() == 2_ull);
        return hr;
      }).then([](const http_response& hr) {
        ut::expect(hr.status_code.OK());
      }).catch_error([](const auto&) {
        ut::expect(false);
      }).catch_exception([](const auto&) {
        ut::expect(false);
      });

      // どっちの引数宣言も通ることを確認
      hr_ok().then([](auto&&) {
        ut::expect(true);
      }).then([](http_response&&) {
        ut::expect(true);
      });
  };

  "catch_exception"_test = []
  {
    int count = 0;

    tb_exptr().then([](int&& hr) {
        ut::expect(false);
        return hr;
      }).then([](int&& hr) {
        ut::expect(false);
        return hr;
      }).then([](int&& hr) {
        ut::expect(false);
        return hr;
      }).catch_error([](const auto&) {
        ut::expect(false);
      }).catch_exception([&](const auto& exptr) {
        ut::expect(visit([&](const std::runtime_error& re) {
          ut::expect(re.what() == "test throw"sv);
          ++count;
        }, exptr));
      }).catch_exception(exception_handler([&](const std::runtime_error& re) {
        ut::expect(re.what() == "test throw"sv);
        ++count;
      }));

    ut::expect(count == 2) << count;
  };

  "catch_error"_test = [] {
    [[maybe_unused]]
    constexpr auto err_v = ::chttpp::underlying::lib_error_code_tratis::no_error_value;

    int count = 0;

    hr_err().catch_error([&](auto e) -> int {
      ut::expect(e == err_v);
      ++count;
      return -11;
    }).catch_error([&](int e) {
      ut::expect(e == -11_i);
      ++count;
    }).catch_exception([](const auto&) {
      ut::expect(false);
    }).then([](auto&& hr) {
      ut::expect(false);
      
      return hr;
    });

    ut::expect(count == 2);
  };

  "match"_test = [] {
    // 2引数void
    hr_ok().match(
      [](http_response&& res) {
        ut::expect(res.body.empty());
      },
      [](auto&&) {
        ut::expect(false);
      });

    [[maybe_unused]]
    constexpr auto err_v = ::chttpp::underlying::lib_error_code_tratis::no_error_value;

    hr_err().match(
        [](http_response &&) {
          ut::expect(false);
        },
        [](error_code &&err) {
          ut::expect(err == err_v);
        });
    
    // 3引数void
    hr_ok().match(
      [](http_response&& res) {
        ut::expect(res.body.empty());
      },
      [](auto&&) {
        ut::expect(false);
      },
      [](exptr_wrapper&&) {
        ut::expect(false);
      });
    
    tb_exptr().match(
      [](int) {
        ut::expect(false);
      },
      [](error_code&&) {
        ut::expect(false);
      },
      [](exptr_wrapper&& exptr) {
        ut::expect(visit([](const std::runtime_error& re) {
          ut::expect(re.what() == "test throw"sv);
        }, exptr));
      });

    // 2引数非void
    auto n = hr_ok().match(
      [](http_response&& res) {
        ut::expect(res.body.empty());
        return 10u;
      },
      [](auto&&) {
        ut::expect(false);
        return 0;
      });

    ut::expect(n == 10u);

    auto d = hr_err().match(
      [](http_response&&) {
        ut::expect(false);
        return 0.0f;
      },
      [](error_code&& err) {
        ut::expect(err == err_v);
        return 3.1415;
      });

    ut::expect(d == 3.1415);

    auto opt = tb_exptr().match(
      [](int) {
        ut::expect(false);
        return 0.0f;
      },
      [](error_code&&) {
        ut::expect(false);
        return 3.1415;
      });

    ut::expect(opt == std::nullopt);

    // 3引数非void
    auto n2 = hr_ok().match(
      [](http_response&& res) {
        ut::expect(res.body.empty());
        return 10l;
      },
      [](auto&&) {
        ut::expect(false);
        return 0;
      },
      [](exptr_wrapper&&) {
        ut::expect(false);
        return -1;
      });
    
    ut::expect(n2 == 10l);

    // デフォルト値の指定
    auto r1 = hr_err().match(
        [](http_response &&) {
          ut::expect(false);
          return 0.0;
        },
        [](error_code &&err) {
          ut::expect(err == err_v);
          return 1.0;
        },
        -1.0);
      
    ut::expect(r1 == 1.0);

    auto r2 = tb_exptr().match(
        [](int) {
          ut::expect(false);
          return 0;
        },
        [](error_code&&) {
          ut::expect(false);
          return 1;
        },
        20);
      
    ut::expect(r2 == 20);
  };

  "status_message()"_test = [] {
    {
      auto msg = hr_ok().status_message();

      ut::expect(msg == "HTTP/1.1 200 OK");
    }
    {
      auto msg = hr_exptr().status_message();

      ut::expect(msg == "Exception : test throw") << msg;
    }
  };

  "http_result |"_test = [] {
    auto hr = hr_ok_range();

    auto r = hr | std::views::filter([](auto c) { return c % 2 == 0; });

    for (char c = 2; auto n : r) {
      ut::expect(c == n);
      c += 2;
    }
  };
}