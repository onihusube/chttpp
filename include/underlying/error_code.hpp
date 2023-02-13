#pragma once

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

  public:
    explicit error_code(errc ec) : m_ec(std::move(ec)) {}

    auto message() const -> string_t {
      return ::chttpp::underlying::lib_error_code_tratis::error_to_string(m_ec);
    }

    auto value() const noexcept -> errc {
      return m_ec;
    }

    friend bool operator==(const error_code &self, errc ec) {
      return self.m_ec == ec;
    }
  };
}