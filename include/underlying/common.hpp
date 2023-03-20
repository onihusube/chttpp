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
  template<typename CharT>
  using basic_string_t = std::pmr::basic_string<CharT>;
  using string_t = std::pmr::string;
  using wstring_t = std::pmr::wstring;
  using header_t = std::pmr::unordered_map<string_t, string_t, string_hash, std::ranges::equal_to>;
  template<typename T>
  using vector_t = std::pmr::vector<T>;
#else
  // アロケータカスタイマイズをしない
  template<typename CharT>
  using basic_string_t = std::basic_string<CharT>;
  using string_t = std::string;
  using wstring_t = std::string;
  using header_t = std::unordered_map<string_t, string_t, string_hash, std::ranges::equal_to>;
  template<typename T>
  using vector_t = std::vector<T>;
#endif

}

namespace chttpp::detail::inline util {

  template<typename CharT>
  class basic_string_buffer {
    basic_string_t<CharT> m_buffer;
  public:
    basic_string_buffer() = default;

    basic_string_buffer(basic_string_buffer&&) = default;
    basic_string_buffer& operator=(basic_string_buffer&&) & = default;

    void use(std::invocable<basic_string_t<CharT>&> auto&& fun) & {
      // 空であること
      assert(m_buffer.empty());

      std::invoke(fun, m_buffer);

      // 使用後に空にする
      m_buffer.clear();
    }
  };

  using string_buffer = basic_string_buffer<char>;
  using wstring_buffer = basic_string_buffer<wchar_t>;
}

namespace chttpp::detail::inline concepts {
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

    /**
     * @brief TとEが同じ型になりうる場合に状態を区別して構築する
     * @brief 主にmonostateで構築する時に使用することを想定
     */
    template<typename V, std::size_t I>
    then_impl(std::in_place_index_t<I>, V&& value)
      : outcome{ std::in_place_index<I>, std::forward<V>(value) }
    {}

    then_impl(then_impl&&) = default;
    then_impl& operator=(then_impl &&) = default;

    template<std::invocable<T&&> F>
      requires (not std::same_as<T, std::monostate>)
    auto then(F&& func) && noexcept -> decltype(auto) {
      using ret_t = std::invoke_result_t<F, T&&>;

      // 戻り値型voidかつ、const T&で呼び出し可能な場合
      // std::conjunctionを利用しているのは、短絡評価を行うため（invocable<F, const T&>の評価を遅延させたい）
      if constexpr (std::conjunction_v<std::is_same<void, std::remove_cvref_t<ret_t>>, std::is_invocable<F, const T&>>) {
        try {
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
      } else {
        // T&&から呼び出し可能な場合
        using ret_then_t = then_impl<void_to_monostate<ret_t>, E>;

        try {
          return std::visit(overloaded{
              [&](T&& value) {
                if constexpr (std::same_as<void, std::remove_cvref_t<ret_t>>) {
                  // 戻り値型voidの場合
                  // ムーブして渡すため、後続でのTの使用が安全ではなくなることから、monostateで置換する
                  std::invoke(std::forward<F>(func), std::move(value));
                  return ret_then_t{ std::in_place_index<0>, std::monostate{} };
                } else {
                  return ret_then_t{ std::invoke(std::forward<F>(func), std::move(value))};
                }
              },
              [](E&& err) {
                return ret_then_t{ std::move(err)};
              },
              [](std::exception_ptr&& exptr) {
                return ret_then_t{ std::move(exptr)};
              }
            }, std::move(this->outcome));
        } catch (...) {
          return ret_then_t{ std::current_exception() };
        }
      }

      assert(false);
/*
* この関数をオーバーロードで分けずにif constexprで内部分岐しているのは
* std::invocable<F, const T&>のチェックを必要になるギリギリまで遅延させるため
* オーバーロードで分けてそれをチェックするとhttp_responseを受けてそのまま返すような関数を渡したときにエラーになる
chttpp::get(...).then([](auto&& hr) {
  ut::expect(true);
  return hr;  // コピーが発生しエラー
});
* この場合、std::invocable<F, const T&>のチェックでauto&& -> const http_response&となり、returnでhrがconstのためコピーされるがコピー禁止のためエラーになる
* そして、この場合はSFINAE的な動作はせずにハードエラーとなる
* これを回避するため、関数のインターフェースの制約はhttp_response&&で呼び出し可能であることを要求しておき
* そのうえで、戻り値型がvoidであるときのみstd::invocable<F, const T&>のチェックを行っている
* このようにconst左辺値呼び出し可能性チェックを遅延させることで、上記のような場合に不要なインスタンス化を回避する
*/
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
              return ret_then_t{ std::in_place_index<1>, std::monostate{}};
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

namespace chttpp::detail::inline config {

  inline namespace enums {
    enum class http_version {
      http1_1,
      http2,
      //http3,
    };

    enum class authentication_scheme {
      none,
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
    authentication_scheme scheme = authentication_scheme::none;
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

    operator request_config_for_get() const {
      return request_config_for_get{ .headers = headers, .params = params, .version = version, .timeout = timeout, .auth = auth, .proxy = proxy };
    }
  };

  struct agent_initial_config {
    http_version version = http_version::http2;
    std::chrono::milliseconds timeout{30000};
    authorization_config auth{};
    proxy_config proxy{};
  };

  struct agent_request_config {
    std::string_view content_type = "";
    vector_t<std::pair<std::string_view, std::string_view>> params{};
  };

  struct agent_config {
    // コンストラクタで渡す設定
    agent_initial_config init_cfg;

    // その他のタイミングで渡される設定
    vector_t<std::pair<std::string_view, std::string_view>> headers{};
    //authorization_config auth{};
  };
}

namespace chttpp {
  // 設定用列挙値の簡易アクセス用の名前空間を定義

  // なるべく短く元の列挙型にアクセス
  namespace cfg {
    using namespace chttpp::detail::config::enums;
  }

  // 認証周りの設定
  namespace cfg_auth {
    using enum chttpp::detail::config::enums::authentication_scheme;
  }

  // httpバージョンの設定
  namespace cfg_ver {
    using enum chttpp::detail::config::enums::http_version;
  }

  // プロクシ周りの設定
  namespace cfg_prxy {
    using enum chttpp::detail::config::enums::proxy_scheme;
  }
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