#pragma once

#include <string_view>
#include <concepts>
#include <ranges>
#include <algorithm>

namespace chttpp::mime_types::inline discrete_types {

  /**
   * @brief MIME Type（type/subtype）の左辺のtypeを表す型
   * @tparam N typeの名前の文字数
   * @details 2つ目のテンプレートパラメータは同じ文字数のtypeオブジェクトを識別するためのタグ型（あるsubtypeが複数のtypeの/で結合する場合に識別する）
   */
  template<std::size_t N, typename>
  struct type_base {
    static constexpr std::size_t Len = N - 1; // \0を除いた長さ
    const std::string_view name;
  };

#define define_type(name) struct name ## _tag {}; inline constexpr type_base<sizeof(#name), name ## _tag> name = {#name}

  // typeはこれで全部、exampleはサンプルコードで出現するためのものなのでとりあえずいらない？

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

  /**
   * @brief MIME Type（type/subtype）の全体を表す型
   * @tparam N MIME Typeの名前の文字数
   * @details subtypeが+で結合する場合でもその中間値として利用される
   */
  template<std::size_t N>
  struct mime_type {
    char name[N];

    static constexpr std::size_t Len = N - 1; // \0を除いた長さ

    /**
     * @brief MIME Type名を構成する
     * @param type /の左辺、typeを表すオブジェクト
     * @param subtype /の右辺、subtypeを表すオブジェクト
     * @param punct 左辺と右辺を結合する一文字、subtypeが+で結合するものの場合は+を指定する
     */
    template<typename Type, typename SubType>
    consteval mime_type(const Type& type, const SubType& subtype, char punct = '/') {
      // 一応長さチェック
      static_assert((Type::Len + SubType::Len + 1) == N);

      // type名をコピー（\0は含めない）
      auto [i_end, o_end] = std::ranges::copy_n(std::ranges::begin(type.name), Type::Len, name);
      *o_end = punct; // typeの\0位置に/(+)を入れる
      // subtype名をコピー（こちらは\0を含める）
      std::ranges::copy_n(std::ranges::begin(subtype.name), SubType::Len, o_end + 1);
    }

    /**
     * @brief MIME Type名を取得
     * @details GCC11は定数式で実行できないらしい
     */
    constexpr operator std::string_view() const noexcept {
      return name;
    }
  };

  template<typename T, typename S>
  mime_type(const T&, const S&, const char = '/') -> mime_type<T::Len + 1 + S::Len>;

  /**
   * @brief subtypeの型
   * @tparam N subtype名の文字数
   * @tparam T 結合対象のtypeの型
   * @tparam Ts 結合対象のtypeの残り
   * @details クラス一段で1つのtype（すなわちT）に対する/を提供する
   */
  template<std::size_t N, typename T, typename... Ts>
  struct subtype_base : public subtype_base<N, Ts...> {
    friend consteval auto operator/(const T& type, const subtype_base& self) {
      return mime_type{type, self};
    }
  };

  template<std::size_t N, typename T>
  struct subtype_base<N, T> {
    static constexpr std::size_t Len = N;
    const std::string_view name;

    friend consteval auto operator/(const T& type, const subtype_base& self) {
      return mime_type{type, self};
    }
  };

  /**
   * @brief 指定されたtypeと結合する適切なsubtype型
   * @tparam N subtype名の文字数
   * @tparam types 結合対象のtypeオブジェクト
   */
  template<std::size_t N, const auto&... types>
  using subtype_t = subtype_base<N, std::remove_cvref_t<decltype(types)>...>;

#define define_subtype(name, ...) inline constexpr subtype_t<sizeof(#name), __VA_ARGS__> name = {#name}

  define_subtype(plain, discrete_types::text);
  define_subtype(css, discrete_types::text);
  define_subtype(csv, discrete_types::text);
  define_subtype(html, discrete_types::text);
  define_subtype(javascript, discrete_types::text);

  define_subtype(apng, discrete_types::image);
  define_subtype(avif, discrete_types::image);
  define_subtype(gif, discrete_types::image);
  define_subtype(jpeg, discrete_types::image);
  define_subtype(png, discrete_types::image);
  define_subtype(webp, discrete_types::image);
  define_subtype(bmp, discrete_types::image);

  define_subtype(aac, discrete_types::audio);
  define_subtype(ac3, discrete_types::audio);
  define_subtype(wave, discrete_types::audio);
  define_subtype(wav, discrete_types::audio);
  define_subtype(opus, discrete_types::audio);

  define_subtype(mp4, discrete_types::video);

  define_subtype(mpeg, discrete_types::video, discrete_types::audio);
  define_subtype(webm, discrete_types::video, discrete_types::audio);
  define_subtype(ogg, discrete_types::video, discrete_types::audio, discrete_types::application);

  // 数字始まりは全角数字に（_始まりは予約されてる・・・
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

  // -で結合するsubtypeは-を_へ置換
  inline constexpr subtype_t<13, discrete_types::application> octet_stream = {"octet-stream"};
  inline constexpr subtype_t<22, discrete_types::application> x_www_form_urlencoded = {"x-www-form-urlencoded"};

#undef define_subtype

  template<std::size_t N, typename T, typename... Ts>
  struct free_subtype : public free_subtype<N, Ts...> {

    friend consteval auto operator/(const T& type, const free_subtype& self)
      requires std::is_const_v<T>
    {
      return mime_type{type, self};
    }

    friend consteval auto operator+(const T &semi_mime, const free_subtype &self)
      requires (not std::is_const_v<T>)
    {
      return mime_type{semi_mime, self, '+'};
    }
  };

  template<std::size_t N, typename T>
  struct free_subtype<N, T> {
    static constexpr std::size_t Len = N;
    const std::string_view name;

    friend consteval auto operator/(const T& type, const free_subtype& self)
      requires std::is_const_v<T>
    {
      return mime_type{type, self};
    }

    friend consteval auto operator+(const T &semi_mime, const free_subtype &self)
      requires (not std::is_const_v<T>)
    {
      return mime_type{semi_mime, self, '+'};
    }
  };

#define define_semi_subtype_lhs(name, ...) struct name ## _tag : subtype_t<sizeof(#name), __VA_ARGS__>{}; inline constexpr name ## _tag name = {#name}
#define define_semi_subtype_rhs(name, ...) inline constexpr free_subtype<sizeof(#name), __VA_ARGS__> name = {#name}
#define D(arg) decltype(arg)
  
  // /で結合した後の+待ち系
  define_semi_subtype_lhs(svg, discrete_types::image);
  define_semi_subtype_lhs(atom, discrete_types::application);

  // xmlは単独でsubtypeになるし、+で結合してsubtypeとなることもある
  define_semi_subtype_rhs(xml, D(image/svg), D(discrete_types::text), D(application/atom));

#undef D
#undef define_semi_subtype_rhs
#undef define_semi_subtype_lhs
}