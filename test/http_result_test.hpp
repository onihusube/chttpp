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
      }).catch_err([](const auto&) {
        ut::expect(false);
      });
  };
}