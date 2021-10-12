#pragma once

#include <string_view>
#include <concepts>
#include <ranges>
#include <algorithm>

namespace chttpp::mime_types::inline discrete_types {

  struct def_tag_t {};

  template<std::size_t N, typename = def_tag_t>
  struct type_base {
    static constexpr std::size_t Len = N - 1;
    const char category_name[N];
  };

  template<std::size_t N>
  type_base(const char (&)[N]) -> type_base<N>;

  inline constexpr type_base application = {"application"};
}

namespace chttpp::mime_types::inline subtype {

  template<std::size_t N>
  class mime_type {
    char name[N];

  public:

    template<typename Type, typename SubType>
    consteval mime_type(const Type& type, const SubType& subtype) {
      auto [i_end, o_end] = std::ranges::copy_n(type.category_name, Type::Len, name);
      *o_end = '/';
      std::ranges::copy_n(subtype.kind_name, SubType::Len, o_end + 1);
    }

    constexpr operator std::string_view() const noexcept {
      return name;
    }
  };

  template<typename T, typename S>
  mime_type(const T&, const S&) -> mime_type<T::Len + 1 + S::Len>;


  template<typename T, std::size_t N>
  struct subtype_base {
    static constexpr std::size_t Len = N;
    const char kind_name[N];

    friend consteval auto operator/(const T& type, const subtype_base& self) {
      return mime_type{type, self};
    }
  };

  template<const auto& type, std::size_t N>
  using subtype_t = subtype_base<std::remove_cvref_t<decltype(type)>, N>;

  inline constexpr subtype_t<discrete_types::application, 4> zip = {"zip"};
}