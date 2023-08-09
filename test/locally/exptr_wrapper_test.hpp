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

auto throw_optint() -> chttpp::detail::exptr_wrapper {
  try {
    throw std::optional<int>{20};
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

    // 文字列の場合はstring_viewへの暗黙変換のみが許可されている
    ut::expect(not
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

    // int -> unsigned int の変換は許可されていない（厳しいか・・・？
    ut::expect(not
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

    ut::expect(
      visit([](const std::runtime_error& ex) {
        ut::expect(ex.what() == "test runtime_error"sv);
      }, exptr_runtimerr)
    );

    auto exptr_bool = throw_true();

    ut::expect(
      visit([](bool b) {
        ut::expect(b == true);
      }, exptr_bool)
    );
  };

  "exptr_wrapper visit<T>"_test = [] {
    auto exptr_opt = throw_optint();

    // デフォルトではあらかじめ登録されていない型以外は呼べない
    ut::expect(
      visit([](const std::optional<int>&) {
        ut::expect(false);
      }, exptr_opt) == false
    );

    // 明示的に型を指定する
    ut::expect(
      visit(std::in_place_type<const std::optional<int>&>, [](const std::optional<int>& opt) {
        ut::expect(opt.has_value());
        ut::expect(opt == 20);
      }, exptr_opt)
    );

  };

  "exptr_wrapper member visit"_test = [] {
    auto exptr_str = throw_charptr();

    ut::expect(exptr_str.visit([](std::string_view str) { ut::expect(str == "test char*"sv); }));

    ut::expect(exptr_str.visit([](const char *str){ ut::expect(str == "test char*"sv); }));

    // 文字列の場合はstring_viewへの暗黙変換のみが許可されている
    ut::expect(not exptr_str.visit([](std::string str){ ut::expect(str == "test char*"sv); }));
  };

  "exptr_wrapper stream output"_test = [] {
    {
      auto exptr_str = throw_charptr();
      std::stringstream ss;

      ss << exptr_str;

      ut::expect(ss.view() == "test char*"sv);
    }
    {
      auto exptr_runtimerr = throw_exception();
      std::stringstream ss;

      ss << exptr_runtimerr;

      ut::expect(ss.view() == "test runtime_error"sv);
    }
    {
      auto exptr_int = throw_int<12345>();
      std::stringstream ss;

      ss << exptr_int;

      ut::expect(ss.view() == "12345"sv);
    }
  };
}