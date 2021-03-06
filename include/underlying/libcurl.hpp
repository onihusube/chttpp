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

namespace chttpp::underlying::impl::initialize {

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
    // curl??????????????????????????????????????????=???????????????????????????????????????

    using namespace std::literals;

    // ?????????\r\n???????????????????????????
    const auto ln_pos = std::string_view{ data_ptr, data_len }.rfind("\r\n"sv);
    std::string_view header_str{ data_ptr, std::min(ln_pos, data_len) };

    // 1?????????????????????????????????????????????????????????????????????
    parse_response_header_oneline(headers, header_str);
  }
}

namespace chttpp::underlying::terse {

  using unique_curl = std::unique_ptr<CURL, decltype([](CURL* p) noexcept { curl_easy_cleanup(p); })>;
  using unique_slist = std::unique_ptr<curl_slist, decltype([](curl_slist* p) noexcept { curl_slist_free_all(p); })>;

  inline void unique_slist_append(unique_slist& plist, const char* value) noexcept {
    auto ptr = plist.release();
    plist.reset(curl_slist_append(ptr, value));
  }

  template<typename T, std::invocable<T&, char*, std::size_t> auto reciever>
  auto write_callback(char* data_ptr, std::size_t one, std::size_t length, void* buffer_ptr) -> std::size_t {
    auto& buffer_obj = *reinterpret_cast<T*>(buffer_ptr);
    const std::size_t data_len = one * length;  // ????????????(one)?????????1

    reciever(buffer_obj, data_ptr, data_len);

    // ???????????????????????????
    return data_len;
  }


  template<typename MethodTag>
    requires (not detail::tag::has_reqbody_method<MethodTag>)
  inline auto request_impl(std::string_view url, const vector_t<std::pair<std::string_view, std::string_view>>& req_headers, MethodTag) -> http_result {
    // ??????????????????
    constexpr bool is_get   = std::same_as<detail::tag::get_t, MethodTag>;
    constexpr bool is_head  = std::same_as<detail::tag::head_t, MethodTag>;
    constexpr bool is_opt   = std::same_as<detail::tag::options_t, MethodTag>;
    constexpr bool is_trace = std::same_as<detail::tag::trace_t, MethodTag>;

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
    curl_easy_setopt(session.get(), CURLOPT_USERAGENT, detail::default_UA.data());

    if constexpr (is_head) {
      curl_easy_setopt(session.get(), CURLOPT_NOBODY, 1L);
    } else if constexpr (is_opt) {
      curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "OPTIONS");
    } else if constexpr (is_trace) {
      curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "TRACE");
    }

    unique_slist req_header_list{};
    {
  
      constexpr std::string_view separater = ": ";
      string_t header_buffer{};
      header_buffer.reserve(100);

      for (const auto &[name, value] : req_headers) {
        // key: name ???????????????????????????
        header_buffer.append(name);
        if (value.empty()) {
          // ???????????????????????????????????????
          header_buffer.append(";");
        } else {
          header_buffer.append(separater);
          header_buffer.append(value);
        }

        unique_slist_append(req_header_list, header_buffer.c_str());

        header_buffer.clear();
      }
    }

    if (req_header_list) {
      curl_easy_setopt(session.get(), CURLOPT_HTTPHEADER, req_header_list.get());
    }

    // ???????????????????????????????????????????????????
    if constexpr (is_get or is_opt) { 
      auto* body_recieve = write_callback<decltype(body), [](decltype(body)& buffer, char* data_ptr, std::size_t data_len) {
        buffer.reserve(buffer.size() + data_len);
        std::ranges::copy(data_ptr, data_ptr + data_len, std::back_inserter(buffer));
      }>;
      curl_easy_setopt(session.get(), CURLOPT_WRITEFUNCTION, body_recieve);
      curl_easy_setopt(session.get(), CURLOPT_WRITEDATA, &body);
    }

    // ???????????????????????????????????????????????????
    auto* header_recieve = write_callback<decltype(headers), chttpp::detail::parse_response_header_on_curl>;
    curl_easy_setopt(session.get(), CURLOPT_HEADERFUNCTION, header_recieve);
    curl_easy_setopt(session.get(), CURLOPT_HEADERDATA, &headers);

    if (url.starts_with("https")) {
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYHOST, 0L);
    }

    const CURLcode curl_status = curl_easy_perform(session.get());

    if (curl_status != CURLE_OK) {
      return http_result{curl_status};
    }

    long http_status;
    curl_easy_getinfo(session.get(), CURLINFO_RESPONSE_CODE, &http_status);

    return http_result{chttpp::detail::http_response{.body = std::move(body), .headers = std::move(headers), .status_code = static_cast<std::uint16_t>(http_status)}};
  }

  template<detail::tag::has_reqbody_method MethodTag>
  inline auto request_impl(std::string_view url, std::string_view mime, std::span<const char> req_body, const vector_t<std::pair<std::string_view, std::string_view>>& req_headers, MethodTag) -> http_result {
    // ??????????????????
    [[maybe_unused]]
    constexpr bool is_post  = std::same_as<detail::tag::post_t, MethodTag>;
    constexpr bool is_put   = std::same_as<detail::tag::put_t, MethodTag>;
    constexpr bool is_del   = std::same_as<detail::tag::delete_t, MethodTag>;
    constexpr bool is_patch = std::same_as<detail::tag::patch_t, MethodTag>;

    // ??????
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
    curl_easy_setopt(session.get(), CURLOPT_USERAGENT, detail::default_UA.data());
    curl_easy_setopt(session.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(req_body.size()));
    curl_easy_setopt(session.get(), CURLOPT_POSTFIELDS, const_cast<char*>(req_body.data()));

    if constexpr (is_put) {
      curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "PUT");
    } else if constexpr (is_del) {
      curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if constexpr (is_patch) {
      //curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "PATCH");
      static_assert([]{return false;}(), "not implemented.");
    }

    unique_slist req_header_list{};
    {
      constexpr std::string_view separater = ": ";
      string_t header_buffer{};
      header_buffer.reserve(100);

      for (const auto &[name, value] : req_headers) {
        // key: name ???????????????????????????
        header_buffer.append(name);
        if (value.empty()) {
          // ????????????????????????????????????????????????curl?????????
          header_buffer.append(";");
        } else {
          header_buffer.append(separater);
          header_buffer.append(value);
        }

        unique_slist_append(req_header_list, header_buffer.c_str());

        header_buffer.clear();
      }

      const bool set_content_type = std::ranges::any_of(req_headers, [](auto name) { return name == "Content-Type"; }, &std::pair<std::string_view, std::string_view>::first);

      if (not set_content_type) {
        constexpr std::string_view name_str = "Content-Type: ";
        std::string_view mime_str{mime};

        header_buffer.append(name_str);
        header_buffer.append(mime_str);

        unique_slist_append(req_header_list, header_buffer.c_str());
      }
    }

    if (req_header_list) {
      curl_easy_setopt(session.get(), CURLOPT_HTTPHEADER, req_header_list.get());
    }

    // ???????????????????????????????????????????????????
    auto* resbody_recieve = write_callback<decltype(body), [](decltype(body)& buffer, char* data_ptr, std::size_t data_len) {
      buffer.reserve(buffer.size() + data_len);
      std::ranges::copy(data_ptr, data_ptr + data_len, std::back_inserter(buffer));
    }>;
    curl_easy_setopt(session.get(), CURLOPT_WRITEFUNCTION, resbody_recieve);
    curl_easy_setopt(session.get(), CURLOPT_WRITEDATA, &body);

    // ???????????????????????????????????????????????????
    auto* header_recieve = write_callback<decltype(headers), chttpp::detail::parse_response_header_on_curl>;
    curl_easy_setopt(session.get(), CURLOPT_HEADERFUNCTION, header_recieve);
    curl_easy_setopt(session.get(), CURLOPT_HEADERDATA, &headers);

    if (url.starts_with("https")) {
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYHOST, 0L);
    }

    const CURLcode curl_status = curl_easy_perform(session.get());

    if (curl_status != CURLE_OK) {
      return http_result{curl_status};
    }

    long http_status;
    curl_easy_getinfo(session.get(), CURLINFO_RESPONSE_CODE, &http_status);

    return http_result{chttpp::detail::http_response{.body = std::move(body), .headers = std::move(headers), .status_code = static_cast<std::uint16_t>(http_status)}};
  }

  inline auto wchar_to_char(std::wstring_view wstr) -> std::pair<string_t, std::size_t> {
    std::mbstate_t state{};
    const wchar_t *src = wstr.data();
    const std::size_t required_len = std::wcsrtombs(nullptr, &src, 0, &state) + 1;  // null????????????+1

    string_t buffer{};
    buffer.resize(required_len);

    // ?????????????????????????????????
    // ????????????????????????????????????????????????????????????
    const std::size_t converted_len = std::wcsrtombs(buffer.data(), &src, required_len, &state);

    return { std::move(buffer), converted_len };
  }

  template<typename... Args>
  auto request_impl(std::wstring_view wchar_url, Args&&... args) -> http_result {

    const auto [url, length] = wchar_to_char(wchar_url);

    if (length == static_cast<std::size_t>(-1)) {
      throw std::invalid_argument{"Failed to convert the URL wchar_t -> char."};
    }

    return request_impl(std::string_view{url.data(), length}, std::forward<Args>(args)...);
  }
}