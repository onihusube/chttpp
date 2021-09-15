#pragma once

#include <variant>
#include <vector>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <span>
#include <iostream>

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


  template<typename Err, typename Byte = char, typename Allocator = std::allocator<Byte>>
  class basic_result {
    std::variant<std::vector<Byte, Allocator>, Err> m_either;
    const std::uint16_t m_status_code;

  public:

    template<typename T = Err>
    basic_result(T&& e, std::uint16_t code) noexcept(std::is_nothrow_constructible_v<Err, T>)
      : m_either{std::in_place_index<1>, std::forward<T>(e)}
      , m_status_code{code}
    {}

    basic_result(std::vector<Byte, Allocator>&& data, std::uint16_t code) noexcept(std::is_nothrow_move_constructible_v<std::vector<Byte, Allocator>>)
      : m_either{std::in_place_index<0>, std::move(data)}
      , m_status_code{code}
    {}

    basic_result(basic_result&& that) noexcept(std::is_nothrow_move_constructible_v<std::variant<std::vector<Byte, Allocator>, Err>>)
      : m_either{std::move(that.m_either)}
      , m_status_code{that.m_status_code}
    {}

    explicit operator bool() const noexcept {
      return m_either.index() == 0;
    }

    auto status() const noexcept -> std::uint16_t {
      return m_status_code;
    }

    auto response() const -> std::string_view {
      const auto &bytes = std::get<0>(m_either);
      return {data(bytes), size(bytes)};
    }

    template<charcter CharT>
    auto response() const -> std::basic_string_view<CharT> {
      const auto &bytes = std::get<0>(m_either);
      return { reinterpret_cast<const CharT*>(data(bytes)), size(bytes) / sizeof(CharT)};
    }

    auto response_body() const -> std::span<Byte> {
      const auto &bytes = std::get<0>(m_either);
      return {data(bytes), size(bytes)};
    }

    template<typename ElementType>
      requires std::is_trivially_copyable_v<ElementType>  // 制約、これで足りてる？
    auto response_body() const -> std::span<ElementType> {
      const auto &bytes = std::get<0>(m_either);
      return {reinterpret_cast<const ElementType*>(data(bytes)), size(bytes) / sizeof(ElementType)};
    }

    auto error_to_string() const -> std::string;

    friend auto operator<<(std::ostream& os, const basic_result& self) -> std::ostream& {
      if (bool(self) == false) {
        os << self.error_to_string() << '\n';
      } else {
        os << "No error, communication completed successfully.\n";
      }
      return os;
    }

    // optional/expected的インターフェース

    auto value() -> std::vector<Byte, Allocator>& {
      return std::get<0>(m_either);
    }

    auto value() const -> const std::vector<Byte, Allocator>& {
      return std::get<0>(m_either);
    }

    auto error() const -> Err {
      return std::get<1>(m_either);
    }
  };
}