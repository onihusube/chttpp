#pragma once

#include <string_view>

#include "chttpp.hpp"

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

auto hr_ok() -> chttpp::http_result {
  return chttpp::http_result{chttpp::detail::http_response{.body{}, .headers = { {"host", "http_result test"} }, .status_code = 200}};
}

auto hr_err() -> chttpp::http_result {
  using E = chttpp::http_result::error_type;

  return chttpp::http_result{E{}};
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
  using chttpp::detail::http_response;

  "then"_test = []
  {
    hr_ok().then([](http_response&& hr) {
        ut::expect(hr.body.empty());
        return hr;
      }).then([](http_response&& hr) {
        ut::expect(hr.headers.size() == 1_ull);
        return hr;
      }).then([](http_response&& hr) {
        ut::expect(hr.status_code == 200_i);
        return hr;
      }).catch_error([](const auto&) {
        ut::expect(false);
      }).catch_exception([](const auto&) {
        ut::expect(false);
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
        ut::expect(hr.status_code == 200_i);
      }).catch_error([](const auto&) {
        ut::expect(false);
      }).catch_exception([](const auto&) {
        ut::expect(false);
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
        try {
          std::rethrow_exception(exptr);
        } catch (const std::runtime_error& re) {
          ut::expect(re.what() == "test throw"sv);
          ++count;
        } catch (...) {
          ut::expect(false);
        }
      }).catch_exception([&](const auto& exptr) {
        try {
          std::rethrow_exception(exptr);
        } catch (const std::runtime_error& re) {
          ut::expect(re.what() == "test throw"sv);
          ++count;
        } catch (...) {
          ut::expect(false);
        }
      });

    ut::expect(count == 2);
  };

  "catch_error"_test = [] {
    using err_t = chttpp::http_result::error_type;

    int count = 0;

    hr_err().catch_error([&](auto e) -> int {
      ut::expect(e == err_t{});
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
}