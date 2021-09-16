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
  inline auto http_result::error_to_string() const -> std::string {
    return std::string{curl_easy_strerror(this->error())};
  }
}

namespace chttpp::underlying::terse {

  using unique_curl = std::unique_ptr<CURL, decltype([](CURL* p) noexcept { curl_easy_cleanup(p); })>;

  template<std::ranges::sized_range T>
    requires requires(T& t, std::size_t N, char c) {
      t.reserve(N);
      t.push_back(c);
    }
  auto write_callback(char* data_ptr, std::size_t one, std::size_t length, void* buf_ptr) -> std::size_t {
    auto& buffer = *reinterpret_cast<T*>(buf_ptr);
    const std::size_t data_len = one * length;

    // vectorを渡しても、stringを渡しても正しく動いて欲しい
    // が、おそらくmemcpy相当まで最適化されなさそう・・・
    buffer.reserve(buffer.size() + data_len);
    std::ranges::copy(data_ptr, data_ptr + data_len, std::back_inserter(buffer));

    // 返さないと失敗扱い
    return data_len;
  }

  auto get_impl(std::string_view url) -> http_result {
    unique_curl session{curl_easy_init()};

    if (not session) {
      return http_result{CURLE_FAILED_INIT, 0};
    }

    //std::string buffer{};
    std::vector<char> buffer{};

    curl_easy_setopt(session.get(), CURLOPT_URL, url.data());
    curl_easy_setopt(session.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(session.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(session.get(), CURLOPT_WRITEFUNCTION, write_callback<decltype(buffer)>);
    curl_easy_setopt(session.get(), CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(session.get(), CURLOPT_FOLLOWLOCATION, 1);

    if (url.starts_with("https")) {
      //curl_easy_setopt(session.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_0 | CURL_SSLVERSION_MAX_TLSv1_3);
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYHOST, 0L);
    }

    CURLcode curl_status = curl_easy_perform(session.get());

    long http_status;
    curl_easy_getinfo(session.get(), CURLINFO_RESPONSE_CODE, &http_status);

    if (curl_status != CURLE_OK) {
      return http_result{curl_status, static_cast<std::uint16_t>(http_status)};
    }

    return http_result{std::move(buffer), static_cast<std::uint16_t>(http_status)};
  }


  auto get_impl(std::wstring_view url) -> http_result {
    const std::size_t estimate_len = url.length() * 2;
    std::string buffer{};
    buffer.resize(estimate_len);

    // ロケールの考慮・・・？
    // 外側でコントロールしてもらう方向で・・・
    const std::size_t converted_len = std::wcstombs(buffer.data(), url.data(), estimate_len);

    if (converted_len == static_cast<std::size_t>(-1)) {
      throw std::invalid_argument{"Failed to convert the URL wchar_t -> char."};
    }

    return get_impl(std::string_view{buffer.data(), converted_len});
  }
}