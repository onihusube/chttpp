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
#include <numeric>

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
    // curlのヘッダコールバックは、行毎=ヘッダ要素毎に呼んでくれる

    using namespace std::literals;

    // 末尾に\r\nがあれば除いておく
    const auto ln_pos = std::string_view{ data_ptr, data_len }.rfind("\r\n"sv);
    std::string_view header_str{ data_ptr, std::min(ln_pos, data_len) };

    // 1行分のヘッダの読み取り・変換・格納は共通処理へ
    parse_response_header_oneline(headers, header_str);
  }

  inline auto rebuild_url(CURLU* hurl, const vector_t<std::pair<std::string_view, std::string_view>>& params, string_t& buffer) -> char* {
    for (const auto& p : params) {
      // name=value の形にしてから追記
      buffer.append(p.first);
      buffer.append("=");
      buffer.append(p.second);

      // URLエンコード（=を除く）と&付加はしてくれる
      curl_url_set(hurl, CURLUPART_QUERY, buffer.c_str(), CURLU_APPENDQUERY | CURLU_URLENCODE);

      buffer.clear();
    }

    // 認証情報が含まれる場合に削除する
    // basic認証実装時はこの前で設定する必要がある
    curl_url_set(hurl, CURLUPART_USER, nullptr, 0);
    curl_url_set(hurl, CURLUPART_PASSWORD, nullptr, 0);

    // アンカーの削除（これはブラウザでのみ意味があり通常リクエストの一部ではない）
    curl_url_set(hurl, CURLUPART_FRAGMENT, nullptr, 0);

    char *purl = nullptr;
    // 編集後urlの取得
    const auto res = curl_url_get(hurl, CURLUPART_URL, &purl, 0);
    
    if (res != CURLUcode::CURLUE_OK) {
      return nullptr;
    }

    return purl;
  }
}

namespace chttpp::underlying::terse {

  using unique_curl = std::unique_ptr<CURL, decltype([](CURL* p) noexcept { curl_easy_cleanup(p); })>;
  using unique_slist = std::unique_ptr<curl_slist, decltype([](curl_slist* p) noexcept { curl_slist_free_all(p); })>;
  using unique_curlurl = std::unique_ptr<CURLU, decltype([](CURLU* p) noexcept { curl_url_cleanup(p); })>;
  using unique_curlchar = std::unique_ptr<char, decltype([](char* p) noexcept { curl_free(p); })>;

  inline void unique_slist_append(unique_slist& plist, const char* value) noexcept {
    auto ptr = plist.release();
    plist.reset(curl_slist_append(ptr, value));
  }

  template<typename T, std::invocable<T&, char*, std::size_t> auto reciever>
  auto write_callback(char* data_ptr, std::size_t one, std::size_t length, void* buffer_ptr) -> std::size_t {
    auto& buffer_obj = *reinterpret_cast<T*>(buffer_ptr);
    const std::size_t data_len = one * length;  // 第二引数(one)は常に1

    reciever(buffer_obj, data_ptr, data_len);

    // 返さないと失敗扱い
    return data_len;
  }


  inline void common_proxy_setting(CURL* session, const detail::proxy_config& prxy_cfg) {
    // アドレス（address:port）設定
    curl_easy_setopt(session, CURLOPT_PROXY, const_cast<char*>(prxy_cfg.address.data()));
    // スキーム（プロトコル）設定
    const ::curl_proxytype scheme = [&] {
      using enum cfg::proxy_scheme;
      switch (prxy_cfg.scheme)
      {
      case http:
        return curl_proxytype::CURLPROXY_HTTP;
      case https:
        return curl_proxytype::CURLPROXY_HTTPS;
      case socks4:
        return curl_proxytype::CURLPROXY_SOCKS4;
      case socks4a:
        return curl_proxytype::CURLPROXY_SOCKS4A;
      case socks5:
        return curl_proxytype::CURLPROXY_SOCKS5;
      case socks5h:
        return curl_proxytype::CURLPROXY_SOCKS5_HOSTNAME;
      default:
        assert(false);
        return curl_proxytype::CURLPROXY_HTTP;
      }
    }();
    curl_easy_setopt(session, CURLOPT_PROXYTYPE, scheme);

    if (not prxy_cfg.auth.username.empty()) {
      // 仮定
      assert(not prxy_cfg.auth.password.empty());

      // とりあえずbasic認証のみ考慮
      curl_easy_setopt(session, CURLOPT_PROXYAUTH, CURLAUTH_BASIC);

      curl_easy_setopt(session, CURLOPT_PROXYUSERNAME, const_cast<char*>(prxy_cfg.auth.username.data()));
      curl_easy_setopt(session, CURLOPT_PROXYPASSWORD, const_cast<char*>(prxy_cfg.auth.password.data()));
    }
  }


  template<typename MethodTag>
    requires (not detail::tag::has_reqbody_method<MethodTag>)
  inline auto request_impl(std::string_view url, detail::request_config_for_get&& cfg, MethodTag) -> http_result {
    // メソッド判定
    constexpr bool is_get   = std::same_as<detail::tag::get_t, MethodTag>;
    constexpr bool is_head  = std::same_as<detail::tag::head_t, MethodTag>;
    constexpr bool is_opt   = std::same_as<detail::tag::options_t, MethodTag>;
    constexpr bool is_trace = std::same_as<detail::tag::trace_t, MethodTag>;

    unique_curl session{curl_easy_init()};

    if (not session) {
      return http_result{CURLE_FAILED_INIT};
    }

    // 使い回し用ローカルバッファ
    string_t buffer;
    {
      const std::size_t init_len = sizeof("User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/106.0.0.0 Safari/537.36 Edg/106.0.1363.0");
      buffer.reserve(init_len);
    }

    // URLの編集（パラメータ追加など）
    unique_curlurl hurl{curl_url()};
    if (not hurl) {
      return http_result{CURLE_FAILED_INIT};
    }
    assert(curl_url_set(hurl.get(), CURLUPART_URL, url.data(), 0) == CURLUE_OK);

    // 認証情報の取得とセット
    // URL編集前に行う必要がある（編集中にURLに含まれている情報を消すため）
    // configに指定された方を優先する
    if (not cfg.auth.username.empty()) {
      // とりあえずbasic認証のみ考慮
      curl_easy_setopt(session.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
      
      curl_easy_setopt(session.get(), CURLOPT_USERNAME, const_cast<char*>(cfg.auth.username.data()));
      curl_easy_setopt(session.get(), CURLOPT_PASSWORD, const_cast<char*>(cfg.auth.password.data()));
    } else {
      // 指定されない場合、URLに含まれているものを使用する
      char* user = nullptr;
      char* pw = nullptr;

      if (curl_url_get(hurl.get(), CURLUPART_USER, &user, 0) == CURLUE_OK) {
        // あるものとする？
        assert(curl_url_get(hurl.get(), CURLUPART_PASSWORD, &pw, 0) == CURLUE_OK);

        // RAII
        unique_curlchar userptr{user};
        unique_curlchar pwptr{pw};

        // とりあえずbasic認証
        curl_easy_setopt(session.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

        curl_easy_setopt(session.get(), CURLOPT_USERNAME, user);
        curl_easy_setopt(session.get(), CURLOPT_PASSWORD, pw);
      }
    }

    // 指定されたURLパラメータを含むようにURLを編集、その先頭ポインタを得る
    unique_curlchar purl{detail::rebuild_url(hurl.get(), cfg.params, buffer)};

    if (purl == nullptr) {
      // エラーコードの変換については要検討
      return http_result{CURLcode::CURLE_URL_MALFORMAT};
    }

    vector_t<char> body{};
    header_t headers;

    curl_easy_setopt(session.get(), CURLOPT_URL, purl.get());
    {
      // HTTP versionの指定
      using enum cfg::http_version;
      switch (cfg.version)
      {
      case http2:
        curl_easy_setopt(session.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
        break;
      case http1_1:
        curl_easy_setopt(session.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        break;
      default:
        assert(false);
        break;
      }
    }
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

    // タイムアウトの指定
    {
      const long timeout = cfg.timeout.count();

      curl_easy_setopt(session.get(), CURLOPT_TIMEOUT_MS, timeout);
      curl_easy_setopt(session.get(), CURLOPT_CONNECTTIMEOUT_MS, timeout);
    }

    // Proxyの指定
    if (not cfg.proxy.address.empty()) {
      common_proxy_setting(session.get(), cfg.proxy);
    }

    unique_slist req_header_list{};
    {
      constexpr std::string_view separater = ": ";

      auto& header_buffer = buffer;
      assert(header_buffer.empty());

      auto& req_headers = cfg.headers;

      for (const auto &[key, value] : req_headers) {
        // key: value となるようにコピー
        header_buffer.append(key);
        if (value.empty()) {
          // 中身が空のヘッダを追加する
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

    // レスポンスボディコールバックの指定
    if constexpr (is_get or is_opt) { 
      auto* body_recieve = write_callback<decltype(body), [](decltype(body)& buffer, char* data_ptr, std::size_t data_len) {
        buffer.reserve(buffer.size() + data_len);
        std::ranges::copy(data_ptr, data_ptr + data_len, std::back_inserter(buffer));
      }>;
      curl_easy_setopt(session.get(), CURLOPT_WRITEFUNCTION, body_recieve);
      curl_easy_setopt(session.get(), CURLOPT_WRITEDATA, &body);
    }

    // レスポンスヘッダコールバックの指定
    auto* header_recieve = write_callback<decltype(headers), chttpp::detail::parse_response_header_on_curl>;
    curl_easy_setopt(session.get(), CURLOPT_HEADERFUNCTION, header_recieve);
    curl_easy_setopt(session.get(), CURLOPT_HEADERDATA, &headers);

    if (url.starts_with("https")) {
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYPEER, 1L);
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYHOST, 1L);
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
  inline auto request_impl(std::string_view url, std::span<const char> req_body, detail::request_config&& cfg, MethodTag) -> http_result {
    // メソッド判定
    [[maybe_unused]]
    constexpr bool is_post  = std::same_as<detail::tag::post_t, MethodTag>;
    constexpr bool is_put   = std::same_as<detail::tag::put_t, MethodTag>;
    constexpr bool is_del   = std::same_as<detail::tag::delete_t, MethodTag>;
    constexpr bool is_patch = std::same_as<detail::tag::patch_t, MethodTag>;

    // 本編
    unique_curl session{curl_easy_init()};

    if (not session) {
      return http_result{CURLE_FAILED_INIT};
    }

    // 使い回し用ローカルバッファ
    string_t buffer;
    {
      const std::size_t init_len = sizeof("User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/106.0.0.0 Safari/537.36 Edg/106.0.1363.0");
      buffer.reserve(init_len);
    }

    // URLの編集（パラメータ追加など）
    unique_curlurl hurl{curl_url()};
    if (not hurl) {
      return http_result{CURLE_FAILED_INIT};
    }
    assert(curl_url_set(hurl.get(), CURLUPART_URL, url.data(), 0) == CURLUE_OK);

    // 認証情報の取得とセット
    // URL編集前に行う必要がある（編集中にURLに含まれている情報を消すため）
    // configに指定された方を優先する
    if (not cfg.auth.username.empty()) {
      // とりあえずbasic認証のみ考慮
      curl_easy_setopt(session.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
      
      curl_easy_setopt(session.get(), CURLOPT_USERNAME, const_cast<char*>(cfg.auth.username.data()));
      curl_easy_setopt(session.get(), CURLOPT_PASSWORD, const_cast<char*>(cfg.auth.password.data()));
    } else {
      // 指定されない場合、URLに含まれているものを使用する
      char* user = nullptr;
      char* pw = nullptr;

      if (curl_url_get(hurl.get(), CURLUPART_USER, &user, 0) == CURLUE_OK) {
        // あるものとする？
        assert(curl_url_get(hurl.get(), CURLUPART_PASSWORD, &pw, 0) == CURLUE_OK);

        // RAII
        unique_curlchar userptr{user};
        unique_curlchar pwptr{pw};

        // とりあえずbasic認証
        curl_easy_setopt(session.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

        curl_easy_setopt(session.get(), CURLOPT_USERNAME, user);
        curl_easy_setopt(session.get(), CURLOPT_PASSWORD, pw);
      }
    }

    // 編集後URLへのポインタ
    unique_curlchar purl{detail::rebuild_url(hurl.get(), cfg.params, buffer)};

    if (purl == nullptr) {
      // エラーコードの変換については要検討
      return http_result{CURLcode::CURLE_URL_MALFORMAT};
    }

    vector_t<char> body{};
    header_t headers;

    curl_easy_setopt(session.get(), CURLOPT_URL, purl.get());
    {
      // HTTP versionの指定
      using enum cfg::http_version;
      switch (cfg.version)
      {
      case http2:
        curl_easy_setopt(session.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
        break;
      case http1_1:
        curl_easy_setopt(session.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        break;
      default:
        assert(false);
        break;
      }
    }
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

    // タイムアウトの指定
    {
      const long timeout = cfg.timeout.count();

      curl_easy_setopt(session.get(), CURLOPT_TIMEOUT_MS, timeout);
      curl_easy_setopt(session.get(), CURLOPT_CONNECTTIMEOUT_MS, timeout);
    }

    // Proxyの指定
    if (not cfg.proxy.address.empty()) {
      common_proxy_setting(session.get(), cfg.proxy);
    }

    unique_slist req_header_list{};
    {
      constexpr std::string_view separater = ": ";

      auto &header_buffer = buffer;
      assert(header_buffer.empty());

      auto& req_headers = cfg.headers;

      for (const auto &[key, value] : req_headers) {
        // key: value となるようにコピー
        header_buffer.append(key);
        if (value.empty()) {
          // 中身が空のヘッダを追加するときのcurlの作法
          header_buffer.append(";");
        } else {
          header_buffer.append(separater);
          header_buffer.append(value);
        }

        unique_slist_append(req_header_list, header_buffer.c_str());

        header_buffer.clear();
      }

      // Content-Type指定はヘッダ設定を優先する
      const bool set_content_type = std::ranges::any_of(req_headers, [](auto name) { return name == "Content-Type" or name == "content-type"; }, &std::pair<std::string_view, std::string_view>::first);

      if (not set_content_type) {
        constexpr std::string_view content_type = "content-type: ";

        header_buffer.append(content_type);
        header_buffer.append(cfg.content_type);

        unique_slist_append(req_header_list, header_buffer.c_str());
      }
    }

    if (req_header_list) {
      curl_easy_setopt(session.get(), CURLOPT_HTTPHEADER, req_header_list.get());
    }

    // レスポンスボディコールバックの指定
    auto* resbody_recieve = write_callback<decltype(body), [](decltype(body)& buffer, char* data_ptr, std::size_t data_len) {
      buffer.reserve(buffer.size() + data_len);
      std::ranges::copy(data_ptr, data_ptr + data_len, std::back_inserter(buffer));
    }>;
    curl_easy_setopt(session.get(), CURLOPT_WRITEFUNCTION, resbody_recieve);
    curl_easy_setopt(session.get(), CURLOPT_WRITEDATA, &body);

    // レスポンスヘッダコールバックの指定
    auto* header_recieve = write_callback<decltype(headers), chttpp::detail::parse_response_header_on_curl>;
    curl_easy_setopt(session.get(), CURLOPT_HEADERFUNCTION, header_recieve);
    curl_easy_setopt(session.get(), CURLOPT_HEADERDATA, &headers);

    if (url.starts_with("https")) {
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYPEER, 1L);
      curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYHOST, 1L);
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
    const std::size_t required_len = std::wcsrtombs(nullptr, &src, 0, &state) + 1;  // null文字の分+1

    string_t buffer{};
    buffer.resize(required_len);

    // ロケールの考慮・・・？
    // 外側でコントロールしてもらう方向で・・・
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