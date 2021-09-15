#pragma once

#include <string_view>

#ifdef _MSC_VER

#include "underlying/winhttp.hpp"

#else

#include "underlying/libcurl.hpp"

#endif


namespace chttpp::detail {

  struct get_impl {

    /*auto operator()(std::string_view URL) -> http_result {
      chttpp::underlying::test_get(URL);
    }*/

    /*auto operator()(std::wstring_view URL) {

    }*/
  };
}