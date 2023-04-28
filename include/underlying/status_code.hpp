#pragma once

#include <utility>
#include <cstdint>
#include <type_traits>
#include <source_location>

#include "common.hpp"

namespace chttpp::detail {

  /**
   * @brief このライブラリのためだけの単純なエラーコードラッパ
   * @details 表面的にライブラリ固有エラーコード型が出てこない様にする
   * @details 状態は常にエラー
   */
  class error_code {
    using errc = ::chttpp::underlying::lib_error_code_tratis::errc;

    errc m_ec;
    std::source_location m_context;

  public:
    explicit error_code() noexcept : m_ec(::chttpp::underlying::lib_error_code_tratis::no_error_value), m_context{} {}

    explicit error_code(errc ec, std::source_location&& ctx = std::source_location::current()) noexcept(std::is_nothrow_move_constructible_v<errc>)
      : m_ec(std::move(ec))
      , m_context{std::move(ctx)}
    {}

    error_code(const error_code&) = default;
    error_code(error_code&&) = default;

    error_code& operator=(const error_code&) & = default;
    error_code& operator=(error_code&&) & = default;

    error_code& operator=(errc ec) & {
      m_ec = std::move(ec);
      return *this;
    }

    [[nodiscard]]
    auto message() const -> string_t {
      return ::chttpp::underlying::lib_error_code_tratis::error_to_string(m_ec);
    }

    [[nodiscard]]
    auto value() const noexcept -> errc {
      return m_ec;
    }

    [[nodiscard]]
    auto context() const noexcept -> const std::source_location& {
      return m_context;
    }

    explicit operator bool() const noexcept {
      // エラーの時にtrue
      return m_ec != ::chttpp::underlying::lib_error_code_tratis::no_error_value;
    }

    [[nodiscard]]
    friend bool operator==(error_code, error_code) = default;

    [[nodiscard]]
    friend bool operator==(error_code self, errc ec) noexcept {
      return self.m_ec == ec;
    }
  };


  /**
   * @brief HTTPステータスコードのラッパ型
   * @details HTTPステータスコードに特化した処理を提供する
   */
  class http_status_code {
    std::uint16_t m_code;

  public:

    explicit http_status_code(std::integral auto code) noexcept
      : m_code(static_cast<std::uint16_t>(code))
    {}

    // よく使うと思われるレスポンスをチェックする関数
    // 命名は対応するReason-Phraseから、他と命名規則が異なりアッパーキャメルケースなのはそれが一番違和感ないと思ったため
    // 参考 : https://developer.mozilla.org/ja/docs/Web/HTTP/Status

    [[nodiscard]]
    bool OK() const noexcept {
      return std::cmp_equal(m_code, 200);
    }

    [[nodiscard]]
    bool Found() const noexcept {
      return std::cmp_equal(m_code, 302);
    }

    [[nodiscard]]
    bool Unauthorized() const noexcept {
      return std::cmp_equal(m_code, 401);
    }

    [[nodiscard]]
    bool Forbidden() const noexcept {
      return std::cmp_equal(m_code, 403);
    }

    [[nodiscard]]
    bool NotFound() const noexcept {
      return std::cmp_equal(m_code, 404);
    }
    
    [[nodiscard]]
    bool RequestTimeout() const noexcept {
      return std::cmp_equal(m_code, 408);
    }

    [[nodiscard]]
    bool InternalServerError() const noexcept {
      return std::cmp_equal(m_code, 500);
    }

    [[nodiscard]]
    bool ServiceUnavailable() const noexcept {
      return std::cmp_equal(m_code, 503);
    }

    // ステータスが5つのクラスのどれに該当しているかを調べる
    // 要は、応答の1桁目の数字を調べることに対応する

    [[nodiscard]]
    bool is_informational() const noexcept {
      return 100u <= m_code && m_code < 200u;
    }

    [[nodiscard]]
    bool is_successful() const noexcept {
      return 200u <= m_code && m_code < 300u;
    }

    [[nodiscard]]
    bool is_redirection() const noexcept {
      return 300u <= m_code && m_code < 400u;
    }

    [[nodiscard]]
    bool is_client_error() const noexcept {
      return 400u <= m_code && m_code < 500u;
    }

    [[nodiscard]]
    bool is_server_error() const noexcept {
      return 500u <= m_code && m_code < 600u;
    }

    [[nodiscard]]
    auto value() const noexcept -> std::uint16_t {
      return m_code;
    }

    [[nodiscard]]
    friend bool operator==(http_status_code, http_status_code) = default;

    [[nodiscard]]
    friend bool operator==(http_status_code self, std::integral auto num) noexcept {
      return std::cmp_equal(self.m_code, num);
    }
  };
}