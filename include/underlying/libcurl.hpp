#pragma once

#include <iostream>
#include <exception>
#include <memory>
#include <string_view>
#include <vector>
#include <cstring>
#include <iterator>
#include <ranges>
#include <algorithm>
#include <stdexcept>
#include <cstdlib>
#include <unordered_map>

#include <curl/curl.h>

#include "common.hpp"

#ifndef CHTTPP_NOT_GLOBAL_INIT_CURL

namespace chttpp::underlying::detail::initialize {

  inline const auto manage_curl_global_state_implicit_v = [] {
    const auto rc = ::curl_global_init(CURL_GLOBAL_ALL);

    if (rc != 0) {
      std::cout << "curl_global_init() failed. Code = " << rc << std::endl;
      std::exit(rc);
    }

    struct tag {
      explicit constexpr tag() = default;
    };

    struct I {

      I(tag) {}

      I(const I &) = delete;
      I &operator=(const I &) = delete;

      ~I() {
        ::curl_global_cleanup();
      }
    };

    return I{ tag{} };
  }();
}

#else

namespace chttpp {

  class raii_curl_global_state {
    bool is_release = true;
    const CURLcode code = CURLE_OK;

  public:

    raii_curl_global_state()
      : code(curl_global_init(CURL_GLOBAL_ALL))
    {}

    raii_curl_global_state(raii_curl_global_state&& that) noexcept 
      : is_release{that.is_release}
      , code{that.code}
    {
      that.is_release = false;
    }

    raii_curl_global_state(const raii_curl_global_state&) = delete;
    raii_curl_global_state& operator=(const raii_curl_global_state&) = delete;
    raii_curl_global_state& operator=(raii_curl_global_state&&) = delete;

    explicit operator bool() const noexcept {
      return this->code == 0;
    }

    CURLcode get_curl_code() const noexcept {
      return this->code;
    }

    ~raii_curl_global_state() {
      if (is_release) {
        ::curl_global_cleanup();
      }
    }
  };
}

#endif

namespace chttpp {
  using http_result = detail::basic_result<::CURLcode>;

  template<>
  inline auto http_result::error_to_string() const -> string_t {
    return {curl_easy_strerror(this->error())};
  }
}

namespace chttpp::detail {

  void parse_response_header_on_curl(header_t& headers, const char *data_ptr, std::size_t data_len) {
    // curlのヘッダコールバックは、行毎=ヘッダ要素毎に呼んでくれる

    using namespace std::literals;

    // 末尾に\r\nがあれば除いておく
    const auto ln_pos = std::string_view{ data_ptr, data_len }.rfind("\r\n"sv);
    std::string_view header_str{ data_ptr, std::min(ln_pos, data_len) };

    parse_response_header_oneline(headers, header_str);
  }
}

namespace chttpp::underlying::terse {

  using unique_curl = std::unique_ptr<CURL, decltype([](CURL* p) noexcept { curl_easy_cleanup(p); })>;

  template<typename T, std::invocable<T&, char*, std::size_t> auto reciever>
  auto write_callback(char* data_ptr, std::size_t one, std::size_t length, void* obj_ptr) -> std::size_t {
    auto& proc_obj = *reinterpret_cast<T*>(obj_ptr);
    const std::size_t data_len = one * length;

    reciever(proc_obj, data_ptr, data_len);

    // 返さないと失敗扱い
    return data_len;
  }

  auto get_impl(std::string_view url) -> http_result {
    unique_curl session{curl_easy_init()};

    if (not session) {
      return http_result{CURLE_FAILED_INIT};
    }

    vector_t<char> body{};
    header_t headers;

    curl_easy_setopt(session.get(), CURLOPT_URL, url.data());
    curl_easy_setopt(session.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(session.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(session.get(), CURLOPT_FOLLOWLOCATION, 1);

    // レスポンスボディコールバックの指定
    auto* body_recieve = write_callback<decltype(body), [](decltype(body)& buffer, char* data_ptr, std::size_t data_len) {
      buffer.reserve(buffer.size() + data_len);
      std::ranges::copy(data_ptr, data_ptr + data_len, std::back_inserter(buffer));
    }>;
    curl_easy_setopt(session.get(), CURLOPT_WRITEFUNCTION, body_recieve);
    curl_easy_setopt(session.get(), CURLOPT_WRITEDATA, &body);

    // レスポンスヘッダコールバックの指定
    auto* header_recieve = write_callback<decltype(headers), chttpp::detail::parse_response_header_on_curl>;
    curl_easy_setopt(session.get(), CURLOPT_HEADERFUNCTION, header_recieve);
    curl_easy_setopt(session.get(), CURLOPT_HEADERDATA, &headers);

    if (url.starts_with("https")) {
      //curl_easy_setopt(session.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_0 | CURL_SSLVERSION_MAX_TLSv1_3);
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode curl_status = curl_easy_perform(session.get());

    long http_status;
    curl_easy_getinfo(session.get(), CURLINFO_RESPONSE_CODE, &http_status);

    if (curl_status != CURLE_OK) {
      return http_result{curl_status};
    }

    return http_result{chttpp::detail::http_response{.body = std::move(body), .headers = std::move(headers), .status_code = static_cast<std::uint16_t>(http_status)}};
  }

  auto wchar_to_char(std::wstring_view wstr) -> std::pair<string_t, std::size_t> {
    const std::size_t estimate_len = wstr.length() * 2;
    string_t buffer{};
    buffer.resize(estimate_len);

    // ロケールの考慮・・・？
    // 外側でコントロールしてもらう方向で・・・
    const std::size_t converted_len = std::wcstombs(buffer.data(), wstr.data(), estimate_len);

    return { std::move(buffer), converted_len };
  }

  auto get_impl(std::wstring_view url) -> http_result {

    const auto [curl, length] = wchar_to_char(url);

    if (length == static_cast<std::size_t>(-1)) {
      throw std::invalid_argument{"Failed to convert the URL wchar_t -> char."};
    }

    return get_impl(std::string_view{curl.data(), length});
  }

  auto head_impl(std::string_view url) -> http_result {
    unique_curl session{curl_easy_init()};

    if (not session) {
      return http_result{CURLE_FAILED_INIT};
    }

    header_t headers;

    curl_easy_setopt(session.get(), CURLOPT_URL, url.data());
    curl_easy_setopt(session.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(session.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(session.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(session.get(), CURLOPT_NOBODY, 1L);

    // レスポンスヘッダコールバックの指定
    auto* header_recieve = write_callback<decltype(headers), chttpp::detail::parse_response_header_on_curl>;
    curl_easy_setopt(session.get(), CURLOPT_HEADERFUNCTION, header_recieve);
    curl_easy_setopt(session.get(), CURLOPT_HEADERDATA, &headers);

    if (url.starts_with("https")) {
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode curl_status = curl_easy_perform(session.get());

    if (curl_status != CURLE_OK) {
      return http_result{curl_status};
    }

    long http_status;
    curl_easy_getinfo(session.get(), CURLINFO_RESPONSE_CODE, &http_status);

    return http_result{chttpp::detail::http_response{.body = {}, .headers = std::move(headers), .status_code = static_cast<std::uint16_t>(http_status)}};

  }

  auto head_impl(std::wstring_view url) -> http_result {

    const auto [curl, length] = wchar_to_char(url);

    if (length == static_cast<std::size_t>(-1)) {
      throw std::invalid_argument{"Failed to convert the URL wchar_t -> char."};
    }

    return head_impl(std::string_view{curl.data(), length});
  }
}