#pragma once

#include <string_view>
#include <ranges>
#include <algorithm>

namespace chttpp::headers::detail {
  template<std::size_t N>
  class header_base {
    char m_header_value[N]{};
  public:

    consteval header_base(std::string_view str) {
      std::ranges::copy(str | std::views::transform([](char c) { return (c == '_') ? '-' : c; }), 
                        std::ranges::begin(m_header_value));
    }

    constexpr operator std::string_view() const noexcept {
      return m_header_value;
    }
  };

  template<std::size_t N>
  header_base(const char(&)[N]) -> header_base<N>;
}

namespace chttpp::headers {
  /*
  HTTP/2以降、ヘッダ名は小文字オンリー
  HTTP/1.1以前は小文字と大文字を区別しないとされていた
  そのため、ここでは小文字でヘッダ名を指定する

  HTTP/2 https://www.rfc-editor.org/rfc/rfc7540#section-8.1.2

  > However,
  > header field names MUST be converted to lowercase prior to their
  > encoding in HTTP/2.  A request or response containing uppercase
  > header field names MUST be treated as malformed (Section 8.1.2.6).

  HTTP/3 https://datatracker.ietf.org/doc/html/rfc9114#section-4.2

  > Characters in field names MUST be
  > converted to lowercase prior to their encoding.  A request or
  > response containing uppercase characters in field names MUST be
  > treated as malformed.

  参考
  - https://qiita.com/developer-kikikaikai/items/4a336420750d7b7d0483

  IANAのヘッダ名レジストリ
  - HTTP/3が参照するもの : https://www.iana.org/assignments/http-fields/http-fields.xhtml
  - HTTP/2が参照するもの : https://www.iana.org/assignments/message-headers/message-headers.xhtml
  */

#define HEADER(name) inline constexpr chttpp::headers::detail::header_base name{#name}

  inline namespace representation {
    HEADER(content_type);
    HEADER(content_encoding);
    HEADER(content_language);
    HEADER(content_location);
  }

  inline namespace payload {
    HEADER(content_length);
    HEADER(content_range);
  }

  HEADER(date);
  HEADER(warning);

  inline namespace request {
    HEADER(accept);
    HEADER(accept_encoding);
    HEADER(accept_language);
    HEADER(accept_ranges);
    HEADER(cookie);
    HEADER(authorization);
    HEADER(forwarded);
    HEADER(if_match);
    HEADER(if_range);
    HEADER(if_none_match);
    HEADER(if_modified_since);
    HEADER(if_unmodified_since);
    HEADER(origin);
    HEADER(range);
    HEADER(referer);
    HEADER(user_agent);
  }

  inline namespace response {
    HEADER(access_control_allow_origin);
    HEADER(etag);
    HEADER(last_modified);
    HEADER(set_cookie);
    HEADER(vary);
    HEADER(www_authenticate);
  }

#undef HEADER
}