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
    //std::uint16_t status_code;
    http_status_code status_code;
  };

  class [[nodiscard]] http_result {
    // exception_ptrも入れ込みたい、そのうち
    std::variant<http_response, error_code> m_either;

  public:
    using error_type = error_code;

    template<typename T>
      requires std::constructible_from<error_code, T, std::source_location>
    explicit http_result(T&& e, std::source_location ctx = std::source_location::current()) noexcept(std::is_nothrow_constructible_v<error_code, T>)
      : m_either{std::in_place_index<1>, std::forward<T>(e), std::move(ctx)}
    {}

    explicit http_result(const error_code& ec) noexcept(std::is_nothrow_copy_constructible_v<error_code>)
      : m_either{ std::in_place_index<1>, ec}
    {}

    explicit http_result(http_response&& res) noexcept(std::is_nothrow_move_constructible_v<http_response>)
      : m_either{std::in_place_index<0>, std::move(res)}
    {}

    http_result(http_result&& that) noexcept(std::is_nothrow_move_constructible_v<std::variant<http_response, error_code>>)
      : m_either{std::move(that.m_either)}
    {}

    http_result(const http_result&) = delete;
    http_result &operator=(const http_result&) = delete;

    explicit operator bool() const noexcept {
      return m_either.index() == 0;
    }

    bool has_response() const noexcept {
      return m_either.index() == 0;
    }

    auto status_code() const -> http_status_code {
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
        return this->error().message();
      }
    }

    template<typename F>
    auto then(F&& func) && noexcept {
      using ret_then_t = then_impl<http_response, error_code>;

      // これnoexceptか・・・？
      return std::visit(overloaded{
        [](http_response&& value) {
          return ret_then_t{ std::move(value)} ;
        },
        [](error_code&& err) {
          return ret_then_t{ std::move(err)} ;
        },
      }, std::move(this->m_either)).then(std::forward<F>(func));
    }

    template<std::invocable<error_code&&> F>
    auto catch_error(F&& func) && noexcept {
      using ret_then_t = then_impl<http_response, error_code>;

      // これnoexceptか・・・？
      return std::visit(overloaded{
        [](http_response&& value) {
          return ret_then_t{ std::move(value)};
        },
        [](error_code&& err) {
          return ret_then_t{ std::move(err)};
        },
      }, std::move(this->m_either)).catch_error(std::forward<F>(func));
    }

    template<std::invocable<http_response&&> F, std::invocable<error_code&&> EH>
    void match(F&& on_success, EH&& on_error) && {
      using ret_then_t = then_impl<http_response, error_code>;
      
      return std::visit(overloaded{
        [](http_response&& value) {
          return ret_then_t{ std::move(value)};
        },
        [](error_code&& err) {
          return ret_then_t{ std::move(err)};
        },
      }, std::move(this->m_either)).match(std::forward<F>(on_success), std::forward<EH>(on_error));
    }

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
      return std::get<0>(m_either);
    }

    auto value() const & -> const http_response& {
      return std::get<0>(m_either);
    }

    auto value() && -> http_response&& {
      return std::get<0>(std::move(m_either));
    }

    auto error() const -> error_code {
      return std::get<1>(m_either);
    }
  };
}

namespace chttpp {
  using chttpp::detail::http_result;
  using chttpp::detail::http_response;
}