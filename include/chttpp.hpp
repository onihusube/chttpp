#pragma once

#include <string_view>

#include "underlying/common.hpp"
#include "null_terminated_string_view.hpp"
#include "mime_types.hpp"

#ifdef _MSC_VER

#include "underlying/winhttp.hpp"

#define url_str(str) L ## str

#else

#include "underlying/libcurl.hpp"

#define url_str(str) str

#endif


namespace chttpp {

  namespace detail {

    template<typename T>
    inline constexpr bool is_specialization_of_span_v = false;

    template<typename T>
    inline constexpr bool is_specialization_of_span_v<std::span<T>> = true;

    template<typename T>
    inline constexpr bool is_character_ptr_v = false;

    template<character T>
    inline constexpr bool is_character_ptr_v<const T*> = true;

    template<typename T>
    inline constexpr bool is_character_literal_v = false;

    template<character T, std::size_t N>
    inline constexpr bool is_character_literal_v<const T(&)[N]> = true;

    template<typename T>
    inline constexpr bool is_specialization_of_string_view_v = false;

    template<typename T>
    inline constexpr bool is_specialization_of_string_view_v<std::basic_string_view<T>> = true;

    template<typename T>
    inline constexpr bool is_specialization_of_string_v = false;

    template<typename T, typename Alloc>
    inline constexpr bool is_specialization_of_string_v<std::basic_string<T, std::char_traits<T>, Alloc>> = true;

    template <typename T>
    concept string_like =
      is_character_ptr_v<std::remove_cvref_t<T>> or 
      is_character_literal_v<T> or 
      is_specialization_of_string_view_v<std::remove_cvref_t<T>> or
      is_specialization_of_string_v<std::remove_cvref_t<T>>;

    template<typename T>
    struct string_like_traits;

    template <typename T>
    struct string_like_traits<std::basic_string_view<T>> {
      using element_type = T;
    };

    template <typename T, typename Alloc>
    struct string_like_traits<std::basic_string<T, std::char_traits<T>, Alloc>> {
      using element_type = T;
    };

    template<character T>
    struct string_like_traits<const T*> {
      using element_type = T;
    };

    template<character T, std::size_t N>
    struct string_like_traits<T[N]> {
      using element_type = T;
    };

    struct as_byte_seq_impl {

      /**
      * @brief 0. 文字列をバイト列へ変換する
      * @details 利用側はこの結果を直接span<const char>を受け取る関数へ渡すことを想定するので右辺値が来ても良い
      */
      template<string_like S>
      [[nodiscard]]
      auto operator()(S&& str) const noexcept -> std::span<const char> {
        using CharT = string_like_traits<std::remove_cvref_t<S>>::element_type;

        // string_viewに変換してからシリアライズする
        std::basic_string_view<CharT> str_view{str};
        return {reinterpret_cast<const char *>(std::ranges::data(str_view)), sizeof(CharT) * std::ranges::size(str_view)};
      }

      /**
      * @brief 1. contiguous_rangeな範囲をバイト列へ変換する
      * @details 利用側はこの結果を直接span<const char>を受け取る関数へ渡すことを想定するので右辺値が来ても良い
      */
      template<std::ranges::contiguous_range R>
        requires (not string_like<R>) and
                 requires(R& t) {
                   std::ranges::data(t);
                 } and
                 std::ranges::sized_range<R>
      [[nodiscard]]
      auto operator()(R&& t) const noexcept -> std::span<const char> {
        return {reinterpret_cast<const char*>(std::ranges::data(t)), sizeof(std::ranges::range_value_t<R>) * std::ranges::size(t)};
      }

      /**
      * @brief 2. as_byte_seq()メンバ関数を呼び出し、その結果をその型の値のバイト列として取得する
      * @details 利用側はこの結果を直接span<const char>を受け取る関数へ渡すことを想定
      * @details 従ってユーザー定義as_byte_seq()は右辺値を返しても良い
      */
      template<typename T>
        requires requires(T&& t) {
          { std::forward<T>(t).as_byte_seq() } -> std::convertible_to<std::span<const char>>;
        }
      [[nodiscard]]
      decltype(auto) operator()(T&& t) const noexcept(noexcept(std::forward<T>(t).as_byte_seq())) {
        return std::forward<T>(t).as_byte_seq();
      }

      /**
      * @brief 3. as_byte_seq()非メンバ関数を呼び出し、その結果をその型の値のバイト列として取得する
      * @details 利用側はこの結果を直接span<const char>を受け取る関数へ渡すことを想定
      * @details 従ってユーザー定義as_byte_seq()は右辺値を返しても良い
      */
      template<typename T>
        requires requires(T&& t) {
          { as_byte_seq(std::forward<T>(t)) } -> std::convertible_to<std::span<const char>>;
        }
      [[nodiscard]]
      decltype(auto) operator()(T&& t) const noexcept(noexcept(as_byte_seq(std::forward<T>(t)))) {
        return as_byte_seq(std::forward<T>(t));
      }

      /**
      * @brief 4. C-likeな構造体のオブジェクトをそのままシリアライズする
      */
      template<substantial T>
        requires (not is_specialization_of_span_v<T>) and (not string_like<const T&>)
      [[nodiscard]]
      auto operator()(const T& t) const noexcept -> std::span<const char> {
        return {reinterpret_cast<const char*>(std::addressof(t)), sizeof(t)};
      }

      /**
      * @brief 5. span<T>をspan<const char>へ変換する
      */
      template<typename T>
      [[nodiscard]]
      auto operator()(std::span<T> s) const noexcept -> std::span<const char> {
        return {reinterpret_cast<const char*>(s.data()), s.size_bytes()};
      }
    };

    struct load_byte_seq_impl {

      template<typename T>
        requires requires(T& t, std::span<const char> bytes) {
          t.load_byte_seq(bytes);
        }
      void operator()(T& t, std::span<const char> bytes) const noexcept(noexcept(t.load_byte_seq(bytes))) {
        t.load_byte_seq(bytes);
      }

      template<typename T>
        requires requires(T& t, std::span<const char> bytes) {
          load_byte_seq(t, bytes);
        }
      void operator()(T& t, std::span<const char> bytes) const noexcept(noexcept(load_byte_seq(t, bytes))) {
        load_byte_seq(t, bytes);
      }

      /**
      * @brief 3. C-likeな構造体のオブジェクト1つをバイト列からロードする
      */
      template<substantial T>
        requires (not is_specialization_of_span_v<T>)
      void operator()(T& t, std::span<const char> bytes) const {
        assert(sizeof(T) <= bytes.size());
        // memcpyによってデシリアライズ
        std::memcpy(std::addressof(t), bytes.data(), sizeof(T));
      }

      /**
      * @brief 4. C-likeな構造体の連続範囲へバイト列からロードする
      */
      template<std::ranges::contiguous_range R>
        requires std::ranges::sized_range<R> and
                 substantial<std::ranges::range_value_t<R>>
      void operator()(R& r, std::span<const char> bytes) const {
        // 要素型
        using T = std::ranges::range_value_t<R>;
        // 短い方のサイズに合わせる
        const std::size_t len = std::min(std::ranges::size(r), bytes.size() / sizeof(T));
        // memcpyによってデシリアライズ
        std::memcpy(std::ranges::data(r), bytes.data(), len * sizeof(T));
      }

      /**
      * @brief 5. C-likeな構造体の範囲へバイト列からロードする
      */
      template<std::ranges::forward_range R>
        requires substantial<std::ranges::range_value_t<R>>
      void operator()(R& r, std::span<const char> bytes) const {
        // 要素型
        using T = std::ranges::range_value_t<R>;

        // 宛先に確保されている長さ
        std::size_t dst_len;
        if constexpr (std::ranges::sized_range<R>) {
          dst_len = std::ranges::size(r);
        } else {
          dst_len = std::ranges::distance(r);
        }

        // 短い方のサイズに合わせる
        const std::size_t len = std::min(dst_len, bytes.size() / sizeof(T));

        // 1要素づつコピー
        std::ranges::copy_n(reinterpret_cast<const T*>(bytes.data()), len, std::ranges::begin(r));
      }
    };
  }

  inline namespace cpo {
    /**
    * @brief オブジェクトをバイトシーケンスへ変換する
    * @details as_byte_seq(E);のように呼び出し、Eの示すオブジェクトをバイト列へ変換する
    * @return std::span<const char>へ変換可能な型の値
    */
    inline constexpr detail::as_byte_seq_impl as_byte_seq{};

    /**
    * @brief バイトシーケンスから所望のデータを読み出す
    * @details load_byte_seq(E, bytes);のように呼び出し、Eの示すオブジェクトへbytesから読み込んだ値をロードする
    * @return なし
    */
    inline constexpr detail::load_byte_seq_impl load_byte_seq{};
  }

  inline namespace concepts {

    /**
    * @brief バイト列として扱うことのできる型
    */
    template<typename T>
    concept byte_serializable = requires(T&& t) {
      cpo::as_byte_seq(std::forward<T>(t));
    };

    /**
    * @brief バイト列からロード可能な型
    */
    template<typename T>
    concept byte_deserializable = requires(T& t, std::span<const char> bytes) {
      cpo::load_byte_seq(t, bytes);
    };
  }
}

namespace chttpp::detail {

  template<typename MethodTag>
  struct terse_req_impl {

    auto operator()(nt_string_view URL) const -> http_result {
      return chttpp::underlying::terse::request_impl(URL, MethodTag{});
    }

    auto operator()(nt_wstring_view URL) const -> http_result {
      return chttpp::underlying::terse::request_impl(URL, MethodTag{});
    }
  };

  template<detail::tag::has_reqbody_method MethodTag>
  struct terse_req_impl<MethodTag> {

    template<std::convertible_to<std::string_view> MimeType, byte_serializable Body>
    auto operator()(nt_string_view URL, Body&& request_body, MimeType&& mime_type) const -> http_result {
      // ここ、request_bodyの完全転送の必要あるかな・・・？
      return chttpp::underlying::terse::request_impl(URL, std::forward<MimeType>(mime_type), cpo::as_byte_seq(std::forward<Body>(request_body)), MethodTag{});
    }

    template<std::convertible_to<std::string_view> MimeType, byte_serializable Body>
    auto operator()(nt_wstring_view URL, Body&& request_body, MimeType&& mime_type) const -> http_result {
      return chttpp::underlying::terse::request_impl(URL, std::forward<MimeType>(mime_type), cpo::as_byte_seq(std::forward<Body>(request_body)), MethodTag{});
    }
  };
}

namespace chttpp {

  inline constexpr detail::terse_req_impl<detail::tag::get_t> get{};
  inline constexpr detail::terse_req_impl<detail::tag::head_t> head{};
  inline constexpr detail::terse_req_impl<detail::tag::options_t> options{};
  inline constexpr detail::terse_req_impl<detail::tag::trace_t> trace{};

  inline constexpr detail::terse_req_impl<detail::tag::post_t> post{};
#ifndef _MSC_VER
  inline constexpr detail::terse_req_impl<detail::tag::put_t> put{};
  inline constexpr detail::terse_req_impl<detail::tag::delete_t> delete_{};
#endif
}