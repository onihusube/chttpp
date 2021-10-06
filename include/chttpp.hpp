#pragma once

#include <string_view>

#include "underlying/common.hpp"
#include "null_terminated_string_view.hpp"

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

    struct as_byte_seq_impl {

      /**
      * @brief 1. span<const char>へ変換可能な型を受ける
      * @details 右辺値の寿命延長のため、ここではまだ変換しない
      */
      template<std::ranges::contiguous_range R>
        requires requires(R& t) {
            std::ranges::data(t);
          } and std::ranges::sized_range<R>
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
        requires (not is_specialization_of_span_v<T>)
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

    template<typename M, byte_serializable B>
      requires std::convertible_to<const M&, std::string_view>
    auto operator()(nt_string_view URL, const M& mime_type, B&& request_body) const -> http_result {
      // ここ、完全転送の必要あるかな・・・？
      return chttpp::underlying::terse::request_impl(URL, mime_type, cpo::as_byte_seq(std::forward<B>(request_body)), MethodTag{});
    }

    template<typename M, byte_serializable B>
      requires std::convertible_to<const M&, std::string_view>
    auto operator()(nt_wstring_view URL, const M& mime_type, B&& request_body) const -> http_result {
      return chttpp::underlying::terse::request_impl(URL, mime_type, cpo::as_byte_seq(std::forward<B>(request_body)), MethodTag{});
    }
  };
}

namespace chttpp {

  inline constexpr detail::terse_req_impl<detail::tag::get_t> get{};
  inline constexpr detail::terse_req_impl<detail::tag::head_t> head{};
  inline constexpr detail::terse_req_impl<detail::tag::options_t> options{};
  inline constexpr detail::terse_req_impl<detail::tag::trace_t> trace{};

  inline constexpr detail::terse_req_impl<detail::tag::post_t> post{};
}