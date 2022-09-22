#pragma once

#include <string_view>
#include <ranges>
#include <compare>

namespace chttpp::detail {

  /**
   * @brief コンパイル時文字列のnull終端判定を行う
   * @param str 文字列の先頭ポインタ
   * @param N 文字列のnullを含む長さ
   */
  template<typename CharT>
  consteval auto check_null_terminate(const CharT* str, std::size_t N) -> std::basic_string_view<CharT> {
    if (str[N - 1] != CharT{}) {
      throw "The string is not null-terminated!";
    }
    return {str, N - 1};
  }

  /**
   * @brief 実行時にnull終端が保証されたstring_view
   * @details 実行時にはnull終端保証のあるstd::stringからのみ構築可能とし
   * @details そのほかの文字列からの構築はコンパイル時のみとし、かつnull終端をチェックする事で保証する
   */
  template<typename CharT>
  class basic_null_terminated_string_view {
    std::basic_string_view<CharT> m_view;

  public:
    basic_null_terminated_string_view() = default;

    template<typename Traits, typename Allocator>
    constexpr basic_null_terminated_string_view(const std::basic_string<CharT, Traits, Allocator>& str) noexcept
      : m_view(str)
    {}

    template<typename Traits, typename Allocator>
    basic_null_terminated_string_view(std::basic_string<CharT, Traits, Allocator>&&) = delete;

    template<std::size_t N>
    consteval basic_null_terminated_string_view(const CharT(&str_literal)[N])
      : m_view(check_null_terminate(str_literal, N))
    {}

    consteval basic_null_terminated_string_view(std::basic_string_view<CharT> str_view)
      : m_view(check_null_terminate(str_view.data(), str_view.size() + 1))
    {}

    basic_null_terminated_string_view(const basic_null_terminated_string_view&) = default;
    basic_null_terminated_string_view(basic_null_terminated_string_view&&) = default;

    basic_null_terminated_string_view& operator=(const basic_null_terminated_string_view &) = default;
    basic_null_terminated_string_view& operator=(basic_null_terminated_string_view&&) = default;

    constexpr operator std::basic_string_view<CharT>() const noexcept {
      return m_view;
    }

    constexpr auto str_view() const noexcept -> std::basic_string_view<CharT>{
      return m_view;
    }

    /**
     * @brief null終端された文字列へのポインタを得る
     */
    constexpr auto c_str() const noexcept -> const CharT* {
      return m_view.data();
    }

    constexpr auto data() const noexcept -> const CharT* {
      return m_view.data();
    }

    friend constexpr std::strong_ordering operator<=>(const basic_null_terminated_string_view &, const basic_null_terminated_string_view &) = default;
  };

#ifdef _MSC_VER
  using nt_string_view = std::string_view;
  using nt_wstring_view = std::wstring_view;
#else 
  using nt_string_view = basic_null_terminated_string_view<char>;
  using nt_wstring_view = basic_null_terminated_string_view<wchar_t>;
#endif  

}

/*
template <typename CharT>
inline constexpr bool std::ranges::enable_borrowed_range<basic_null_terminated_string_view<CharT>> = true;

template <typename CharT>
inline constexpr bool std::ranges::enable_view<basic_null_terminated_string_view<CharT>> = true;
*/