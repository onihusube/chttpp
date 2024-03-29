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

namespace chttpp::underlying {
  struct lib_error_code_tratis {
    using errc = ::CURLcode;

    static constexpr ::CURLcode no_error_value = ::CURLE_OK;
    static constexpr ::CURLcode url_error_value = ::CURLE_URL_MALFORMAT;

    static auto error_to_string(::CURLcode ec) -> string_t {
      return {curl_easy_strerror(ec)};
    }
  };
}

#include "status_code.hpp"
#include "http_result.hpp"

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

namespace chttpp::detail {

  inline void parse_response_header_on_curl(header_t& headers, const char *data_ptr, std::size_t data_len) {
    // curlのヘッダコールバックは、行毎=ヘッダ要素毎に呼んでくれる

    using namespace std::literals;

    // 末尾に\r\nがあれば除いておく
    const auto ln_pos = std::string_view{ data_ptr, data_len }.rfind("\r\n"sv);
    std::string_view header_str{ data_ptr, std::min(ln_pos, data_len) };

    // 1行分のヘッダの読み取り・変換・格納は共通処理へ
    parse_response_header_oneline(headers, header_str);
  }
}

namespace chttpp::underlying {

  using ::chttpp::detail::http_result;
  using detail::util::string_buffer;

  template<typename P, auto& del>
    requires requires(P* p) {
      del(p);
    }
  struct deleter_t {
    void operator()(P* p) const noexcept {
      del(p);
    }
  };

  using unique_curl = std::unique_ptr<CURL, deleter_t<CURL, curl_easy_cleanup>>;
  using unique_slist = std::unique_ptr<curl_slist, deleter_t<curl_slist, curl_slist_free_all>>;
  using unique_curlurl = std::unique_ptr<CURLU, deleter_t<CURLU, curl_url_cleanup>>;
  using unique_curlchar = std::unique_ptr<char, deleter_t<char, curl_free>>;

  inline void unique_slist_append(unique_slist& plist, const char* value) noexcept {
    auto ptr = plist.release();
    plist.reset(curl_slist_append(ptr, value));
  }

  template<typename T, std::invocable<T&, char*, std::size_t> auto receiver>
  auto write_callback(char* data_ptr, std::size_t one, std::size_t length, void* buffer_ptr) -> std::size_t {
    auto& buffer_obj = *reinterpret_cast<T*>(buffer_ptr);
    const std::size_t data_len = one * length;  // 第二引数(one)は常に1

    receiver(buffer_obj, data_ptr, data_len);

    // 返さないと失敗扱い
    return data_len;
  }

  inline auto rebuild_url(CURLU* hurl, const vector_t<std::pair<std::string_view, std::string_view>>& params, string_buffer& buffer) -> char* {
    for (const auto& p : params) {
      buffer.use([&](auto& param_buf) {
        // name=value の形にしてから追記
        param_buf.append(p.first);
        param_buf.append("=");
        param_buf.append(p.second);

        // URLエンコード（=を除く）と&付加はしてくれる
        curl_url_set(hurl, CURLUPART_QUERY, param_buf.c_str(), CURLU_APPENDQUERY | CURLU_URLENCODE);
      });
    }

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

  inline auto wchar_to_char(std::wstring_view wstr) -> std::pair<string_t, std::size_t> {
    std::mbstate_t state{};
    const wchar_t *src = wstr.data();

    // この長さはnull文字を含まない
    const std::size_t required_len = std::wcsrtombs(nullptr, &src, 0, &state);

    string_t buffer{};
    // resize()はnull文字を含まない長さを指定する
    buffer.resize(required_len);

    // ロケールの考慮・・・？
    // 外側でコントロールしてもらう方向で・・・
    const std::size_t converted_len = std::wcsrtombs(buffer.data(), &src, required_len, &state);

    return { std::move(buffer), converted_len };
  }

  inline auto wchar_to_char(std::wstring_view wstr, string_t& out) -> bool {
    std::mbstate_t state{};
    const wchar_t *src = wstr.data();

    // この長さはnull文字を含まない
    const std::size_t required_len = std::wcsrtombs(nullptr, &src, 0, &state);

    // resize()はnull文字を含まない長さを指定する
    out.resize(required_len);

    // ロケールの考慮・・・？
    // 外側でコントロールしてもらう方向で・・・
    return std::wcsrtombs(out.data(), &src, required_len, &state) == static_cast<std::size_t>(-1);
  }

  inline auto to_string(std::string_view str) {
    return str;
  }

  inline auto to_string(std::wstring_view wstr) -> string_t {
    string_t buffer{};

    if (wchar_to_char(wstr, buffer)) {
      return "";
    } else {
      return buffer;
    }
  }

  struct libcurl_session_state {
    // CURLのセッションハンドル
    unique_curl session = nullptr;
    // 使い回し用バッファ
    string_buffer buffer{};
    // CURLのURL
    unique_curlurl hurl = nullptr;

    // URLから抽出した認証情報の保存用
    unique_curlchar userptr = nullptr;
    unique_curlchar pwptr = nullptr;

    libcurl_session_state() = default;

    auto init(std::string_view url, const detail::config::proxy_config& prxy_cfg, std::chrono::milliseconds timeout, cfg::http_version version) & -> detail::error_code {
      // セッションハンドル初期化
      session.reset(curl_easy_init());

      if (not session) {
        return detail::error_code{CURLE_FAILED_INIT};
      }

      {
        // HTTP versionの指定
        using enum cfg::http_version;
        switch (version)
        {
        case http2:
          curl_easy_setopt(session.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
          break;
        /*case http3:
          curl_easy_setopt(session.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_3);
          break;*/
        case http1_1:
          curl_easy_setopt(session.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
          break;
        default:
          assert(false);
          break;
        }
      }

      // タイムアウトの指定
      {
        const long timeout_ms = timeout.count();

        curl_easy_setopt(session.get(), CURLOPT_TIMEOUT_MS, timeout_ms);
        curl_easy_setopt(session.get(), CURLOPT_CONNECTTIMEOUT_MS, timeout_ms);
      }

      // Proxyの指定
      if (not prxy_cfg.address.empty()) {
        common_proxy_setting(session.get(), prxy_cfg);
      }

      if (url.starts_with("https")) {
        curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(session.get(), CURLOPT_SSL_VERIFYHOST, 1L);
      }

      // この辺のURLからの情報抽出、url_infoを使うことができるかもしれない
      hurl.reset(curl_url());
      if (not hurl) {
        // エラーコード要検討
        return detail::error_code{CURLE_FAILED_INIT};
      }
      // URLの読み込み
      if (auto ec = curl_url_set(hurl.get(), CURLUPART_URL, url.data(), 0); ec != CURLUE_OK) {
        // エラーコード要検討
        return detail::error_code{CURLE_URL_MALFORMAT};
      }

      // URLに含まれている認証情報の取得
      // URL編集前に行う必要がある（編集中にURLに含まれている情報を消すため）
      // 実際の認証設定はリクエスト時に行う
      char* user = nullptr;
      char* pw = nullptr;

      if (curl_url_get(hurl.get(), CURLUPART_USER, &user, 0) == CURLUE_OK) {
        // あるものとする？
        assert(curl_url_get(hurl.get(), CURLUPART_PASSWORD, &pw, 0) == CURLUE_OK);

        // RAII
        userptr.reset(user);
        pwptr.reset(pw);

        // URLに含まれている場合はここで設定してしまえば良い
        // この場合はbasic認証決め打ち（でいい？
        curl_easy_setopt(session.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

        curl_easy_setopt(session.get(), CURLOPT_USERNAME, user);
        curl_easy_setopt(session.get(), CURLOPT_PASSWORD, pw);

        // URLのホスト部から認証情報を削除する
        curl_url_set(hurl.get(), CURLUPART_USER, nullptr, 0);
        curl_url_set(hurl.get(), CURLUPART_PASSWORD, nullptr, 0);
      }

      return detail::error_code{};
    }

    auto init(std::string_view url_head, underlying::agent_impl::dummy_buffer, const detail::config::agent_initial_config& init_cfg) & -> detail::error_code {
      return this->init(url_head, init_cfg.proxy, init_cfg.timeout, init_cfg.version);
    }

    auto init(std::wstring_view url_head, detail::string_buffer& cnv_buf, const detail::config::agent_initial_config& init_cfg) & -> detail::error_code {
      return cnv_buf.use([&, this](string_t &converted_url) {
          if (wchar_to_char(url_head, converted_url)) {
            // std::errcだとillegal_byte_sequence
            return detail::error_code{CURLcode::CURLE_CONV_FAILED};
          }

          return this->init(converted_url, init_cfg.proxy, init_cfg.timeout, init_cfg.version);
        });
    }

  };
}


namespace chttpp::underlying::terse {

  using namespace chttpp::underlying;

  template<typename MethodTag>
  auto request_impl(libcurl_session_state&& state, auto&& cfg, [[maybe_unused]] std::span<const char> req_body, MethodTag) -> http_result {
    // メソッドタイプ判定
    constexpr bool has_request_body = detail::tag::has_reqbody_method<MethodTag>;

    // メソッド判定
    constexpr bool is_get = std::is_same_v<detail::tag::get_t, MethodTag>;
    constexpr bool is_head = std::is_same_v<detail::tag::head_t, MethodTag>;
    constexpr bool is_opt = std::is_same_v<detail::tag::options_t, MethodTag>;
    constexpr bool is_trace = std::is_same_v<detail::tag::trace_t, MethodTag>;
    [[maybe_unused]]
    constexpr bool is_post = std::is_same_v<detail::tag::post_t, MethodTag>;
    constexpr bool is_put = std::is_same_v<detail::tag::put_t, MethodTag>;
    constexpr bool is_del = std::is_same_v<detail::tag::delete_t, MethodTag>;
    constexpr bool is_patch = std::is_same_v<detail::tag::patch_t, MethodTag>;

    auto& session = state.session;
    auto& buffer = state.buffer;
    auto& hurl = state.hurl;

    // 認証情報の取得とセット、configに指定された方を優先する
    if (cfg.auth.scheme != detail::authentication_scheme::none) {
      // とりあえずbasic認証のみ考慮
      curl_easy_setopt(session.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

      curl_easy_setopt(session.get(), CURLOPT_USERNAME, const_cast<char *>(cfg.auth.username.data()));
      curl_easy_setopt(session.get(), CURLOPT_PASSWORD, const_cast<char *>(cfg.auth.password.data()));
    }

    // 指定されたURLパラメータを含むようにURLを編集、その先頭ポインタを得る
    unique_curlchar purl{rebuild_url(hurl.get(), cfg.params, buffer)};

    if (purl == nullptr) {
      // エラーコードの変換については要検討
      return http_result{CURLcode::CURLE_URL_MALFORMAT};
    }

    // URLのセット
    curl_easy_setopt(session.get(), CURLOPT_URL, purl.get());

    vector_t<char> body{};
    header_t headers;

    curl_easy_setopt(session.get(), CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(session.get(), CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(session.get(), CURLOPT_USERAGENT, detail::default_UA.data());

    if constexpr (has_request_body) {

      curl_easy_setopt(session.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(req_body.size()));
      curl_easy_setopt(session.get(), CURLOPT_POSTFIELDS, const_cast<char *>(req_body.data()));

      if constexpr (is_put) {
        curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "PUT");
      } else if constexpr (is_del) {
        curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "DELETE");
      } else if constexpr (is_patch) {
        //curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "PATCH");
        static_assert([]{return false;}(), "not implemented.");
      }
    } else {
      if constexpr (is_head) {
        curl_easy_setopt(session.get(), CURLOPT_NOBODY, 1L);
      } else if constexpr (is_opt) {
        curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "OPTIONS");
      } else if constexpr (is_trace) {
        curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "TRACE");
      }
    }

    unique_slist req_header_list{};
    {
      constexpr std::string_view separater = ": ";

      auto& req_headers = cfg.headers;

      for (const auto& [key, value] : req_headers) {
        buffer.use([&](string_t& header_buffer) {
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
        });
      }

      if constexpr (has_request_body) {
        // Content-Type指定はヘッダ設定を優先する
        const bool set_content_type = std::ranges::any_of(req_headers, [](auto name) { return name == "Content-Type" or name == "content-type"; }, &std::pair<std::string_view, std::string_view>::first);

        if (not set_content_type) {
          constexpr std::string_view content_type = "content-type: ";

          buffer.use([&](string_t& header_buffer) {
            header_buffer.append(content_type);
            header_buffer.append(cfg.content_type);

            unique_slist_append(req_header_list, header_buffer.c_str());
          });
        }
      }
    }

    if (req_header_list) {
      curl_easy_setopt(session.get(), CURLOPT_HTTPHEADER, req_header_list.get());
    }

    // レスポンスボディコールバックの指定
    if constexpr (has_request_body or is_get or is_opt) { 
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

    const CURLcode curl_status = curl_easy_perform(session.get());

    if (curl_status != CURLE_OK) {
      return http_result{curl_status};
    }

    long http_status;
    curl_easy_getinfo(session.get(), CURLINFO_RESPONSE_CODE, &http_status);

    return http_result{chttpp::detail::http_response{ {}, std::move(body), std::move(headers), detail::http_status_code{http_status} }};
  }

  template<typename... Args>
  auto request_impl(std::string_view url, auto&& cfg, Args&&... args) noexcept -> http_result try {
    libcurl_session_state session_obj;

    if (auto ec = session_obj.init(url, cfg.proxy, cfg.timeout, cfg.version); ec != CURLE_OK) {
      return http_result{ec};
    }

    return request_impl(std::move(session_obj), std::move(cfg), std::forward<Args>(args)...);
  } catch (...) {
    return http_result{detail::from_exception_ptr};
  }

  template<typename... Args>
  auto request_impl(std::wstring_view wchar_url, Args&&... args) noexcept -> http_result try {

    const auto [url, length] = wchar_to_char(wchar_url);

    if (length == static_cast<std::size_t>(-1)) {
      throw std::invalid_argument{"Failed to convert the URL wchar_t -> char."};
    }

    return request_impl(std::string_view{url.data(), length}, std::forward<Args>(args)...);
  } catch (...) {
    return http_result{detail::from_exception_ptr};
  }
}

namespace chttpp::underlying::agent_impl {

  using namespace chttpp::underlying;
  using session_state = libcurl_session_state;

  template<>
  struct determin_buffer<char> {
    using type = dummy_buffer;
  };

  template<>
  struct determin_buffer<wchar_t> {
    using type = detail::string_buffer;
  };

  struct agent_resource {
    // コンストラクタで渡す設定
    detail::agent_initial_config config;

    // その他のタイミングで渡される設定
    umap_t<string_t, string_t> headers{};
    detail::cookie_store cookie_vault{};

    chttpp::cookie_management cookie_management;
    chttpp::follow_redirects follow_redirect;
    chttpp::automatic_decompression auto_decomp;

    // agentの状態詳細（agentでしか使わないバッファなどはsession_stateに置かないようにする）
    libcurl_session_state state{};
    // URLからコンポーネント情報などを抽出する（ほぼクッキーのため）
    detail::url_info request_url;
    // vector_t<detail::cookie_ref> cookie_buf{};
    detail::vector_buffer<detail::cookie_ref> cookie_buf{};
  };

  template<typename MethodTag>
  inline auto request_impl(std::string_view url_path, agent_resource& resource, detail::agent_request_config&& req_cfg, [[maybe_unused]] std::span<const char> req_body, MethodTag) -> http_result {
    // メソッドタイプ判定
    constexpr bool has_request_body = detail::tag::has_reqbody_method<MethodTag>;

    // メソッド判定
    constexpr bool is_get = std::is_same_v<detail::tag::get_t, MethodTag>;
    constexpr bool is_head = std::is_same_v<detail::tag::head_t, MethodTag>;
    constexpr bool is_opt = std::is_same_v<detail::tag::options_t, MethodTag>;
    constexpr bool is_trace = std::is_same_v<detail::tag::trace_t, MethodTag>;
    [[maybe_unused]]
    constexpr bool is_post = std::is_same_v<detail::tag::post_t, MethodTag>;
    constexpr bool is_put = std::is_same_v<detail::tag::put_t, MethodTag>;
    constexpr bool is_del = std::is_same_v<detail::tag::delete_t, MethodTag>;
    constexpr bool is_patch = std::is_same_v<detail::tag::patch_t, MethodTag>;

    auto& state = resource.state;
    auto& session = state.session;

    // 認証情報の取得とセット、configに指定された方を優先する
    // 以前に設定されていて、後のリクエストで認証情報が削除された場合、curl_easy_reset()を呼び出さないと認証情報をクリアできない
    // あるいは、送信ヘッダから削除するという方法があるらしいが・・・
    if (req_cfg.auth.scheme != detail::authentication_scheme::none) {
      // とりあえずbasic認証のみ考慮
      curl_easy_setopt(session.get(), CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

      curl_easy_setopt(session.get(), CURLOPT_USERNAME, const_cast<char *>(req_cfg.auth.username.data()));
      curl_easy_setopt(session.get(), CURLOPT_PASSWORD, const_cast<char *>(req_cfg.auth.password.data()));
    }

    // URL末尾に追加部分を付加
    // この関数の実行中はURLを維持する
    [[maybe_unused]]
    auto pinning_fullurl = resource.request_url.append_path(url_path);

    // 同じcurluステートを使い回してメモリ確保回避を期待（なってるんかな
    auto &hurl = state.hurl;

    // URLの読み込み
    // クエリが事前に存在する場合などのハンドルが面倒なので、毎回URLをセットし直す
    if (auto ec = curl_url_set(hurl.get(), CURLUPART_URL, resource.request_url.full_url().data(), 0); ec != CURLUE_OK) {
      // エラーコード要検討
      return http_result{CURLcode::CURLE_URL_MALFORMAT};
    }

    // 指定されたURLパラメータを含むようにURLを編集、その先頭ポインタを得る
    unique_curlchar purl{rebuild_url(hurl.get(), req_cfg.params, state.buffer)};

    if (purl == nullptr) {
      // エラーコードの変換については要検討
      return http_result{CURLcode::CURLE_URL_MALFORMAT};
    }

    // フルに構成したURLをセット
    curl_easy_setopt(session.get(), CURLOPT_URL, purl.get());

    if (resource.auto_decomp.enabled()) {
      // この指定はlibcurlが対応する全ての圧縮を自動解凍する指定
      // ヘッダで指定された場合はそちらが送信される
      // その場合、自動解凍はされる？（ChatGPTはされるって言ってる）
      curl_easy_setopt(session.get(), CURLOPT_ACCEPT_ENCODING, "");
    } else {
      // Accept-Encodingヘッダがない場合、サーバーは好きな圧縮で送ってくる
      curl_easy_setopt(session.get(), CURLOPT_ACCEPT_ENCODING, nullptr);
    }

    curl_easy_setopt(session.get(), CURLOPT_USERAGENT, detail::default_UA.data());

    if (resource.follow_redirect.enabled()) {
      curl_easy_setopt(session.get(), CURLOPT_FOLLOWLOCATION, 1L);
    } else {
      curl_easy_setopt(session.get(), CURLOPT_FOLLOWLOCATION, 0L);
    }

    if constexpr (has_request_body) {

      curl_easy_setopt(session.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(req_body.size()));
      curl_easy_setopt(session.get(), CURLOPT_POSTFIELDS, const_cast<char *>(req_body.data()));

      if constexpr (is_put) {
        curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "PUT");
      } else if constexpr (is_del) {
        curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "DELETE");
      } else if constexpr (is_patch) {
        //curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "PATCH");
        static_assert([]{return false;}(), "not implemented.");
      }
    } else {

      if constexpr (is_get) {
        // 一度GET以外のリクエストを行った後でGETに戻す、明示しないと前回のメソッドが引き継がれてしまう
        curl_easy_setopt(session.get(), CURLOPT_HTTPGET, 1L);
      } else if constexpr (is_head) {
        curl_easy_setopt(session.get(), CURLOPT_NOBODY, 1L);
      } else if constexpr (is_opt) {
        curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "OPTIONS");
      } else if constexpr (is_trace) {
        curl_easy_setopt(session.get(), CURLOPT_CUSTOMREQUEST, "TRACE");
      }
    }

    unique_slist req_header_list{};
    {
      constexpr std::string_view separater = ": ";

      auto set_headers = [&](string_t& header_buffer,  std::string_view key, std::string_view value) {
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
      };

      // agentに直接設定されたヘッダ、リクエスト間で共通
      for (const auto& [key, value] : resource.headers) {
        state.buffer.use([&](string_t& buf) { set_headers(buf, key, value); });
      }

      // リクエスト時指定のヘッダ、リクエスト毎に独立
      for (const auto& [key, value] : req_cfg.headers) {
        state.buffer.use([&](string_t& buf) { set_headers(buf, key, value); });
      }
      
      if constexpr (has_request_body) {

        // Content-Type指定はヘッダ設定を優先する
        const bool set_content_type =
            resource.headers.contains("Content-Type") or
            resource.headers.contains("content-type") or
            std::ranges::any_of(req_cfg.headers, [](auto name) { return name == "Content-Type" or name == "content-type"; }, &std::pair<std::string_view, std::string_view>::first);

        if (not set_content_type) {
          constexpr std::string_view content_type = "content-type: ";

          state.buffer.use([&](string_t& header_buffer) {
            header_buffer.append(content_type);
            header_buffer.append(req_cfg.content_type);

            unique_slist_append(req_header_list, header_buffer.c_str());
          });
        }
      }
    }

    // CURLOPT_HTTPHEADERの複数回の呼び出しは最後のみ有効
    // ここで、前回のリクエスト時のヘッダ設定は消える
    // ヘッダがない場合（req_header_listがnullptrの場合）、ヘッダ設定がリセットされる
    curl_easy_setopt(session.get(), CURLOPT_HTTPHEADER, req_header_list.get());

    // クッキーの設定など
    {
      // 自動クッキー管理が無効だとしても、クッキーを送信しないわけではない
      if (resource.cookie_management.enabled()) {
        // 期限切れクッキーの削除
        resource.cookie_vault.remove_expired_cookies();
      }

      // クッキーを送信順になるようにソート
      auto [sorted_cookies, pinning_cookies] = resource.cookie_buf.pin([&](auto &cookie_buf) -> std::span<const detail::cookie_ref> {
        // ソート
        resource.cookie_vault.create_cookie_list_to(cookie_buf, req_cfg.cookies, resource.request_url);

        return { cookie_buf };
      });

      // クッキーの設定
      // ヘッダより後で設定するため、ヘッダ設定を上書きする？（未確認）
      const auto cookie_ec = state.buffer.use([&](string_t &cookie_str) {
        // "name1=value1; name2=value2; ..."のように整形する
        for (const auto& c : sorted_cookies) {
          cookie_str.append(c.name());
          cookie_str.append(1, '=');
          cookie_str.append(c.value());
          cookie_str.append("; ");
        }

        return curl_easy_setopt(session.get(), CURLOPT_COOKIE, cookie_str.c_str());
      });

      if (cookie_ec != CURLcode::CURLE_OK) {
        return http_result{cookie_ec};
      }
    }

    vector_t<char> body{};
    header_t headers;

    // レスポンスボディコールバックの指定
    if constexpr (has_request_body or is_get or is_opt) {
      if (req_cfg.streaming_receiver) {
        // カスタムのコールバックによる応答本文受け取り
        auto* body_recieve = write_callback<decltype(req_cfg.streaming_receiver), [](decltype(req_cfg.streaming_receiver)& callback, char* data_ptr, std::size_t data_len) {
          callback(std::span<const char>{data_ptr, data_len});
        }>;
        curl_easy_setopt(session.get(), CURLOPT_WRITEFUNCTION, body_recieve);
        curl_easy_setopt(session.get(), CURLOPT_WRITEDATA, &req_cfg.streaming_receiver);
      } else {
        // デフォルトのコールバック
        auto* body_recieve = write_callback<decltype(body), [](decltype(body)& buffer, char* data_ptr, std::size_t data_len) {
          buffer.reserve(buffer.size() + data_len);
          std::ranges::copy(data_ptr, data_ptr + data_len, std::back_inserter(buffer));
        }>;
        curl_easy_setopt(session.get(), CURLOPT_WRITEFUNCTION, body_recieve);
        curl_easy_setopt(session.get(), CURLOPT_WRITEDATA, &body);
      }
    }

    // レスポンスヘッダコールバックの指定
    auto* header_recieve = write_callback<decltype(headers), chttpp::detail::parse_response_header_on_curl>;
    curl_easy_setopt(session.get(), CURLOPT_HEADERFUNCTION, header_recieve);
    curl_easy_setopt(session.get(), CURLOPT_HEADERDATA, &headers);

    const CURLcode curl_status = curl_easy_perform(session.get());

    if (curl_status != CURLE_OK) {
      return http_result{curl_status};
    }

    long http_status;
    curl_easy_getinfo(session.get(), CURLINFO_RESPONSE_CODE, &http_status);

    if (resource.cookie_management.enabled()) {
      // サーバからのクッキーを保存する（あれば
      if (const auto pos = headers.find("set-cookie"); pos != headers.end()) {
        resource.cookie_vault.insert_from_set_cookie((*pos).second, resource.request_url.host());
      }
    }

    return http_result{chttpp::detail::http_response{ {}, std::move(body), std::move(headers), detail::http_status_code{http_status} }};
  }

  template<typename... Args>
  auto request_impl(std::string_view url_path, dummy_buffer, Args&&... args) noexcept -> http_result try {
    // bufferをはがすだけ
    return request_impl(url_path, std::forward<Args>(args)...);
  } catch (...) {
    return http_result{detail::from_exception_ptr};
  }

  template<typename... Args>
  auto request_impl(std::wstring_view url_path, detail::string_buffer& buffer, Args&&... args) noexcept -> http_result try {
    // path文字列をcharへ変換する
    return buffer.use([&](string_t& converted_url) {
        if (wchar_to_char(url_path, converted_url)) {
          // std::errcだとillegal_byte_sequence
          return http_result{ CURLcode::CURLE_CONV_FAILED };
        }

        return request_impl(converted_url, std::forward<Args>(args)...);
      });
  } catch (...) {
    return http_result{detail::from_exception_ptr};
  }

}