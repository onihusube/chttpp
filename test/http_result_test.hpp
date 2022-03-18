#pragma once

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
/*
auto hr_exptr() -> chttpp::http_result {
  try {
    throw std::runtime_error{"exception test."};
  } catch (...) {
    return chttpp::http_result{std::current_exception()};
  }
}*/

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

/*
  "catch_exception"_test = []
  {
    hr_exptr().then([](http_response&& hr) {
        ut::expect(false);
        return hr;
      }).then([](http_response&& hr) {
        ut::expect(false);
        return hr;
      }).then([](http_response&& hr) {
        ut::expect(false);
        return hr;
      }).catch_error([](const auto&) {
        ut::expect(false);
      }).catch_exception([](const auto& exptr) {
        try {
          std::rethrow_exception(exptr);
        } catch (const std::runtime_error& re) {
          ut::expect(re.what() == "exception test."sv);
        } catch (...) {
          ut::expect(false);
        }
      }).catch_exception([](const auto& exptr) {
        try {
          std::rethrow_exception(exptr);
        } catch (const std::runtime_error& re) {
          ut::expect(re.what() == "exception test."sv);
        } catch (...) {
          ut::expect(false);
        }
      });
  };
  */
}