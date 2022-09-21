#pragma once

#include <variant>
#include <vector>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <span>
#include <iostream>
#include <unordered_map>
#include <ranges>
#include <algorithm>
#include <cctype>
#include <cassert>

#if __has_include(<memory_resource>)

#include <memory_resource>

#else

#define CHTTPP_DO_NOT_CUSTOMIZE_ALLOCATOR
#warning <memory_resource> is not found. Allocator customization will be disabled.

#endif

#include "null_terminated_string_view.hpp"

#ifdef _MSC_VER

#define OPT_STR(str) L##str

#else

#define OPT_STR(str) str

#endif


namespace chttpp::inline types {

  /**
   * @brief unorderd_map<std::string, T>のheterogenius lookupでstring_viewを利用するためのハッシュクラス
   * @details 追加で、等価比較可能な比較関数オブジェクト型をunorderd_mapに指定する必要がある
   */
  struct string_hash {
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;

    std::size_t operator()(std::string_view str) const noexcept(noexcept(hash_type{}(str))) {
      return hash_type{}(str);
    }
  };

#ifndef CHTTPP_DO_NOT_CUSTOMIZE_ALLOCATOR
  // デフォルト : polymorphic_allocatorによるアロケータカスタイマイズ
  using string_t = std::pmr::string;
  using wstring_t = std::pmr::wstring;
  using header_t = std::pmr::unordered_map<string_t, string_t, string_hash, std::ranges::equal_to>;
  template<typename T>
  using vector_t = std::pmr::vector<T>;
#else
  // アロケータカスタイマイズをしない
  using string_t = std::string;
  using wstring_t = std::string;
  using header_t = std::unordered_map<string_t, string_t, string_hash, std::ranges::equal_to>;
  template<typename T>
  using vector_t = std::vector<T>;
#endif

}

namespace chttpp::inline concepts {
  template <typename T>
  concept fundamental_type_with_substance =
    std::is_scalar_v<T> and
    not std::is_pointer_v<T> and
    not std::is_member_object_pointer_v<T> and
    not std::is_member_function_pointer_v<T> and
    not std::is_null_pointer_v<T>;

  template <typename T>
  concept aggregate_with_substance = std::is_aggregate_v<T> and std::is_trivially_copyable_v<T>;

  // もう少し考慮が必要（これだとstd::vectorでも満たせるっぽい）
  template <typename T>
  concept standard_layout_class = std::is_class_v<T> and std::is_standard_layout_v<T>;

  /**
   * @brief バイト列への/からの読み替えが問題のない型を表す
   */
  template <typename T>
  concept substantial =
    fundamental_type_with_substance<std::remove_reference_t<T>> or
    aggregate_with_substance<T>;
}

namespace chttpp::detail {
  using namespace std::string_view_literals;

  // chttpp デフォルトUser-Agent
  inline constexpr std::string_view default_UA = "Mozilla/5.0 chttpp/0.0.1";
  inline constexpr std::wstring_view default_UA_w = L"Mozilla/5.0 chttpp/0.0.1";

  /**
   * @brief ヘッダ1行分（1つ分）をパースし、適切に保存する
   * @details winhttpとcurlとの共通処理
   * @param headers 保存するmap<string, string>オブジェクの参照
   * @param header_str 1行分のヘッダ要素文字列
   */
  auto parse_response_header_oneline(header_t& headers, std::string_view header_str) {
    using namespace std::string_view_literals;
    // \r\nは含まないとする
    assert(header_str.ends_with("\r\n") == false);

    if (header_str.starts_with("HTTP")) [[unlikely]] {
      const auto line_end_pos = header_str.end();
      headers.emplace("HTTP Ver"sv, std::string_view{ header_str.begin(), line_end_pos });
      return;
    }

    const auto colon_pos = header_str.find(':');
    const auto header_end_pos = header_str.end();
    const auto header_value_pos = std::ranges::find_if(header_str.begin() + colon_pos + 1, header_end_pos, [](char c) { return c != ' '; });

    // キー文字列は全て小文字になるようにする
    // curlはこの処理いらないかもしれない？
    string_t key_str{header_str.substr(0, colon_pos)};
    for (auto& c : key_str) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (const auto [it, inserted] = headers.emplace(std::move(key_str), std::string_view{ header_value_pos, header_end_pos }); not inserted) {
      // ヘッダ要素が重複している場合、値をカンマ区切りリストによって追記する
      // 詳細 : https://this.aereal.org/entry/2017/12/21/190158

      auto& header_value = (*it).second;
      // ヘッダ要素を追加する分の領域を拡張
      header_value.reserve(header_value.length() + std::size_t(std::ranges::distance(header_value_pos, header_end_pos)) + 2);
      header_value.append(", ");
      header_value.append(std::string_view{header_value_pos, header_end_pos});
    }
  }
}

namespace chttpp::detail {
  /**
	* @brief オーバーロード関数オブジェクトを生成する
	* @tparam Fs... オーバーロードする関数呼び出し可能な型のリスト
	*/
	template<typename... Fs>
	struct overloaded : public Fs... {
		using Fs::operator()...;
	};

	/**
	* @brief overloadedの推定ガイド
	*/
	template<typename... Fs>
	overloaded(Fs&&...) -> overloaded<Fs...>;

  template <typename F, typename R, typename... Args>
  concept invocable_r =
    std::invocable<F, Args...> and
    std::same_as<R, std::invoke_result_t<F, Args...>>;

  /**
   * @brief `T`がvoidならmonostateに変換する
   */
  template <typename T>
  using void_to_monostate = std::conditional_t<std::same_as<std::remove_cvref_t<T>, void>, std::monostate, std::remove_cvref_t<T>>;

  template<typename T, typename E>
    requires (not std::same_as<T, E>)
  struct then_impl {
    std::variant<T, E, std::exception_ptr> outcome;
    using V = std::variant<T, E, std::exception_ptr>;

    then_impl(T&& value)
      : outcome{std::in_place_index<0>, std::move(value)}
    {}

    then_impl(E&& err)
      : outcome{std::in_place_index<1>, std::move(err)}
    {}

    then_impl(std::exception_ptr&& exptr)
      : outcome{std::in_place_index<2>, std::move(exptr)}
    {}

    then_impl(then_impl&&) = default;
    then_impl& operator=(then_impl &&) = default;

    template<std::invocable<T&&> F>
      requires (not std::same_as<void, std::invoke_result_t<F, T&&>>)
    auto then(F&& func) && noexcept -> then_impl<std::remove_cvref_t<std::invoke_result_t<F, T&&>>, E> {
      using ret_then_t = then_impl<std::remove_cvref_t<std::invoke_result_t<F, T&&>>, E>;

      try {
        return std::visit(overloaded{
            [&](T &&value) {
              return ret_then_t{ std::invoke(std::forward<F>(func), std::move(value))};
            },
            [](E &&err) {
              return ret_then_t{ std::move(err)};
            },
            [](std::exception_ptr &&exptr) {
              return ret_then_t{ std::move(exptr)};
            }}, std::move(this->outcome));
      } catch (...) {
        return ret_then_t{std::current_exception()};
      }
    }

    /**
     * @brief 戻り値型がvoidの時のオーバーロード、現在のオブジェクトをそのままリターン
     */
    template <invocable_r<void, const T&> F>
    auto then(F&& func) && noexcept -> then_impl&& try {
      // 左辺値で呼び出し
      std::visit(overloaded{
          [&](T& value) {
            std::invoke(std::forward<F>(func), value);
          },
          [](auto&&) noexcept {}
        }, this->outcome);

      // 普通にリターンすると左辺値が帰り、暗黙にコピーが起きる
      return std::move(*this);
    } catch (...) {
      // 実質、Tを保持している場合にのみここに来るはず
      this->outcome.template emplace<2>(std::current_exception());
      return std::move(*this);
    }

    template<std::invocable<E&&> F>
      requires (not std::same_as<E, std::monostate>)
    auto catch_error(F&& func) && noexcept -> then_impl<T, void_to_monostate<std::invoke_result_t<F, E&&>>> try {
      using ret_t = std::invoke_result_t<F, E&&>;
      using ret_then_t = then_impl<T, void_to_monostate<ret_t>>;

      return std::visit(overloaded{
          [&](E &&err) {
            if constexpr (std::same_as<void, std::remove_cvref_t<ret_t>>) {
              std::invoke(std::forward<F>(func), std::move(err));
              return ret_then_t{ std::monostate{}};
            } else {
              return ret_then_t{ std::invoke(std::forward<F>(func), std::move(err))};
            }
          },
          [](T&& v) {
            return ret_then_t{ std::move(v)};
          },
          [](std::exception_ptr &&exptr) {
            return ret_then_t{ std::move(exptr)};
          }
        }, std::move(this->outcome));

    } catch (...) {
      using ret_t = std::invoke_result_t<F, E&&>;
      using ret_then_t = then_impl<T, void_to_monostate<ret_t>>;

      return ret_then_t{ std::current_exception()};
    }

    template<std::invocable<const std::exception_ptr&> F>
      requires requires(F&& f, const std::exception_ptr& exptr) {
        {std::invoke(std::forward<F>(f), exptr)} -> std::same_as<void>;
      }
    auto catch_exception(F&& func) && noexcept -> then_impl try {
      return std::visit(overloaded{
          [](T&& v) {
            return then_impl{std::move(v)};
          },
          [](E &&err) {
            return then_impl{std::move(err)};
          },
          [&](std::exception_ptr &&exptr) {
            std::invoke(std::forward<F>(func), exptr);
            return then_impl{std::move(exptr)};
          }
        }, std::move(this->outcome));
    } catch (...) {
      return then_impl{std::current_exception()};
    }
  };
}

namespace chttpp::detail {

  using std::ranges::data;
  using std::ranges::size;

  template<typename CharT>
  concept character = 
    std::same_as<CharT, char> or
    std::same_as<CharT, wchar_t> or 
    std::same_as<CharT, char8_t> or
    std::same_as<CharT, char16_t> or
    std::same_as<CharT, char32_t>;

  struct enable_move_only {
    enable_move_only() = default;

    enable_move_only(const enable_move_only &) = delete;
    enable_move_only& operator=(const enable_move_only&) = delete;

    enable_move_only(enable_move_only&&) = default;
    enable_move_only& operator=(enable_move_only&&) = default;
  };

  struct http_response : enable_move_only {
    vector_t<char> body;
    header_t headers;
    std::uint16_t status_code;
  };

  template<typename Err>
  class basic_result {
    // exception_ptrも入れ込みたい、そのうち
    std::variant<http_response, Err> m_either;

    auto error_to_string() const -> string_t;

  public:
    using error_type = Err;

    template<typename T = Err>
      requires std::constructible_from<Err, T>
    explicit basic_result(T&& e) noexcept(std::is_nothrow_constructible_v<Err, T>)
      : m_either{std::in_place_index<1>, std::forward<T>(e)}
    {}

    explicit basic_result(http_response&& res) noexcept(std::is_nothrow_move_constructible_v<http_response>)
      : m_either{std::in_place_index<0>, std::move(res)}
    {}

    basic_result(basic_result&& that) noexcept(std::is_nothrow_move_constructible_v<std::variant<http_response, Err>>)
      : m_either{std::move(that.m_either)}
    {}

    basic_result(const basic_result&) = delete;
    basic_result &operator=(const basic_result&) = delete;

    explicit operator bool() const noexcept {
      return m_either.index() == 0;
    }

    bool has_response() const noexcept {
      return m_either.index() == 0;
    }

    auto status_code() const -> std::uint16_t {
      assert(bool(*this));
      const auto &response = std::get<0>(m_either);
      return response.status_code;
    }

    auto response_body() const & -> std::string_view {
      assert(bool(*this));
      const auto &response = std::get<0>(m_either);
      return {data(response.body), size(response.body)};
    }

    template<character CharT>
    auto response_body() const & -> std::basic_string_view<CharT> {
      assert(bool(*this));
      const auto &response = std::get<0>(m_either);
      return { reinterpret_cast<const CharT*>(data(response.body)), size(response.body) / sizeof(CharT)};
    }

    auto response_data() & -> std::span<char> {
      assert(bool(*this));
      auto& response = std::get<0>(m_either);
      return response.body;
    }

    auto response_data() const & -> std::span<const char> {
      assert(bool(*this));
      const auto &response = std::get<0>(m_either);
      return response.body;
    }

    template<substantial ElementType>
    auto response_data(std::size_t N = std::dynamic_extent) & -> std::span<const ElementType> {
      assert(bool(*this));
      auto& response = std::get<0>(m_either);
      const std::size_t count = std::min(N, size(response.body) / sizeof(ElementType));

      return { reinterpret_cast<ElementType*>(data(response.body)), count };
    }

    template<substantial ElementType>
    auto response_data(std::size_t N = std::dynamic_extent) const & -> std::span<ElementType> {
      assert(bool(*this));
      const auto &response = std::get<0>(m_either);
      const std::size_t count = std::min(N, size(response.body) / sizeof(ElementType));

      return {reinterpret_cast<const ElementType *>(data(response.body)), count};
    }

    auto response_header() const & -> const header_t& {
      assert(bool(*this));
      const auto &response = std::get<0>(m_either);
      return response.headers;
    }

    auto response_header(std::string_view header_name) const & -> std::string_view {
      assert(bool(*this));
      const auto &headers = std::get<0>(m_either).headers;

      const auto pos = headers.find(header_name);
      if (pos == headers.end()) {
        return {};
      }

      return (*pos).second;
    }

    auto error_message() const -> string_t {
      if (this->has_response()) {
        return "";
      } else {
        return this->error_to_string();
      }
    }

    template<std::invocable<http_response&&> F>
    auto then(F&& func) && noexcept {
      using ret_then_t = then_impl<http_response, Err>;

      // これnoexceptか・・・？
      return std::visit(overloaded{
        [](http_response&& value) {
          return ret_then_t{ std::move(value)} ;
        },
        [](Err&& err) {
          return ret_then_t{ std::move(err)} ;
        },
      }, std::move(this->m_either)).then(std::forward<F>(func));
    }

    template<std::invocable<Err&&> F>
    auto catch_error(F&& func) && noexcept {
      using ret_then_t = then_impl<http_response, Err>;

      // これnoexceptか・・・？
      return std::visit(overloaded{
        [](http_response&& value) {
          return ret_then_t{ std::move(value)};
        },
        [](Err&& err) {
          return ret_then_t{ std::move(err)};
        },
      }, std::move(this->m_either)).catch_error(std::forward<F>(func));
    }

    /**
     * @brief 結果を文字列として任意の継続処理を実行する
     * @param f std::string_viewを1つ受けて呼び出し可能なもの
     * @details 有効値を保持していない場合（httpアクセスに失敗している場合）、空のstring_viewが渡される
     * @return f(str_view)の戻り値
     */
    template<std::invocable<std::string_view> F>
    friend decltype(auto) operator|(const basic_result& self, F&& f) noexcept(std::is_nothrow_invocable_v<F, std::string_view>) {
      if (self.has_response()) {
        return std::invoke(std::forward<F>(f), self.response_body());
      } else {
        return std::invoke(std::forward<F>(f), std::string_view{});
      }
    }

    // optional/expected的インターフェース

    auto value() & -> http_response& {
      return std::get<0>(m_either);
    }

    auto value() const & -> const http_response& {
      return std::get<0>(m_either);
    }

    auto value() && -> http_response&& {
      return std::get<0>(std::move(m_either));
    }

    auto error() const -> Err {
      return std::get<1>(m_either);
    }
  };
}

namespace chttpp::detail::inline config {

  inline namespace enums {
    enum class http_version {
      http1_1,
      http2,
#if defined(WINHTTP_PROTOCOL_FLAG_HTTP3) || defined(CURL_HTTP_VERSION_3)
      http3,
#endif
    };

    enum class authentication_scheme {
      basic,
      //digest,
    };

    enum class proxy_scheme {
      http,
      https,
      socks4,
      socks4a,
      socks5,
      socks5h,
    };
  }

  struct authorization_config {
    std::string_view username = "";
    std::string_view password = "";
    authentication_scheme scheme = authentication_scheme::basic;
  };

  struct proxy_config {
    std::string_view address = "";
    proxy_scheme scheme = proxy_scheme::http;
    authorization_config auth{};
  };

  struct request_config_for_get {
    vector_t<std::pair<std::string_view, std::string_view>> headers{};
    vector_t<std::pair<std::string_view, std::string_view>> params{};
    http_version version = http_version::http2;
    std::chrono::milliseconds timeout{ 30000 };
    authorization_config auth{};
    proxy_config proxy{};
  };

  struct request_config {
    std::string_view content_type = "";
    vector_t<std::pair<std::string_view, std::string_view>> headers{};
    vector_t<std::pair<std::string_view, std::string_view>> params{};
    http_version version = http_version::http2;
    std::chrono::milliseconds timeout{ 30000 };
    authorization_config auth{};
    proxy_config proxy{};
  };
}

namespace chttpp {
  namespace cfg = chttpp::detail::config::enums;
}

namespace chttpp::detail::tag {
  struct get_t {};
  struct post_t {};
  struct head_t {};
  struct options_t {};
  struct put_t {};
  struct delete_t {};
  struct trace_t {};
  struct patch_t {};

  template<typename Tag>
  concept has_reqbody_method =
    std::same_as<Tag, post_t> or
    std::same_as<Tag, put_t> or
    std::same_as<Tag, delete_t> or
    std::same_as<Tag, patch_t>;
}