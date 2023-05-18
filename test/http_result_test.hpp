#pragma once

#include <string_view>

#include "chttpp.hpp"

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

auto hr_ok() -> chttpp::http_result {
  return chttpp::http_result{chttpp::detail::http_response{ {}, {}, { {"host", "http_result test"} }, chttpp::detail::http_status_code{200} }};
}

auto hr_err() -> chttpp::http_result {
  return chttpp::http_result{::chttpp::underlying::lib_error_code_tratis::no_error_value};
}

decltype(auto) hr_exptr() {
  return hr_ok().then([](auto&&) {
    throw std::runtime_error{"test throw"};
    return 10;
  });
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
        ut::expect(hr.headers.size() == 1_ull);
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
  };

  "then void"_test = []
  {
    hr_ok().then([](const http_response& hr) {
        ut::expect(hr.body.empty());
      }).then([](http_response&& hr) {
        ut::expect(hr.headers.size() == 1_ull);
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

    hr_exptr().then([](int&& hr) {
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

    ut::expect(count == 2);
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
    
    hr_exptr().match(
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

    auto opt = hr_exptr().match(
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

    auto r2 = hr_exptr().match(
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
}