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

  using header_t = std::pmr::unordered_map<std::string, std::string>;

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