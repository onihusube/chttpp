#pragma once

#include <string_view>

#include "underlying/common.hpp"
#include "null_terminated_string_view.hpp"

#ifdef _MSC_VER

#include "underlying/winhttp.hpp"

#define url_str(str) L ## str

#else

#include "underlying/libcurl.hpp"

#define url_str(str) str

#endif


namespace chttpp::detail {

  struct terse_get_impl {

    auto operator()(nt_string_view URL) const -> http_result {
      return chttpp::underlying::terse::get_impl(URL);
    }

    auto operator()(nt_wstring_view URL) const -> http_result {
      return chttpp::underlying::terse::get_impl(URL);
    }
  };

  struct terse_head_impl {

    auto operator()(nt_string_view URL) const -> http_result {
      return chttpp::underlying::terse::head_impl(URL);
    }

    auto operator()(nt_wstring_view URL) const -> http_result {
      return chttpp::underlying::terse::head_impl(URL);
    }
  };

  struct terse_options_impl {

    /*auto operator()(nt_string_view URL) const -> http_result {
      return chttpp::underlying::terse::options_impl(URL);
    }*/

    auto operator()(nt_wstring_view URL) const -> http_result {
      return chttpp::underlying::terse::options_impl(URL);
    }
  };
}

namespace chttpp {
  inline constexpr detail::terse_get_impl get{};
  inline constexpr detail::terse_head_impl head{};
  inline constexpr detail::terse_options_impl options{};
}