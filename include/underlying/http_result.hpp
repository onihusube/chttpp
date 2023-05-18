#pragma once

#include <variant>
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <ranges>
#include <algorithm>
#include <cctype>
#include <cassert>
#include <source_location>

#include "common.hpp"
#include "status_code.hpp"

namespace chttpp::detail {

  template<typename...>
  struct check_handler_impl;

  template<>
  struct check_handler_impl<> {
    static void call_handler(auto&, const std::exception_ptr& exptr) {
      std::rethrow_exception(exptr);
    }
  };

  template<typename Head, typename... Args>
  struct check_handler_impl<Head, Args...> {

    template<typename F>
    static void call_handler(F& handler, const std::exception_ptr& exptr) {
      if constexpr (std::invocable<F&, Head>) {
        try {
          check_handler_impl<Args...>::call_handler(handler, exptr);
        } catch(Head ex) {
          // Headを受けられるならば、catchを追加
          std::invoke(handler, ex);
        }
      } else {
        check_handler_impl<Args...>::call_handler(handler, exptr);
      }
    }

  };

  class exptr_wrapper {
    std::exception_ptr m_exptr;

    template<typename... T>
    bool visit_impl(auto& handler) const & {
      try {
        check_handler_impl<
          T...,
          const std::exception&,
          const std::runtime_error&,
          const std::logic_error&,
          const std::string&,
          const std::wstring&,
          const char*,
          const wchar_t*,
          signed long long,
          unsigned long long,
          signed long,
          unsigned long,
          signed int,
          unsigned int,
          signed short,
          unsigned short,
          signed char,
          unsigned char,
          bool
        >::call_handler(handler, m_exptr);
      } catch(...) {
        // なんもしない（でいい？
        return false;
      }

      return true;
    }

  public:
    exptr_wrapper()
      : m_exptr{std::current_exception()}
    {
      assert(bool(m_exptr));
    }

    exptr_wrapper(exptr_wrapper&&) = default;
    exptr_wrapper& operator=(exptr_wrapper&&) & = default;
    //exptr_wrapper(const exptr_wrapper&) = default;
    //exptr_wrapper& operator=(const exptr_wrapper&) & = default;

    [[noreturn]]
    void rethrow() const {
      assert(bool(m_exptr));
      std::rethrow_exception(m_exptr);
    }

    template<typename T>
    friend bool visit(std::in_place_type_t<T>, std::invocable<T> auto&& handler, const exptr_wrapper& self) {
      assert(bool(self.m_exptr));
      return self.visit_impl<T>(handler);
    }

    friend bool visit(auto&& handler, const exptr_wrapper& self) {
      assert(bool(self.m_exptr));
      return self.visit_impl<>(handler);
    }
  };

  template<typename F>
  struct exception_handler : F {
    using F::operator();
  };

  template<typename F>
  exception_handler(F&&) -> exception_handler<F>;
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

  template <template<class> typename S, typename T>
  inline constexpr bool is_specialization_v = false;

  template <template<class> typename S, typename T>
  inline constexpr bool is_specialization_v<S, S<T>> = true;

  /**
   * @brief `T`が`S`の特殊化であることを表すコンセプト
   */
  template <typename T, template <class> typename S>
  concept specialization_of = is_specialization_v<S, std::remove_cvref_t<T>>;

  /**
   * @brief `T`がvoidならmonostateに変換する
   */
  template <typename T>
  using void_to_monostate = std::conditional_t<std::same_as<std::remove_cvref_t<T>, void>, std::monostate, std::remove_cvref_t<T>>;

  /**
   * @brief catch節のなかで、exptr_wrapperを構築する際に指定するタグ型
   */
  struct from_exception_ptr_t { explicit from_exception_ptr_t() = default; };

  inline constexpr from_exception_ptr_t from_exception_ptr{};


  template<typename T, typename E>
    requires requires {
      // 短絡評価によって、右辺の制約を省略してる
      requires (not std::same_as<T, E>) || std::same_as<T, std::monostate>;
    }
  struct then_base {
  protected:
    std::variant<T, E, exptr_wrapper> m_outcome;
    using V = std::variant<T, E, exptr_wrapper>;
  
  public:

    then_base(T&& value)
      : m_outcome{std::in_place_index<0>, std::move(value)}
    {}

    then_base(E&& err) requires (not std::same_as<T, E>)
      : m_outcome{std::in_place_index<1>, std::move(err)}
    {}

    then_base(from_exception_ptr_t)
      : m_outcome{std::in_place_index<2>}
    {}

    /**
     * @brief TとEが同じ型になりうる場合に状態を区別して構築する
     * @brief 主にmonostateで構築する時に使用することを想定
     */
    template<typename V, std::size_t I>
    then_base(std::in_place_index_t<I>, V&& value)
      : m_outcome{ std::in_place_index<I>, std::forward<V>(value) }
    {}

    then_base(then_base&&) = default;
    then_base& operator=(then_base &&) = default;

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
            }, this->m_outcome);

          // 普通にリターンすると左辺値が帰り、暗黙にコピーが起きる
          return std::move(*this);
        } catch (...) {
          // 実質、Tを保持している場合にのみここに来るはず
          this->m_outcome.template emplace<2>();
          return std::move(*this);
        }
      } else {
        // T&&から呼び出し可能な場合
        using ret_then_t = then_base<void_to_monostate<ret_t>, E>;

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
              [](exptr_wrapper&& exptr) {
                return ret_then_t{ std::in_place_index<2>, std::move(exptr)};
              }
            }, std::move(this->m_outcome));
        } catch (...) {
          return ret_then_t{ from_exception_ptr };
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
    auto catch_error(F&& func) && noexcept -> then_base<T, void_to_monostate<std::invoke_result_t<F, E&&>>> try {
      using ret_t = std::invoke_result_t<F, E&&>;
      using ret_then_t = then_base<T, void_to_monostate<ret_t>>;

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
          [](exptr_wrapper&& exptr) {
            return ret_then_t{ std::in_place_index<2>, std::move(exptr) };
          }
        }, std::move(this->m_outcome));

    } catch (...) {
      using ret_t = std::invoke_result_t<F, E&&>;
      using ret_then_t = then_base<T, void_to_monostate<ret_t>>;

      return ret_then_t{ from_exception_ptr };
    }

    template<std::invocable<const exptr_wrapper&> F>
      requires requires(F&& f, const exptr_wrapper& exptr) {
        {std::invoke(std::forward<F>(f), exptr)} -> std::same_as<void>;
      }
    auto catch_exception(F&& func) && noexcept -> then_base try {
      return std::visit(overloaded{
          [](T&& v) {
            return then_base{std::move(v)};
          },
          [](E &&err) {
            return then_base{std::move(err)};
          },
          [&](exptr_wrapper&& exptr) {
            std::invoke(std::forward<F>(func), exptr);
            return then_base{ std::in_place_index<2>, std::move(exptr) };
          }
        }, std::move(this->m_outcome));
    } catch (...) {
      return then_base{from_exception_ptr};
    }

    template<specialization_of<exception_handler> F>
    auto catch_exception(F&& func) && noexcept -> then_base try {
      return std::visit(overloaded{
          [](T&& v) {
            return then_base{std::move(v)};
          },
          [](E&& err) {
            return then_base{std::move(err)};
          },
          [&](exptr_wrapper&& exptr) {
            // exception_handlerを通した場合、直でvisitに渡すことを意図しているものとして扱う
            visit(func, exptr);
            return then_base{ std::in_place_index<2>, std::move(exptr) };
          }
        }, std::move(this->m_outcome));
    } catch (...) {
      return then_base{from_exception_ptr};
    }

    template<std::invocable<T&&> F, std::invocable<E&&> EH>
    auto match(F&& on_success, EH&& on_error) && {
      using SR = std::remove_cvref_t<std::invoke_result_t<F&&, T&&>>;
      using ER = std::remove_cvref_t<std::invoke_result_t<EH&&, E&&>>;

      if constexpr (std::is_same_v<SR, void> and std::is_same_v<ER, void>) {
        std::visit(overloaded{
          [&](T&& v) {
            std::invoke(std::forward<F>(on_success), std::move(v));
          },
          [&](E&& err) {
            std::invoke(std::forward<EH>(on_error), std::move(err));
          },
          [&](exptr_wrapper&&) {}
        }, std::move(this->m_outcome));

        return;
      } else if constexpr (requires { typename std::common_type_t<SR, ER>; }) {
        using ret_t = std::optional<std::common_type_t<SR, ER>>;

        return std::visit(overloaded{
          [&](T&& v) {
            return ret_t{ std::invoke(std::forward<F>(on_success), std::move(v)) };
          },
          [&](E&& err) {
            return ret_t{ std::invoke(std::forward<EH>(on_error), std::move(err)) };
          },
          [&](exptr_wrapper&&) {
            return ret_t{ std::nullopt };
          }
        }, std::move(this->m_outcome));
      } else {
        static_assert([] {return false; }(), "Success and error return types cannot be deduced to a common type.");
      }
    }

    template<std::invocable<T&&> F, std::invocable<E&&> EH, std::invocable<exptr_wrapper&&> ExH>
    auto match(F&& on_success, EH&& on_error, ExH&& on_exception) && {
      using SR = std::remove_cvref_t<std::invoke_result_t<F&&, T&&>>;
      using ER = std::remove_cvref_t<std::invoke_result_t<EH&&, E&&>>;
      using ExR = std::remove_cvref_t<std::invoke_result_t<ExH &&, exptr_wrapper &&>>;

      if constexpr (std::is_same_v<SR, void> and std::is_same_v<ER, void> and std::is_same_v<ExR, void>) {
        std::visit(overloaded{
          [&](T&& v) {
            std::invoke(std::forward<F>(on_success), std::move(v));
          },
          [&](E&& err) {
            std::invoke(std::forward<EH>(on_error), std::move(err));
          },
          [&](exptr_wrapper&& ex) {
            std::invoke(std::forward<ExH>(on_exception), std::move(ex));
          }
        }, std::move(this->m_outcome));

        return;
      } else if constexpr (requires { typename std::common_type_t<SR, ER, ExR>; }) {
        using ret_t = std::common_type_t<SR, ER, ExR>;

        return std::visit(overloaded{
          [&](T&& v) {
            return ret_t{ std::invoke(std::forward<F>(on_success), std::move(v)) };
          },
          [&](E&& err) {
            return ret_t{ std::invoke(std::forward<EH>(on_error), std::move(err)) };
          },
          [&](exptr_wrapper&& ex) {
            return ret_t{ std::invoke(std::forward<ExH>(on_exception), std::move(ex)) };
          }
        }, std::move(this->m_outcome));
      } else {
        static_assert([] {return false; }(), "Success and error and exception return types cannot be deduced to a common type.");
      }
    }

    template<typename R, std::invocable<T&&> F, std::invocable<E&&> EH>
      requires std::convertible_to<std::invoke_result_t<F&&, T&&>, std::remove_cvref_t<R>> and
               std::convertible_to<std::invoke_result_t<EH&&, E&&>, std::remove_cvref_t<R>>
    auto match(F&& on_success, EH&& on_error, R&& default_value) && noexcept -> std::remove_cvref_t<R> try {
      return std::visit(overloaded{
          [&](T&& v) {
            return std::invoke(std::forward<F>(on_success), std::move(v));
          },
          [&](E&& err) {
            return std::invoke(std::forward<EH>(on_error), std::move(err));
          },
          [&](exptr_wrapper&&) {
            return std::forward<R>(default_value);
          }
        }, std::move(this->m_outcome));
    } catch (...) {
      // tryブロック内exptr_wrapperケースで、戻り値構築のムーブ中に例外が起きたとしたら、これは安全ではなくなる・・・？
      // とりあえず、ムーブコンストラクタで例外が投げられることは考えないことにする・・・
      return std::forward<R>(default_value);
    }

  };
}

namespace chttpp::detail {

  using std::ranges::data;
  using std::ranges::size;

  template<typename CharT>
  concept character = requires {
    requires std::is_same_v<CharT, char> or
             std::is_same_v<CharT, wchar_t> or 
             std::is_same_v<CharT, char8_t> or
             std::is_same_v<CharT, char16_t> or
             std::is_same_v<CharT, char32_t>;
  };

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
    http_status_code status_code;
  };

  class [[nodiscard]] http_result : private then_base<http_response, error_code> {
    using base = then_base<http_response, error_code>;
  public:
    using error_type = error_code;

    template<typename T>
      requires std::constructible_from<error_code, T, std::source_location>
    explicit http_result(T&& e, std::source_location ctx = std::source_location::current()) noexcept(std::is_nothrow_constructible_v<error_code, T>)
      : base{ error_code{ std::forward<T>(e), std::move(ctx) } }
    {}

    explicit http_result(error_code ec) noexcept(std::is_nothrow_move_constructible_v<error_code>)
      : base{std::move(ec)}
    {}

    explicit http_result(http_response&& res) noexcept(std::is_nothrow_move_constructible_v<http_response>)
      : base{std::move(res)}
    {}

    //http_result(http_result&& that) noexcept(std::is_nothrow_move_constructible_v<std::variant<http_response, error_code>>)
    //  : base{std::move(that)}
    //{}

    http_result(http_result&&) = default;
    
    http_result(const http_result&) = delete;
    http_result &operator=(const http_result&) = delete;

    explicit operator bool() const noexcept {
      return m_outcome.index() == 0;
    }

    bool has_response() const noexcept {
      return m_outcome.index() == 0;
    }

    auto status_code() const -> http_status_code {
      assert(bool(*this));
      const auto &response = std::get<0>(m_outcome);
      return response.status_code;
    }

    auto response_body() const & -> std::string_view {
      assert(bool(*this));
      const auto &response = std::get<0>(m_outcome);
      return {data(response.body), size(response.body)};
    }

    template<character CharT>
    auto response_body() const & -> std::basic_string_view<CharT> {
      assert(bool(*this));
      const auto &response = std::get<0>(m_outcome);
      return { reinterpret_cast<const CharT*>(data(response.body)), size(response.body) / sizeof(CharT)};
    }

    auto response_data() & -> std::span<char> {
      assert(bool(*this));
      auto& response = std::get<0>(m_outcome);
      return response.body;
    }

    auto response_data() const & -> std::span<const char> {
      assert(bool(*this));
      const auto &response = std::get<0>(m_outcome);
      return response.body;
    }

    template<substantial ElementType>
    auto response_data(std::size_t N = std::dynamic_extent) & -> std::span<ElementType> {
      assert(bool(*this));
      auto& response = std::get<0>(m_outcome);
      const std::size_t count = std::min(N, size(response.body) / sizeof(ElementType));

      return { reinterpret_cast<ElementType*>(data(response.body)), count };
    }

    template<substantial ElementType>
    auto response_data(std::size_t N = std::dynamic_extent) const & -> std::span<const ElementType> {
      assert(bool(*this));
      const auto &response = std::get<0>(m_outcome);
      const std::size_t count = std::min(N, size(response.body) / sizeof(ElementType));

      return {reinterpret_cast<const ElementType *>(data(response.body)), count};
    }

    auto response_header() const & -> const header_t& {
      assert(bool(*this));
      const auto &response = std::get<0>(m_outcome);
      return response.headers;
    }

    auto response_header(std::string_view header_name) const & -> std::string_view {
      assert(bool(*this));
      const auto &headers = std::get<0>(m_outcome).headers;

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
        return this->error().message();
      }
    }

    using base::then;
    using base::catch_error;
    using base::catch_exception;
    using base::match;

    /**
     * @brief 結果を文字列として任意の継続処理を実行する
     * @param f std::string_viewを1つ受けて呼び出し可能なもの
     * @details 有効値を保持していない場合（httpアクセスに失敗している場合）、空のstring_viewが渡される
     * @return f(str_view)の戻り値
     */
    template<std::invocable<std::string_view> F>
    friend decltype(auto) operator|(const http_result& self, F&& f) noexcept(std::is_nothrow_invocable_v<F, std::string_view>) {
      if (self.has_response()) {
        return std::invoke(std::forward<F>(f), self.response_body());
      } else {
        return std::invoke(std::forward<F>(f), std::string_view{});
      }
    }

    // optional/expected的インターフェース

    auto value() & -> http_response& {
      assert(bool(*this));
      return std::get<0>(m_outcome);
    }

    auto value() const & -> const http_response& {
      assert(bool(*this));
      return std::get<0>(m_outcome);
    }

    auto value() && -> http_response&& {
      assert(bool(*this));
      return std::get<0>(std::move(m_outcome));
    }

    auto error() const -> error_code {
      assert(bool(*this) == false);
      return std::get<1>(m_outcome);
    }
  };
}

namespace chttpp {
  using chttpp::detail::http_result;
  using chttpp::detail::http_response;
}