#pragma once

#include <string_view>

namespace chttpp {

  namespace detail {

    template<typename CharT>
    consteval auto check_null_terminate(const CharT* str, std::size_t N) -> std::basic_string_view<CharT> {
      if (str[N - 1] != CharT{}) {
        throw "The string is not null-terminated!";
      }
      return {str, N - 1};
    }
  }
  
  template<typename CharT>
  class basic_null_terminated_string_view {
    std::basic_string_view<CharT> m_view;
  
  public:

    basic_null_terminated_string_view() = default;

    template<typename Allocator>
    explicit constexpr basic_null_terminated_string_view(const std::basic_string<CharT, std::char_traits<CharT>, Allocator>& str)
      : m_view(str)
    {}

    template<std::size_t N>
    consteval basic_null_terminated_string_view(const CharT(&str_literal)[N])
      : m_view(detail::check_null_terminate(str_literal, N))
    {}

    consteval basic_null_terminated_string_view(std::basic_string_view<CharT> str_view)
      : m_view(detail::check_null_terminate(str_view.data(), str_view.size() + 1))
    {}

    constexpr operator std::basic_string_view<CharT>() const noexcept {
      return m_view;
    }

    constexpr auto str_view() const noexcept -> std::basic_string_view<CharT>{
      return m_view;
    }

    constexpr auto c_str() const noexcept -> const CharT* {
      return m_view.data();
    }
  };

  using nt_string_view = basic_null_terminated_string_view<char>;
  using nt_wstring_view = basic_null_terminated_string_view<wchar_t>;
}