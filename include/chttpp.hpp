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

  template<typename MethodTag>
  struct terse_req_impl {

    auto operator()(nt_string_view URL) const -> http_result {
      return chttpp::underlying::terse::request_impl(URL, MethodTag{});
    }

    auto operator()(nt_wstring_view URL) const -> http_result {
      return chttpp::underlying::terse::request_impl(URL, MethodTag{});
    }
  };

  template<detail::tag::has_reqbody_method MethodTag>
  struct terse_req_impl<MethodTag> {

    /*auto operator()(nt_string_view URL) const -> http_result {
      return chttpp::underlying::terse::request_impl(URL, MethodTag{});
    }

    auto operator()(nt_wstring_view URL) const -> http_result {
      return chttpp::underlying::terse::request_impl(URL, MethodTag{});
    }*/
  };
}

namespace chttpp {

  inline constexpr detail::terse_req_impl<detail::tag::get_t> get{};
  inline constexpr detail::terse_req_impl<detail::tag::head_t> head{};
  inline constexpr detail::terse_req_impl<detail::tag::options_t> options{};
  inline constexpr detail::terse_req_impl<detail::tag::trace_t> trace{};
}