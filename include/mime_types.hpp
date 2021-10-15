#pragma once

#include <string_view>
#include <concepts>
#include <ranges>
#include <algorithm>

namespace chttpp::mime_types::inline discrete_types {

  template<std::size_t N, typename>
  struct type_base {
    static constexpr std::size_t Len = N - 1;
    const std::string_view category_name;
  };

#define define_type(name) struct name ## _tag {}; inline constexpr type_base<sizeof(#name), name ## _tag> name = {#name}

  define_type(text);
  define_type(font);
  define_type(audio);
  define_type(image);
  define_type(model);
  define_type(video);
  //define_type(example);
  define_type(application);

#undef define_type
}

namespace chttpp::mime_types::inline subtypes {

  template<std::size_t N>
  class mime_type {
    char name[N];

  public:

    template<typename Type, typename SubType>
    consteval mime_type(const Type& type, const SubType& subtype) {
      static_assert((Type::Len + SubType::Len + 1) == N);

      auto [i_end, o_end] = std::ranges::copy_n(std::ranges::begin(type.category_name), Type::Len, name);
      *o_end = '/';
      std::ranges::copy_n(std::ranges::begin(subtype.kind_name), SubType::Len, o_end + 1);
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
    const std::string_view kind_name;

    friend consteval auto operator/(const T& type, const subtype_base& self) {
      return mime_type{type, self};
    }
  };

  template<std::size_t N, typename T, typename... Ts>
  struct subtype_multitype : public subtype_multitype<N, Ts...> {
    friend consteval auto operator/(const T& type, const subtype_multitype& self) {
      return mime_type{type, self};
    }
  };

  template<std::size_t N, typename T>
  struct subtype_multitype<N, T> : public subtype_base<T, N> {};

  template<std::size_t N, const auto& type1, const auto&... types>
  using subtype_t = std::conditional_t<sizeof...(types) == 0, 
                                       subtype_base<std::remove_cvref_t<decltype(type1)>, N>, 
                                       subtype_multitype<N, std::remove_cvref_t<decltype(type1)>, std::remove_cvref_t<decltype(types)>...>
                                      >;

#define define_subtype(name, ...) inline constexpr subtype_t<sizeof(#name), __VA_ARGS__> name = {#name}

  define_subtype(plain, discrete_types::text);
  define_subtype(css, discrete_types::text);
  define_subtype(csv, discrete_types::text);
  define_subtype(xml, discrete_types::text);
  define_subtype(html, discrete_types::text);
  define_subtype(javascript, discrete_types::text);

  define_subtype(apng, discrete_types::image);
  define_subtype(avif, discrete_types::image);
  define_subtype(gif, discrete_types::image);
  define_subtype(jpeg, discrete_types::image);
  define_subtype(png, discrete_types::image);
  define_subtype(webp, discrete_types::image);
  define_subtype(bmp, discrete_types::image);
  inline constexpr subtype_t<8, discrete_types::image> svg_xml = {"svg+xml"};

  define_subtype(aac, discrete_types::audio);
  define_subtype(ac3, discrete_types::audio);
  define_subtype(wave, discrete_types::audio);
  define_subtype(wav, discrete_types::audio);
  define_subtype(opus, discrete_types::audio);

  define_subtype(mp4, discrete_types::video);

  define_subtype(mpeg, discrete_types::video, discrete_types::audio);
  define_subtype(webm, discrete_types::video, discrete_types::audio);
  define_subtype(ogg, discrete_types::video, discrete_types::audio, discrete_types::application);
  inline constexpr subtype_t<5, discrete_types::video, discrete_types::audio> ３gpp = {"3gpp"};
  inline constexpr subtype_t<6, discrete_types::video, discrete_types::audio> ３gpp2 = {"3gpp2"};

  define_subtype(collection, discrete_types::font);
  define_subtype(otf, discrete_types::font);
  define_subtype(sfnt, discrete_types::font);
  define_subtype(ttf, discrete_types::font);
  define_subtype(woff, discrete_types::font);
  define_subtype(woff2, discrete_types::font);

  define_subtype(vml, discrete_types::model);
  inline constexpr subtype_t<4, discrete_types::model> ３mf = {"3mf"};

  define_subtype(zip, discrete_types::application);
  define_subtype(gzip, discrete_types::application);
  define_subtype(pdf, discrete_types::application);
  define_subtype(json, discrete_types::application);
  define_subtype(pkcs8, discrete_types::application);
  define_subtype(msword, discrete_types::application);
  inline constexpr subtype_t<13, discrete_types::application> octet_stream = {"octet-stream"};
  inline constexpr subtype_t<22, discrete_types::application> x_www_form_urlencoded = {"x-www-form-urlencoded"};


#undef define_subtype
}