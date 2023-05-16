#pragma once

#include "chttpp.hpp"

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

namespace ut = boost::ut;

auto throw_charptr() -> chttpp::detail::exptr_wrapper {
  try {
    throw "test char*";
  } catch (...) {
    return chttpp::detail::exptr_wrapper{};
  }
}

template<std::integral auto N>
auto throw_int() -> chttpp::detail::exptr_wrapper {
  try {
    throw N;
  } catch (...) {
    return chttpp::detail::exptr_wrapper{};
  }
}

auto throw_exception() -> chttpp::detail::exptr_wrapper {
  try {
    throw std::runtime_error{"test runtime_error"};
  } catch (...) {
    return chttpp::detail::exptr_wrapper{};
  }
}

auto throw_true() -> chttpp::detail::exptr_wrapper {
  try {
    throw true;
  } catch (...) {
    return chttpp::detail::exptr_wrapper{};
  }
}

void exptr_wrapper_test() {
  using namespace std::string_view_literals;
  using namespace boost::ut::literals;
  using chttpp::detail::exptr_wrapper;

  "exptr_wrapper visit"_test = [] {
    auto exptr_str = throw_charptr();

    ut::expect(
      visit([](std::string_view str) {
        ut::expect(str == "test char*"sv);
      }, exptr_str)
    );

    ut::expect(
      visit([](const char* str) {
        ut::expect(str == "test char*"sv);
      }, exptr_str)
    );

    ut::expect(
      visit([](std::string str) {
        ut::expect(str == "test char*"sv);
      }, exptr_str)
    );

    auto exptr_int = throw_int<12345>();

    ut::expect(
      visit([](int n) {
        ut::expect(n == 12345);
      }, exptr_int)
    );

    ut::expect(
      visit([](unsigned int n) {
        ut::expect(n == 12345);
      }, exptr_int)
    );

    auto exptr_runtimerr = throw_exception();

    ut::expect(
      visit([](const std::exception& ex) {
        ut::expect(ex.what() == "test runtime_error"sv);
      }, exptr_runtimerr)
    );

    // 現状、標準例外型はリストにない。std::exceptionで受けることを想定
    ut::expect(
      visit([](const std::runtime_error&) {
        ut::expect(false);
      }, exptr_runtimerr) == false
    );

    auto exptr_bool = throw_true();

    ut::expect(
      visit([](bool b) {
        ut::expect(b == true);
      }, exptr_bool)
    );
  };

  "exptr_wrapper visit<T>"_test = [] {
    auto exptr_runtimerr = throw_exception();

    ut::expect(
      visit(std::in_place_type<const std::runtime_error&>,
        [](const std::runtime_error& ex) {
          ut::expect(ex.what() == "test runtime_error"sv);
        }, exptr_runtimerr)
    );
  };
}