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
#include <deque>
#include <initializer_list>
#include <unordered_set>
#include <spanstream>
#include <iomanip>
#include <ctime>
#include <charconv>
#include <climits>

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
  template<typename Key, typename Value, typename Hash = std::hash<Key>, typename Comp = std::ranges::equal_to>
  using umap_t = std::pmr::unordered_map<Key, Value, Hash, Comp>;
  template<typename Key, typename Hash = std::hash<Key>, typename Comp = std::ranges::equal_to>
  using uset_t = std::pmr::unordered_set<Key, Hash, Comp>;
  template<typename T>
  using deque_t = std::pmr::deque<T>;
#else
  // アロケータカスタイマイズをしない
  template<typename CharT>
  using basic_string_t = std::basic_string<CharT>;
  using string_t = std::string;
  using wstring_t = std::string;
  using header_t = std::unordered_map<string_t, string_t, string_hash, std::ranges::equal_to>;
  template<typename T>
  using vector_t = std::vector<T>;
  template<typename Key, typename Value, typename Hash = std::hash<Key>, typename Comp = std::ranges::equal_to >>
  using umap_t = std::unordered_map<Key, Value, Hash, Comp>;
  template<typename Key, typename Hash = std::hash<Key>, typename Comp = std::ranges::equal_to>
  using uset_t = std::unordered_set<Key, Hash, Comp>;
  template<typename T>
  using deque_t = std::deque<T>;
#endif

}

namespace chttpp::detail::inline util {

  template<typename T>
  class scoped_clear {
    T& buf;

  public:

    scoped_clear(T& r) : buf(r) {}

    scoped_clear(const scoped_clear&) = delete;
    scoped_clear &operator=(const scoped_clear&) = delete;

    ~scoped_clear() {
      buf.clear();
    }
  };

  //template<typename CharT>
  //using str_auto_clear = scoped_clear<basic_string_t<CharT>>;

  template<typename CharT>
  struct str_auto_clear {
    basic_string_t<CharT>& str;

    ~str_auto_clear() {
      str.clear();
    }
  };

  template<typename CharT>
  class basic_string_buffer {
    basic_string_t<CharT> m_buffer;
  public:
    basic_string_buffer() = default;

    basic_string_buffer(basic_string_buffer&&) = default;
    basic_string_buffer& operator=(basic_string_buffer&&) & = default;

    auto use(std::invocable<basic_string_t<CharT>&> auto&& fun) & {
      // 空であること
      assert(m_buffer.empty());

      // 使用後に空にする
      [[maybe_unused]]
      str_auto_clear raii{ m_buffer };

      return std::invoke(fun, m_buffer);
    }

    template<typename... CharTs>
    friend auto use_multiple_buffer(std::invocable<basic_string_t<CharTs>&...> auto&& fun, basic_string_buffer<CharTs>&... buffers);
  };

  using string_buffer = basic_string_buffer<char>;
  using wstring_buffer = basic_string_buffer<wchar_t>;

  template<typename F, typename... CharTs>
  auto use_multiple_buffer_impl(F&& fun, str_auto_clear<CharTs>... buffers) {
    return std::invoke(fun, buffers.str...);
  }

  template<typename... CharTs>
  auto use_multiple_buffer(std::invocable<basic_string_t<CharTs>&...> auto&& fun, basic_string_buffer<CharTs>&... buffers) {
    // 空であること
    assert((buffers.m_buffer.empty() && ...));

    // str_auto_clearで文字列バッファをラップして自動クリーンを行う（その実装の）ために、この関数を経由する必要がある
    return use_multiple_buffer_impl(fun, str_auto_clear{ buffers.m_buffer }...);
  }


  template<typename T>
    requires requires (T& t) {
      t.clear();
      {t.empty()} -> std::convertible_to<bool>;
    }
  class pinned_buffer {
    T m_buffer;

  public:
    pinned_buffer() = default;

    pinned_buffer(pinned_buffer &&) = default;

    // 代入操作は意味なさそうに思える
    pinned_buffer &operator=(pinned_buffer &&) = delete;

    [[nodiscard]]
    auto pin(std::invocable<T&> auto&& func) & {
      using func_ret_t = std::remove_cvref_t<decltype(std::invoke(func, m_buffer))>;
      using ret_pair = std::pair<func_ret_t, scoped_clear<T>>;

      // 空であるはず
      assert(m_buffer.empty());

      if constexpr (std::is_same_v<func_ret_t, void>) {
        std::invoke(func, m_buffer);
        return scoped_clear<T>{m_buffer};
      } else {
        return ret_pair(std::piecewise_construct, std::forward_as_tuple(std::invoke(func, m_buffer)), std::tie(m_buffer));
      }
    }
  };

  template <typename T>
  using vector_buffer = pinned_buffer<vector_t<T>>;
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

    const bool is_set_cookie = key_str == "set-cookie";

    if (const auto [it, inserted] = headers.emplace(std::move(key_str), std::string_view{ header_value_pos, header_end_pos }); not inserted) {
      // ヘッダ要素が重複している場合、値をカンマ区切りリストによって追記する
      // 詳細 : https://this.aereal.org/entry/2017/12/21/190158

      auto& header_value = (*it).second;
      
      // ヘッダ要素を追加する分の領域を拡張
      header_value.reserve(header_value.length() + std::size_t(std::ranges::distance(header_value_pos, header_end_pos)) + 2);
      if (is_set_cookie) {
        // クッキーの分割は"; "によって行う（後での分離しやすさのため）
        header_value.append("; ");
      } else {
        header_value.append(", ");
      }
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

namespace chttpp::detail {

  class url_info {
    string_t m_urlstr;
    
    bool m_is_https = true;
    bool m_is_ipv4_host = false;
    bool m_is_ipv6_host = false;

    // ホスト部の開始位置
    std::size_t m_host_begin_pos;
    // パス部分の開始位置、必ず'/'で始まる
    std::size_t m_path_begin_pos;

    bool parse_host_name() {
      // memo : https://tex2e.github.io/rfc-translater/html/rfc3986.html

      if (m_urlstr.empty()) {
        return false;
      }

      // 後からパスが追加されるので、ある程度確保しておく
      m_urlstr.reserve(m_urlstr.length() + 15u);

      // パース注目位置、先頭から順番に見ていく
      std::size_t pos = 0;

      // 想定するURLは次の3タイプ（認証情報やポート番号はあってもいい）
      // http(s)://example.com/...
      // example.com/...
      // example.com

      // httpから始まっているか
      if (m_urlstr.starts_with("http")) {
        pos = 4;
        // httpsであるか
        if (m_urlstr[pos] != 's') {
          m_is_https = false;
        } else {
          // sを消費
          ++pos;
        }

        // '://'のパース
        if (m_urlstr[pos] == ':') {
          ++pos;
          if (m_urlstr[pos] == '/' and m_urlstr[pos + 1] == '/') {
            pos += 2;
          } else {
            // ':/example.com'とか':example.com'みたいなの
            return false;
          }
        } else {
          // 'http(s)'の後に':'が続かない
          return false;
        }
      } else {
        // http始まりではない時、httpsを仮定
        m_is_https = true;
      }

      // Authority（ホスト部を含むURLの部分）のパース
      m_host_begin_pos = pos;

      // Authorityの終わりを見つける
      auto slash_pos = m_urlstr.find_first_of("/#?", pos);

      if (slash_pos != string_t::npos) {
        if (m_urlstr[slash_pos] != '/') {
          // Authorityの終了が/ではない場合、パスはない
          // #?以降を消して、/を置いておく
          m_urlstr.resize(slash_pos);
          m_urlstr += '/';
          m_path_begin_pos = m_urlstr.length() - 1;
          slash_pos = m_path_begin_pos;
        } else {
          m_path_begin_pos = slash_pos;
        }
      } else {
        // 終わりの'/'がない時、'/'を加えておく
        m_urlstr += '/';
        m_path_begin_pos = m_urlstr.length() - 1;
        slash_pos = m_path_begin_pos;
      }

      // userinfo（認証情報）の探索
      // 例えば、http://name:pass@example.com/ みたいな感じ

      const auto authority_part = std::string_view{m_urlstr}.substr(pos, slash_pos - pos);

      // Authority部先頭文字のチェック（最低限の検証）
      if (not authority_part.empty()) {
        const auto c = authority_part[0];

        do {
          // アルファベットと数字は当然
          if (std::isalnum(static_cast<unsigned char>(c)) != 0) break;
          // ハイフン始まりはあり得る（？
          if (c == '-') break;
          // IPv6アドレスホストの囲いとして現れうる
          if (c == '[') break;

          // それ以外の文字は現れない
          return false;
        } while (false);
      } else {
        // ホスト部空はありえない
        return false;
      }

      // Authority内部でのhost部の開始位置
      std::size_t host_begin = 0;
      
      if (auto atpos = authority_part.find_first_of('@'); atpos != std::string_view::npos) {
        // userinfoがある場合、ホスト部は@の次から
        m_host_begin_pos += atpos + 1;
        host_begin = atpos + 1;
      }

      // hostがIPアドレスであるかを判定
      // IPv4の場合 : http://172.0.0.1/
      // IPv6の場合 : http://[::1]/

      const auto host_part = authority_part.substr(host_begin);

      // http://user:pass@/ みたいなの？
      if (host_part.empty()) {
        return false;
      }

      if (host_part[0] == '[') {
        // IP-literal すなわちIPv6アドレス指定と思われる
        m_is_ipv6_host = true;

        // 一応続く]をチェック
        // IPv6アドレスの最短は'::'の2文字（全部0）
        // 次点が'::1'の3文字（ループバックアドレス）
        if (host_part.find(']', 3) == std::string_view::npos) {
          // 閉じの]が見つからないのはおかしい
          return false;
        }
      } else {
        // IPv4をチェック
        using namespace std::views;

        // ポートの前の文字列を取り出して、その前半部分をチェック
        for (auto exclude_port : host_part | split(':') 
                                           | take(1))
        {
          // IPパートのカウント
          unsigned int count = 0;

          for (const auto part : exclude_port | split('.')
                                              | take(5)
                                              | transform([](auto&& inner_rng) { return std::string_view{inner_rng}; }))
          {
            // unsigned char（0~255）を使用することで、値チェックを省略する
            static_assert(CHAR_BIT == 8);
            unsigned char n;

            if (auto [ptr, ec] = std::from_chars(std::to_address(part.begin()), std::to_address(part.end()), n); ec == std::errc{}) {
              ++count;
              continue;
            } else {
              // IPv4ではなかった
              break;
            }
          }

          if (count == 4) {
            m_is_ipv4_host = true;
          } else if (4 < count) {
            // .区切りが5回以上続くものはURLではない
            m_is_ipv4_host = true;
            return false;
          }
        }
      }

      // 両方同時にtrueになることはない
      assert((m_is_ipv4_host && m_is_ipv6_host) == false);

      return true;
    }

  public:

    [[nodiscard]]
    url_info(std::string_view url_head)
      : m_urlstr(url_head) 
    {
      if (this->parse_host_name() == false) {
        // パースエラー
        m_path_begin_pos = string_t::npos;
        m_host_begin_pos = m_path_begin_pos - 1;
      }

      // 事後条件
      assert(m_host_begin_pos < m_path_begin_pos);
    }

    [[nodiscard]]
    bool is_valid() const noexcept {
      return m_path_begin_pos != string_t::npos;
    }

    [[nodiscard]]
    auto host() const & noexcept -> std::string_view {
      if (not is_valid()) {
        // パースエラーが起きている場合
        return {};
      }

      const auto p = m_urlstr.data();

      return std::string_view(p + m_host_begin_pos, p + m_path_begin_pos);
    }

    [[nodiscard]]
    auto request_path() const & noexcept -> std::string_view {
      if (not is_valid()) {
        // パースエラーが起きている場合
        return {};
      }

      assert(m_urlstr[m_path_begin_pos] == '/');

      return std::string_view(m_urlstr).substr(m_path_begin_pos);
    }

    [[nodiscard]]
    bool secure() const noexcept {
      return m_is_https;
    }

    [[nodiscard]]
    bool is_ip_host() const noexcept {
      return m_is_ipv4_host or m_is_ipv6_host;
    }

    [[nodiscard]]
    bool is_ipv4_host() const noexcept {
      return m_is_ipv4_host;
    }

    [[nodiscard]]
    bool is_ipv6_host() const noexcept {
      return m_is_ipv6_host;
    }

    [[nodiscard]]
    auto append_path(std::string_view path) & {
      class raii {
        string_t& str;
        std::size_t pos;

      public:
        raii(string_t& urlstr, std::size_t org_len)
          : str{urlstr}
          , pos{org_len}
        {
          assert(pos <= str.length());
        }

        raii(const raii&) = delete;
        raii& operator=(const raii&) = delete;
        raii(raii&&) = delete;
        raii& operator=(raii&&) = delete;

        ~raii() {
          if (pos != string_t::npos) {
            str.resize(pos);
          }
        }
      };

      if (not is_valid()) {
        // 先頭URLのパースでエラーが起きている場合は何もしない
        return raii{m_urlstr, string_t::npos};
      }

      // 元の長さ
      const auto org_len = m_urlstr.length();

      // クエリ('?')やアンカー（'#'）は除く
      path = path.substr(0, path.find_first_of("#?"));

      if (m_urlstr.back() == '/' and path.front() == '/') {
        // '/'が重複する場合は取り除いておく
        m_urlstr.append(path.substr(1));
      } else {
        m_urlstr.append(path);
      }

      // RAIIによって、後で元の長さに切り詰めることで元のURL先頭部分を復帰する
      return raii{m_urlstr, org_len};
    }
  };
}

namespace chttpp::detail::inline cookie_related {
  // memo : https://tex2e.github.io/rfc-translater/html/rfc6265.html

  struct cookie {
    // 必須
    string_t name;
    string_t value;

    // オプション
    string_t domain = "";
    string_t path = "/";
    bool secure = false;

    std::chrono::system_clock::time_point expires = std::chrono::system_clock::time_point::max();

    // このライブラリの用法ではあっても無意味と思われる
    //bool http_only = false;
    //string_t SameSite = "Strict";

    // クッキーの作成時刻基本的に触らない
    std::chrono::system_clock::time_point create_time = std::chrono::system_clock::now();

    // クッキーの等価性は、name,domain,pathの一致によって判定される
    friend bool operator==(const cookie& self, const cookie& other) noexcept(noexcept(std::string{} == std::string{})) {
      return self.name   == other.name && 
             self.domain == other.domain && 
             self.path   == other.path;
    }
  };

  class cookie_ref {
    std::string_view m_name;
    std::size_t m_path_length;
    std::chrono::system_clock::time_point m_create_time;

    std::string_view m_value;

  public:

    cookie_ref(const cookie& c)
      : m_name{c.name}
      , m_path_length{c.path.length()}
      , m_create_time{c.create_time}
      , m_value{c.value}
    {}

    cookie_ref(const std::pair<std::string_view, std::string_view>& c, std::string_view path = "/")
      : m_name{c.first}
      , m_path_length{path.length()}
      , m_create_time{std::chrono::system_clock::time_point::max()} // つまり、パスが同じ長さならより後ろに並べる
      , m_value{c.second}
    {}

    auto name() const noexcept -> std::string_view {
      return m_name;
    }

    auto value() const noexcept -> std::string_view {
      return m_value;
    }

    friend auto operator<=>(const cookie_ref& lhs, const cookie_ref& rhs) {
      // memo : https://qiita.com/y_tochukaso/items/a4e815674be61ca68707
      // クッキー名 -> Path -> 作成時刻 の順で比較
      // 昇順に並んだ時にクッキー送信順になるようにする

      // 名前はそのまま（lhs < rhs）
      if (auto cmp = lhs.m_name <=> rhs.m_name; cmp != 0) {
        return cmp;
      }
      // パスは長い方が前（lhs > rhs）、他と逆
      if (auto cmp = rhs.m_path_length <=> lhs.m_path_length; cmp != 0) {
        return cmp;
      }
      // 作成日時は早い方が前（lhs < rhs）
      return lhs.m_create_time <=> rhs.m_create_time;
    }

    friend bool operator==(const cookie_ref& lhs, const cookie_ref& rhs) {
      return lhs.m_name == rhs.m_name &&
             lhs.m_path_length == rhs.m_path_length &&
             lhs.m_create_time == rhs.m_create_time;
    }

    friend void swap(cookie_ref& lhs, cookie_ref& rhs) {
      using std::ranges::swap;

      swap(lhs.m_name, rhs.m_name);
      swap(lhs.m_path_length, rhs.m_path_length);
      swap(lhs.m_create_time, rhs.m_create_time);
      swap(lhs.m_value, rhs.m_value);
    }
  };

  struct cookie_hash {
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;

    /***
     * @cite https://stackoverflow.com/questions/8513911/how-to-create-a-good-hash-combine-with-64-bit-output-inspired-by-boosthash-co
     */
    static inline void hash_combine(std::size_t& seed, std::size_t hash) {
      constexpr std::size_t kMul = 0x9ddfea08eb382d69ULL;
      std::size_t a = (hash ^ seed) * kMul;
      a ^= (a >> 47);
      std::size_t b = (seed ^ a) * kMul;
      b ^= (b >> 47);
      seed = b * kMul;
    }

    std::size_t operator()(const cookie& c) const noexcept(noexcept(hash_type{}(c.name))) {
      hash_type hasher{};

      std::size_t hash = hasher(c.name);
      hash_combine(hash, hasher(c.domain));
      hash_combine(hash, hasher(c.path));

      return hash;
    }
  };

  class cookie_store : private uset_t<cookie, cookie_hash> {
    using base = uset_t<cookie, cookie_hash>;

  public:

    cookie_store(cookie_store&&) = default;
    cookie_store& operator=(cookie_store&&) = default;

    cookie_store(const cookie_store&) = delete;
    cookie_store& operator=(const cookie_store&) = delete;

    using base::unordered_set;
    
    using base::begin;
    using base::cbegin;
    using base::end;
    using base::cend;
    using base::size;
    using base::empty;
    
    using base::find;
    using base::contains;
    using base::insert;
    using base::merge;
    using base::extract;
    using base::erase;
    using base::clear;

    using base::get_allocator;

    void merge(cookie_store& other) & {
      this->merge(static_cast<base&>(other));
    }

    friend auto erase_if(cookie_store& self, auto pred)
      requires requires(base& container) {
        std::erase_if(container, pred);
      }
    {
      return std::erase_if(self, pred);
    }

    auto remove_expired_cookies() & {
      const auto nowtime = std::chrono::system_clock::now();

      return std::erase_if(*this, [nowtime](const auto &c) {
        return c.expires < nowtime;
      });
    }

    template <typename Out, std::ranges::input_range R>
      requires std::constructible_from<cookie_ref, std::ranges::range_reference_t<R>> and
               std::is_same_v<cookie_ref, std::ranges::range_value_t<Out>> and
               requires(Out& c, cookie_ref&& cookie) {
                 c.push_back(std::move(cookie));
                 std::ranges::sort(c);
               }
    void sort_by_send_order_to(Out& store, R& additional_cookies) const & {
      
      if constexpr (std::ranges::sized_range<R> and requires { store.reserve(1ull); }) {
        store.reserve(this->size() + std::ranges::size(additional_cookies));
      }

      constexpr auto to_cookie_ref = [](const auto& c) { return cookie_ref{c}; };

      std::ranges::copy(*this | std::views::transform(to_cookie_ref), std::back_inserter(store));
      std::ranges::copy(additional_cookies | std::views::transform(to_cookie_ref), std::back_inserter(store));

      // 同名ならPathが長い方が先、同じ長さなら作成時間が早い方が先、になるようにソート
      std::ranges::sort(store);
    }

    template <typename Out, std::ranges::input_range R>
      requires std::constructible_from<cookie_ref, std::ranges::range_reference_t<R>> and
               std::is_same_v<cookie_ref, std::ranges::range_value_t<Out>> and
               requires(Out& c, cookie_ref&& cookie) {
                 c.push_back(std::move(cookie));
                 std::ranges::sort(c);
               }
    void create_cookie_list_to(Out& store, R& additional_cookies, url_info& urlinfo) const & {

      assert(urlinfo.is_valid());
      
      if constexpr (std::ranges::sized_range<R> and requires { store.reserve(1ull); }) {
        store.reserve(this->size() + std::ranges::size(additional_cookies));
      }

      // ドメイン文字列は正規化されている
      // すくなくとも全て小文字らしい

      // パス文字列は'/'で始まる、少なくとも1文字
      // クエリ文字列やアンカーを含まない
      {
        const auto path = urlinfo.request_path();
        
        assert(not path.empty());
        assert(path.front() == '/');
        assert(path.find('?') == std::string_view::npos);
        assert(path.find('#') == std::string_view::npos);
      }

      // パスとドメインをマッチングし送信すべきクッキーを弁別する
      auto cookie_filter = [&urlinfo = urlinfo](const detail::cookie& c) -> bool {
        // Secure属性があるクッキーはhttpsでのみ送信
        if (c.secure) {
          if (not urlinfo.secure()) return false;
        }

        const auto domain_str = urlinfo.host();

        // ドメインのマッチング
        // クッキードメイン aaa.example.com に対しては
        // ドメイン文字列（ホスト名） example.com, aaa.example.com はマッチするが
        // bbb.aaa.example.com, a.example.com はマッチしない
        do {
          // 例外対応として、ドメインが空ならば一致しているものとして扱う
          // agent::set_cookiesの都合（必ずしもdomainが指定されるとは限らない）
          if (c.domain.empty()) break;

          // ドメインはクッキードメインのサフィックスになっていなければならない
          if (c.domain.ends_with(domain_str) == false) return false;
          
          // ドメインが完全一致している
          if (c.domain.length() == domain_str.length()) break;

          // サフィックスの手前の位置
          const std::size_t sufix_pos = c.domain.length() - domain_str.length() - 1;
          // サフィックスとなっていない文字列の最後（サフィックスの直前）は`.`
          if (c.domain[sufix_pos] == '.') {
            // ホスト名である（IPアドレスではない）
            if (urlinfo.is_ip_host() == false) {
              break;
            }
          }

          // マッチしない
          return false;
        } while (false);

        const auto request_path = urlinfo.request_path();
        // パスのマッチング
        // 少なくとも、クッキーパスはリクエストパスのプリフィックスになっていなければならない
        // /abc/defというリクエスパスに対しては
        // /, /abc/, /abc というクッキーパスはマッチするが、/abc/def/ghi, /abcdefはマッチしない
        if (request_path.starts_with(c.path)) {
          // パスの完全一致
          if (c.path.length() == request_path.length()) return true;

          // プリフィックスであり、クッキーパスの最後の文字が'/'でおわる
          if (c.path.back() == '/') return true;

          // クッキーパスに含まれないリクエストパスの最初の文字は'/'
          if (request_path[c.path.length()] == '/') return true;
        }

        return false;
      };

      // agent本体登録のものとリクエスト時の一時的なものとを統一的に扱い、またソートするためのクッキー参照へ変換する
      constexpr auto to_cookie_ref = [](const auto& c) { return cookie_ref{c}; };

      std::ranges::copy(*this | std::views::filter(cookie_filter) | std::views::transform(to_cookie_ref), std::back_inserter(store));
      // リクエスト時クッキーは、同じドメインかつSecure等の属性も合致しているものと仮定
      std::ranges::copy(additional_cookies | std::views::transform(to_cookie_ref), std::back_inserter(store));

      // 同名ならPathが長い方が先、同じ長さなら作成時間が早い方が先、になるようにソート
      std::ranges::sort(store);
    }

    void insert_from_set_cookie(std::string_view set_cookie_str, std::string_view host = "") {
      // memo : https://triple-underscore.github.io/http-cookie-ja.html#sane-set-cookie
      // memo : https://qiita.com/sekai/items/489378d60267cc85fd34

      using namespace std::views;
      using std::forward_iterator;
      using std::ranges::begin;
      using std::ranges::end;
      using std::ranges::find;
      using std::ranges::size;
      using std::ranges::subrange;

      constexpr std::string_view semicolon = ";";
      constexpr std::string_view equal = "=";
      constexpr std::string_view attribute_names[] = {
          "Expires",  // 7
          "Max-Age",  // 7
          "Domain",   // 6
          "Secure",   // 6
          "Path",     // 4
          "HttpOnly", // 8
          "SameSite", // 8
      };

      enum class attribute {
        NotAttribute = -1,
        Expires,
        MaxAge,
        Domain,
        Secure,
        Path,
        HttpOnly,
        SameSite,
      };

      auto classify_attribute = [&attribute_names](std::string_view name) -> attribute {
        switch (name.length())
        {
        case 7:
          if (name == attribute_names[0]) return attribute::Expires;  // Expires
          if (name == attribute_names[1]) return attribute::MaxAge;   // Max-Age
          return attribute::NotAttribute;
        case 6:
          if (name == attribute_names[2]) return attribute::Domain; // Domain
          if (name == attribute_names[3]) return attribute::Secure; // Secure
          return attribute::NotAttribute;
        case 4:
          if (name == attribute_names[4]) return attribute::Path; // Path
          return attribute::NotAttribute;
        case 8:
          if (name == attribute_names[5]) return attribute::HttpOnly; // HttpOnly
          if (name == attribute_names[6]) return attribute::SameSite; // SameSite
          return attribute::NotAttribute;
        default:
          return attribute::NotAttribute;
        }
      };

      constexpr auto is_attribute = [](attribute attr) -> bool {
        return attr != attribute::NotAttribute;
      };

      // 先頭ホワイトスペースのスキップ
      // split結果の要素であるsubrangeを受けて、drop_while_viewを返す
      constexpr auto skip_leading_ws = [](auto&& r) {
        return std::move(r) | drop_while([](char c) -> bool { return c == ' '; });
      };

      // split結果をstring_viewにする、ついでに末尾ホワイトスペースを除く
      constexpr auto to_string_view = [](auto&& subrng) -> std::string_view {
        std::string_view str_view = {begin(subrng), end(subrng)};
        
        if (const auto pos = str_view.find_last_not_of(' '); pos == std::string_view::npos) {
          return str_view;
        } else {
          // 末尾のホワイトスペースを除去
          return str_view.substr(0, pos + 1);
        }

      };

      // '='で分割し、先頭と末尾のホワイトスペースを無視するアダプタ
      constexpr auto spliteq = split(equal) | transform(skip_leading_ws) | transform(to_string_view);

      // 取得時刻
      const auto now_time = std::chrono::system_clock::now();

      // Expiresの日付を変換する
      auto parse_expires = [now_time](std::string_view str) -> std::chrono::system_clock::time_point {
        std::ispanstream ss{str};

  #if 201907L <= __cpp_lib_chrono
        using namespace std::chrono;

        system_clock::time_point time;
        ss >> parse("%a, %d %b %Y %H:%M:%S %Z", time);
  #else
        std::tm tm{};
        // MSVCだと失敗する
        ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S %Z");
        const auto time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
  #endif

        if (ss.fail()) {
          // 失敗は現在時刻を返すことで、すぐに削除されるようにする
          return now_time;
        }

        return time;
      };


      // まず';'で分割
      auto primary_part = set_cookie_str | split(semicolon);

      forward_iterator auto primary_it = begin(primary_part);
      const auto primary_end = end(primary_part);

      // primary_itの進行を制御するために、外側のループはwhileとする
      while(primary_it != primary_end) {
        // さらに'='で分割
        auto cookie_pair = *primary_it | spliteq;

        // この時点で現在の要素には興味がないので次に進めておく
        // forward_iteratorであるため、cookie_pairが無効になることはない
        ++primary_it;

        // "; "みたいなのが入ってる時（spliteqへの入力文字範囲が空となる時）
        if (cookie_pair.empty()) {
          continue;
        }

        // 長くても2要素
        auto it = begin(cookie_pair);

        // クッキー名の抽出
        std::string_view name = *it;

        // 名前は必須
        if (name.empty()) {
          continue;
        }

        // 属性名ではないことを確認
        if (is_attribute(classify_attribute(name))) {
          // クッキー本体がないことになる
          // 次の本体が現れるまで読み飛ばす
          continue;
        }

        ++it;

        if (it == cookie_pair.end()) {
          // `name; `のように=を含まないクッキー本体は無視する
          // =を含んでいれば、たとえ値がなくてもcookie_pairは2要素のrangeとなる
          continue;
        }

        // クッキー値は空でも良い
        cookie tmp_cookie{ .name = string_t{name}, .value = string_t{*it}, .domain = string_t{host} };

        // 属性の読み取りループ、次のクッキー本体が現れるまで
        for (; primary_it != primary_end; ++primary_it) {

          auto secondary_part = *primary_it | spliteq;

          // "; "みたいなのが混ざってる時（spliteqへの入力文字範囲が空となる時）
          if (secondary_part.empty()) {
            continue;
          }

  #ifdef _MSC_VER
  #pragma warning(push)
  #pragma warning(disable:4456)
  #endif
          // 外側の名前をわざとシャドウイングする
          // 型レベルでは比較等が通るので、名前を変えて取り違えるとバグの元になる
          auto it = begin(secondary_part);
  #ifdef _MSC_VER
  #pragma warning(pop)
  #endif

          // 属性名の確認
          const auto attr = classify_attribute(*it);
          if (not is_attribute(attr)) {
            // 次のクッキー本体が現れた
            // primary_itは次のクッキー本体を指した状態でこのループを抜ける
            break;
          }

          // 属性値抽出
          switch (attr)
          {
          case attribute::Expires:
            // Expiresのパース
            ++it;
            if (it != secondary_part.end()) {
              tmp_cookie.expires = parse_expires(*it);
            } else {
              tmp_cookie.expires = now_time;
            }
            break;
          case attribute::MaxAge:
            // Max-Ageの計算
            // まずは取得時刻を入れる
            tmp_cookie.expires = now_time;
            ++it;
            if (it != secondary_part.end()) {
              const auto str = *it;
              // Max-Ageは符号無し
              std::size_t age;
              if (auto [ptr, ec] = std::from_chars(std::to_address(str.begin()), std::to_address(str.end()), age); ec == std::errc{}) {
                tmp_cookie.expires += std::chrono::seconds{age};
                break;
              }
            }
            // 指定なしや変換失敗はそのまま（取得時刻）にする
            break;
          case attribute::Domain:
            ++it;
            if (it != secondary_part.end()) {
              tmp_cookie.domain = string_t{*it};
            }
            break;
          case attribute::Secure:
            tmp_cookie.secure = true;
            break;
          case attribute::Path:
            ++it;
            if (it != secondary_part.end()) {
              tmp_cookie.path = string_t{*it};
            }
            break;
          // HttpOnly SameSite は読み飛ばし
          case attribute::HttpOnly: [[fallthrough]];
          case attribute::SameSite: continue;
          default:
            // ここにはこない
            assert(false);
          }
        }

        // 抽出したクッキーの保存
        if (auto pos = this->find(tmp_cookie); pos != this->end()) {
          // 上書きするためにノードハンドルを取り出す
          auto nh = this->extract(pos);

          // name domain path の3つは一致しているので触らない
          nh.value().value = std::move(tmp_cookie.value);
          nh.value().expires = tmp_cookie.expires;
          nh.value().secure = tmp_cookie.secure;

          // ノードハンドルを戻す
          this->insert(std::move(nh));
        } else {
          // 新規挿入
          this->insert(std::move(tmp_cookie));
        }
      }
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

#define common_request_config \
    vector_t<std::pair<std::string_view, std::string_view>> headers{}; \
    vector_t<std::pair<std::string_view, std::string_view>> params{}; \
    http_version version = http_version::http2; \
    std::chrono::milliseconds timeout{ 30000 }; \
    authorization_config auth{}; \
    proxy_config proxy{}

  struct request_config_for_get {
    common_request_config;
  };

  struct request_config {
    std::string_view content_type = "";
    common_request_config;
  };

#undef common_request_config

  struct agent_initial_config {
    http_version version = http_version::http2;
    std::chrono::milliseconds timeout{30000};
    proxy_config proxy{};
  };

  struct agent_request_config {
    std::string_view content_type = "";
    // MSVCがこの2つに初期化子`{}`を付けるとC2797エラーになるので、なしにする
#ifdef _MSC_VER
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers;
    std::initializer_list<std::pair<std::string_view, std::string_view>> cookies;
#else
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers{};
    std::initializer_list<std::pair<std::string_view, std::string_view>> cookies{};
#endif
    vector_t<std::pair<std::string_view, std::string_view>> params{};
    authorization_config auth{};
  };

  struct agent_config {
    // コンストラクタで渡す設定
    agent_initial_config init_cfg;

    // その他のタイミングで渡される設定
    umap_t<string_t, string_t> headers{};
    cookie_store cookie_vault{};
  };

  enum class toggle_enum : bool {
    enable = true,
    disable = false
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

  // 設定の有効/無効を切り替える値
  using enum detail::toggle_enum;
}

namespace chttpp::detail {

  // これのパクリです・・・
  // https://github.com/Reputeless/YesNo
  template<typename Tag>
  class [[nodiscard]] toggle {
    bool m_value;

    struct helper {
      bool b;
    };

  public:

    //explicit toggle() = default;

    explicit constexpr toggle(bool conf) noexcept
      : m_value{conf}
    {}

    constexpr toggle(helper conf) noexcept
      : m_value{conf.b}
    {}

    toggle& operator=(const toggle&) & = default;

    constexpr explicit operator bool() const noexcept {
      return m_value;
    }

    constexpr bool enabled() const noexcept {
      return m_value == true;
    }

    friend auto operator<=>(toggle, toggle) noexcept = default;

    static constexpr helper enable{true};
    static constexpr helper disable{false};
  };
}

namespace chttpp::inline cfg_agent {

  // クッキー管理を行うかどうか
  using cookie_management = chttpp::detail::toggle<struct cookie_management_tag>;

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

  template <typename Tag>
  concept has_reqbody_method =
      requires {
        requires std::is_same_v<Tag, post_t> or
                 std::is_same_v<Tag, put_t> or
                 std::is_same_v<Tag, delete_t> or
                 std::is_same_v<Tag, patch_t>;
      };
}