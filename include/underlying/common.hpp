#pragma once

#include <variant>
#include <vector>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <span>
#include <iostream>
#include <memory_resource>
#include <unordered_map>
#include <ranges>
#include <algorithm>

namespace chttpp::inline types{

#ifndef CHTTPP_DO_NOT_CUSTOMIZE_ALLOCATOR
  // デフォルト : polymorphic_allocatorによるアロケータカスタイマイズ
  using string_t = std::pmr::string;
  using wstring_t = std::pmr::wstring;
  using header_t = std::pmr::unordered_map<string_t, string_t>;
  template<typename T>
  using vector_t = std::pmr::vector<T>;
#else
  // アロケータカスタイマイズをしない
  using string_t = std::string;
  using header_t = std::unordered_map<string_t, string_t>;
  template<typename T>
  using vector_t = std::vector<T>;
#endif

}

namespace chttpp::detail {

  auto parse_response_header_oneline(header_t& headers, std::string_view header_str) {
    using namespace std::string_view_literals;
    // \r\nは含まないとする

    if (header_str.starts_with("HTTP")) [[unlikely]] {
      const auto line_end_pos = header_str.end();
      headers.emplace("HTTP Ver"sv, std::string_view{ header_str.begin(), line_end_pos });
      return;
    }

    const auto colon_pos = header_str.find(':');
    const auto header_end_pos = header_str.end();
    const auto heade_value_pos = std::ranges::find_if(header_str.begin() + colon_pos + 1, header_end_pos, [](char c) { return c != ' '; });
    headers.emplace(header_str.substr(0, colon_pos), std::string_view{ heade_value_pos, header_end_pos });
  }
}

namespace chttpp::detail {

  using std::ranges::data;
  using std::ranges::size;

  template<typename CharT>
  concept charcter = 
    std::same_as<CharT, char> or
    std::same_as<CharT, wchar_t> or 
    std::same_as<CharT, char8_t> or
    std::same_as<CharT, char16_t> or
    std::same_as<CharT, char32_t>;


  struct http_response {
    std::pmr::vector<char> body;
    header_t headers;
  };

  template<typename Err>
  class basic_result {
    std::variant<http_response, Err> m_either;
    const std::uint16_t m_status_code;

  public:

    template<typename T = Err>
    basic_result(T&& e, std::uint16_t code) noexcept(std::is_nothrow_constructible_v<Err, T>)
      : m_either{std::in_place_index<1>, std::forward<T>(e)}
      , m_status_code{code}
    {}

    basic_result(http_response&& res, std::uint16_t code) noexcept(std::is_nothrow_move_constructible_v<http_response>)
      : m_either{std::in_place_index<0>, std::move(res)}
      , m_status_code{code}
    {}

    basic_result(basic_result&& that) noexcept(std::is_nothrow_move_constructible_v<std::variant<http_response, Err>>)
      : m_either{std::move(that.m_either)}
      , m_status_code{that.m_status_code}
    {}

    explicit operator bool() const noexcept {
      return m_either.index() == 0;
    }

    auto status_code() const noexcept -> std::uint16_t {
      return m_status_code;
    }

    auto response_body() const -> std::string_view {
      const auto &response = std::get<0>(m_either);
      return {data(response.body), size(response.body)};
    }

    template<charcter CharT>
    auto response_body() const -> std::basic_string_view<CharT> {
      const auto &response = std::get<0>(m_either);
      return { reinterpret_cast<const CharT*>(data(response.body)), size(response.body) / sizeof(CharT)};
    }

    auto response_data() const -> std::span<char> {
      const auto &response = std::get<0>(m_either);
      return {data(response.body), size(response.body)};
    }

    template<typename ElementType>
      requires std::is_trivially_copyable_v<ElementType>  // 制約、これで足りてる？
    auto response_data() const -> std::span<ElementType> {
      const auto &response = std::get<0>(m_either);
      return {reinterpret_cast<const ElementType*>(data(response.body)), size(response.body) / sizeof(ElementType)};
    }

    auto error_to_string() const -> std::pmr::string;

    friend auto operator<<(std::ostream& os, const basic_result& self) -> std::ostream& {
      if (bool(self) == false) {
        os << self.error_to_string() << '\n';
      } else {
        os << "No error, communication completed successfully.\n";
      }
      return os;
    }

    // optional/expected的インターフェース

    auto value() -> http_response& {
      return std::get<0>(m_either);
    }

    auto value() const -> const http_response& {
      return std::get<0>(m_either);
    }

    auto error() const -> Err {
      return std::get<1>(m_either);
    }
  };
}