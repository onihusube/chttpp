#pragma once

#include <string_view>
#include <concepts>
#include <ranges>
#include <algorithm>

namespace chttpp::headers::detail {

  /**
   * @brief ヘッダオブジェクトに=で値を追加してヘッダペアを作るときの中間型
   * @tparam T =の右辺の型
   * @details content-typeヘッダにmime_typeオブジェクトを入れるとき、一時オブジェクトの参照を取るのを回避するためのもの（内部に一旦コピーして保持する）
   */
  template<typename T>
  struct headerpair_precursor {
    std::string_view name;
    T value;

    constexpr operator std::pair<std::string_view, std::string_view>() const noexcept {
      return {name, value};
    }
  };

  template<std::size_t N, bool Req = false>
  class header_base {
    char m_header_value[N]{};
  public:

    consteval header_base(std::string_view str, bool = false) {
      std::ranges::copy(str | std::views::transform([](char c) { return (c == '_') ? '-' : c; }), 
                        std::ranges::begin(m_header_value));
    }

    constexpr operator std::string_view() const noexcept {
      return m_header_value;
    }

    /**
     * @brief ヘッダ名=値、の形でヘッダ設定できるようにするための=
     * @details リクエストヘッダの事前定義オブジェクトに対してのみ有効（にする
     * @param value ヘッダの値
     * @return ヘッダペア（std::pair<std::string_view, std::string_view>に暗黙変換可能な型の値）
     */
    template<std::convertible_to<std::string_view> T>
      requires Req
    constexpr auto operator=(T&& value) const noexcept {
      return headerpair_precursor{.name = m_header_value, .value = value};
    }
  };

  template<std::size_t N>
  header_base(const char(&)[N]) -> header_base<N>;

  template<std::size_t N>
  header_base(const char(&)[N], bool) -> header_base<N, true>;
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
#define REQ_HEADER(name) inline constexpr chttpp::headers::detail::header_base name{#name, true}

  inline namespace representation {
    REQ_HEADER(content_type);
    REQ_HEADER(content_encoding);
    REQ_HEADER(content_language);
    REQ_HEADER(content_location);
  }

  inline namespace payload {
    HEADER(content_length);
    REQ_HEADER(content_range);
  }

  REQ_HEADER(date);
  REQ_HEADER(warning);

  inline namespace request {
    REQ_HEADER(accept);
    REQ_HEADER(accept_encoding);
    REQ_HEADER(accept_language);
    REQ_HEADER(accept_ranges);
    REQ_HEADER(cookie);
    REQ_HEADER(authorization);
    REQ_HEADER(forwarded);
    REQ_HEADER(if_match);
    REQ_HEADER(if_range);
    REQ_HEADER(if_none_match);
    REQ_HEADER(if_modified_since);
    REQ_HEADER(if_unmodified_since);
    REQ_HEADER(origin);
    REQ_HEADER(range);
    REQ_HEADER(referer);
    REQ_HEADER(user_agent);
  }

  inline namespace response {
    HEADER(access_control_allow_origin);
    HEADER(etag);
    HEADER(last_modified);
    HEADER(set_cookie);
    HEADER(vary);
    HEADER(www_authenticate);

    inline constexpr chttpp::headers::detail::header_base HTTP_ver{"HTTP Ver"};
  }

#undef REQ_HEADER
#undef HEADER
}