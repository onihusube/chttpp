#pragma once

#include <string_view>
#include <memory>
#include <cassert>
#include <iostream>
#include <memory_resource>
#include <algorithm>
#include <functional>
#include <format>
#include <numeric>
#include <execution>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")

namespace chttpp::underlying {
  struct lib_error_code_tratis {
    using errc = DWORD;

    static constexpr DWORD no_error_value = ERROR_SUCCESS;

    static constexpr DWORD url_error_value = ERROR_WINHTTP_INVALID_URL;

    static auto error_to_string(DWORD err) -> string_t {
      constexpr std::ptrdiff_t max_len = 192;
      string_t str{};
      str.resize(max_len);

      // まずエラーコードを記載
      const auto [add_pos, msglen] = std::format_to_n(str.begin(), max_len, "GetLastError() = {} ", err);
        assert(msglen <= max_len);

      if (const std::size_t len = ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, err, LANG_USER_DEFAULT, std::to_address(add_pos), static_cast<DWORD>(max_len - msglen), nullptr); len == 0) {
        // 失敗（対応する文字列表現が見つからないとき）
        // null文字の位置を変更する（切り詰める）だけなのでアロケートは発生しないはず
        str.resize(msglen);

        // おそらく説明が記載されているリンクを付加
        str.append("(see https://learn.microsoft.com/en-us/windows/win32/winhttp/error-messages).");
      } else {
        // lenは終端\0を含まない長さ
        assert(len <= max_len);
        // null文字の位置を変更する（切り詰める）だけなのでアロケートは発生しないはず
        str.resize(msglen + len);
      }

      // 暗黙ムーブ
      return str;
    }
  };
}

#include "status_code.hpp"
#include "http_result.hpp"

namespace chttpp::detail {

  inline auto parse_response_header_on_winhttp(std::string_view header_str) -> header_t {
    using namespace std::literals;

    header_t headers;

    // この処理、views::splitでできないか・・・？
    auto current_pos = header_str.begin();
    const auto terminator = "\r\n"sv;
    std::boyer_moore_searcher search{ terminator.begin(), terminator.end() };

    // \0の分と最後にダブルで入ってる\r\nの分を引いておく
    const auto end_pos = header_str.end() - 1 - 2;
    while (current_pos < end_pos) {
      const auto line_end_pos = std::search(current_pos, header_str.end(), search);

      parse_response_header_oneline(headers, std::string_view{ current_pos, line_end_pos });
      current_pos = std::ranges::next(line_end_pos, 2, end_pos); // \r\nの次へ行くので+2
    }

    return headers;
  }
}

namespace chttpp::underlying {

  struct hinet_deleter {
    using pointer = HINTERNET;

    void operator()(HINTERNET hi) const noexcept {
      WinHttpCloseHandle(hi);
    }
  };

  using hinet = std::unique_ptr<HINTERNET, hinet_deleter>;

  using detail::util::string_buffer;
  using detail::util::wstring_buffer;

  /**
  * @brief charの文字列をstd::wstringへ変換する
  * @param cstr 変換したい文字列
  * @return {変換後文字列, 失敗を示すbool値（失敗したらtrue）}
  */
  auto char_to_wchar(std::string_view cstr) -> std::pair<wstring_t, bool> {
    const std::size_t converted_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, cstr.data(), static_cast<int>(cstr.length()), nullptr, 0);
    wstring_t converted_str{};
    converted_str.resize(converted_len);
    int res = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, cstr.data(), static_cast<int>(cstr.length()), converted_str.data(), static_cast<int>(converted_len));

    return { std::move(converted_str), res == 0 };
  }

  auto char_to_wchar(std::string_view cstr, wstring_t& out) -> bool {
    const std::size_t converted_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, cstr.data(), static_cast<int>(cstr.length()), nullptr, 0);
    
    out.resize(converted_len);
    int res = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, cstr.data(), static_cast<int>(cstr.length()), out.data(), static_cast<int>(converted_len));

    return res == 0;
  }

  inline auto to_string(std::string_view str) {
    return str;
  }

  inline auto to_string(std::wstring_view wstr) -> string_t {
    assert(not wstr.empty());

    // この長さはnull文字を含む
    const std::size_t converted_len = ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wstr.data(), -1, nullptr, 0, nullptr, nullptr);
    
    string_t converted_str{};
    converted_str.resize(converted_len - 1);

    if (::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wstr.data(), -1, converted_str.data(), static_cast<int>(converted_str.length()), nullptr, nullptr) != 0) {
      return converted_str;
    } else {
      converted_str.clear();
      return converted_str;
    }
  }

  inline auto wchar_to_char(std::wstring_view wstr, string_t& out) -> bool {
    // この長さはnull文字を含む
    const std::size_t converted_len = ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wstr.data(), -1, nullptr, 0, nullptr, nullptr);

    // 入力が空？
    if (converted_len <= 1) {
      return true;
    }

    out.resize(converted_len - 1);

    return ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wstr.data(), -1, out.data(), static_cast<int>(out.length()), nullptr, nullptr) == 0;
  }


  inline auto init_winhttp_session(const detail::proxy_config& proxy_conf) -> hinet {
    if (proxy_conf.address.empty()) {
      return hinet{ WinHttpOpen(detail::default_UA_w.data(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };
    } else {
      // WinHttpSetOptionでプロクシを設定する場合は、ここの第二引数をWINHTTP_ACCESS_TYPE_NO_PROXYにしておかなければならない
      // なぜここで設定しないのか？ -> ここでのプロクシはおそらく普通のhttpプロクシしか対応してない（ドキュメントではCERN type proxies for HTTPのみと指定されている
      //                          -> httpsをトンネルするタイプのプロクシを使用できないと思われる（未確認）
      hinet hsession{ WinHttpOpen(detail::default_UA_w.data(), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };

      wstring_t prxy_buf{};
      // WINHTTP_PROXY_INFO構造体に格納するためのプロクシアドレスの構成
      {
        using enum cfg::proxy_scheme;

        switch (proxy_conf.scheme) {
        case http:
          prxy_buf.append(L"http://");
          break;
        case https:
          prxy_buf.append(L"https://");
          break;
        case socks4: [[fallthrough]];
        case socks4a: [[fallthrough]];
        case socks5: [[fallthrough]];
        case socks5h:
          // socksプロクシの場合はsocks=address:port、とする
          // どうやらバージョン指定はいらないらしい（未確認
          prxy_buf.append(L"socks=");
          break;
        default: std::unreachable();
        };

        // アドレス:ポートの部分の変換後長さ。\0を含まない
        const std::size_t converted_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, proxy_conf.address.data(), static_cast<int>(proxy_conf.address.length()), nullptr, 0);

        const auto scheme_len = prxy_buf.length();

        // 構成後の長さに合うようにリサイズ
        prxy_buf.resize(scheme_len + converted_len);

        // スキーム文字列の後ろに追記
        int res = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, proxy_conf.address.data(), static_cast<int>(proxy_conf.address.length()), prxy_buf.data() + scheme_len, static_cast<int>(converted_len));

        assert(res == converted_len);
      }

      ::WINHTTP_PROXY_INFO prxy_info{
        .dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY,
        .lpszProxy = prxy_buf.data(),
        .lpszProxyBypass = const_cast<wchar_t*>(L"<local>") // これ大丈夫なんすか？？
      };

      // プロクシアドレスの設定
      if (not ::WinHttpSetOption(hsession.get(), WINHTTP_OPTION_PROXY, &prxy_info, sizeof(::WINHTTP_PROXY_INFO))) {
        return nullptr;
      }

      // 暗黙ムーブ
      return hsession;
    }

    //std::unreachable();
  }

  inline auto build_path_and_query(std::wstring_view path, std::wstring_view org_query, const vector_t<std::pair<std::string_view, std::string_view>>& params) -> wstring_t {
    wstring_t new_path;

    // アンカー削除
    // アンカーとはURLの最後にくっついてる#xxxのこと、ページ内ジャンプとかフォームの送信とかで生成される
    // アンカーはリクエストURLの一部ではなくブラウザ以外で意味がない
    if (const auto pos = org_query.find_last_of('#'); pos != std::wstring_view::npos) {
      org_query = org_query.substr(0, pos);
    }

    if (params.empty()) {
      new_path.reserve(path.length() + org_query.length() + 1);

      new_path.append(path);
      new_path.append(org_query);

      return new_path;
    }

    const std::size_t param_len = std::accumulate(params.begin(), params.end(), 0ull, [](std::size_t acc, const auto& pair) {
      return acc + pair.first.length() + 1 + pair.second.length() + 1;
    });

    new_path.reserve(path.length() + org_query.length() + param_len);

    new_path.append(path);

    // org_queryは?を含んでいる
    if (org_query.empty()) {
      new_path.append(L"?");
    } else {
      new_path.append(org_query);
      new_path.append(L"&");
    }

    string_t buffer{};
    for (auto& p : params) {
      // name=value& を追記していく（URLエンコードをどうするか？
      buffer.append(p.first);
      buffer.append("=");
      buffer.append(p.second);
      buffer.append("&");
    }
    // 一番最後の&を取り除く
    buffer.pop_back();

    // クエリ部分をまとめてwchar_t変換
    {
      // クエリ部分の変換後の長さ（\0を含まない長さが得られる）
      const std::size_t converted_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, buffer.data(), static_cast<int>(buffer.length()), nullptr, 0);
      // パス部分の今の（元の）長さ
      const auto path_len = new_path.length();
      // クエリ部分を追加可能なように拡張
      new_path.resize(new_path.length() + converted_len);
      // 変換後文字列を直接書き込み
      int res = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, buffer.data(), static_cast<int>(buffer.length()), new_path.data() + path_len, static_cast<int>(converted_len));

      assert(res == converted_len);
    }

    return new_path;
  }

  inline bool common_authentication_setting(HINTERNET req_handle, const detail::authorization_config& auth, const ::URL_COMPONENTS& url_component, wstring_buffer& buffer, int target) {

    // 設定で指定された方を優先
    if (auth.scheme != detail::authentication_scheme::none) {
      // ユーザー名とパスワードのwchar_t変換

      // ユーザー名の長さ
      const std::size_t user_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, auth.username.data(), static_cast<int>(auth.username.length()), nullptr, 0);
      // パスワードの長さ
      const std::size_t pw_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, auth.password.data(), static_cast<int>(auth.password.length()), nullptr, 0);

      return buffer.use([&](wstring_t& auth_buf) {
        // \0を1つ含むようにリサイズ
        auth_buf.resize(user_len + pw_len + 1);

        // ユーザー名の変換
        int res_user = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, auth.username.data(), static_cast<int>(auth.username.length()), auth_buf.data(), static_cast<int>(user_len));
        // パスワードの変換
        int res_pw = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, auth.password.data(), static_cast<int>(auth.password.length()), auth_buf.data() + user_len + 1, static_cast<int>(pw_len));
        // これら変換では、1つのバッファの中で\0で区切って2つの文字列を格納している

        assert(res_user == user_len);
        assert(res_pw == pw_len);
        // パスワードは空でもいい、その場合0になる？
        assert(1ull < pw_len);

        auth_buf[user_len] = L'\0';

        // \0を含まない範囲を参照させる
        std::wstring_view user{ auth_buf.data(), user_len };
        std::wstring_view pw{ auth_buf.data() + user_len + 1, pw_len };

        // Basic認証以外の認証では、WinHttpSendRequest()の後に応答を確認しつつ設定し再送する必要がある
        // https://docs.microsoft.com/ja-jp/windows/win32/winhttp/authentication-in-winhttp

        return ::WinHttpSetCredentials(req_handle, target, WINHTTP_AUTH_SCHEME_BASIC, user.data(), pw.data(), nullptr) == TRUE;
      });
    } else if (url_component.dwUserNameLength != 0) {
      // URLにユーザー名とパスワードが含まれている場合
      // null終端のためにいったんバッファにコピー
      return buffer.use([&](wstring_t& auth_buf) {
        // \0を1つ含むようにリサイズ
        auth_buf.reserve(static_cast<std::size_t>(url_component.dwUserNameLength) + url_component.dwPasswordLength + 1);
        auth_buf.append(url_component.lpszUserName, url_component.dwUserNameLength);
        auth_buf.append(1, L'\0');
        auth_buf.append(url_component.lpszPassword, url_component.dwPasswordLength);

        std::wstring_view user{ auth_buf.data(), url_component.dwUserNameLength };
        std::wstring_view pw{ auth_buf.data() + user.length() + 1, url_component.dwPasswordLength };

        return ::WinHttpSetCredentials(req_handle, target, WINHTTP_AUTH_SCHEME_BASIC, user.data(), pw.data(), nullptr) == TRUE;
      });
    } else {
      // 認証なし
      return true;
    }
  }

  inline bool proxy_authentication_setting(HINTERNET req_handle, const detail::authorization_config& auth, wstring_buffer& buffer) {
    // プロクシアドレスからユーザー名とパスワード（あれば）を抽出
    // とりあえずnot suport
    /*
    ::URL_COMPONENTS prxy_url_component{ .dwStructSize = sizeof(::URL_COMPONENTS), .dwUserNameLength = (DWORD)-1, .dwPasswordLength = (DWORD)-1 };
    if (not ::WinHttpCrackUrl(prxy_buf.data(), static_cast<DWORD>(prxy_buf.length()), 0, &prxy_url_component)) {
      return http_result{ ::GetLastError() };
    }*/

    return common_authentication_setting(req_handle, auth, {}, buffer, WINHTTP_AUTH_TARGET_PROXY);
  }


  struct winhttp_session_state {
    hinet session;
    hinet connect;

    ::URL_COMPONENTS url_component;

    std::wstring_view http_ver_str;
    wstring_t host_name;

    // 使いまわす共有バッファ
    wstring_buffer buffer;

    // ヘッダ文字列構成時に使用する
    string_buffer char_buf;

    auto init(std::wstring_view url, const detail::config::proxy_config& prxy_cfg, std::chrono::milliseconds timeout, cfg::http_version version) & -> detail::error_code {

      // sessionの初期化
      session = init_winhttp_session(prxy_cfg);

      // タイムアウトの設定
      const int timeout_int = static_cast<int>(timeout.count());

      // セッション全体ではなく各点でのタイムアウト指定になる
      // 最悪の場合、指定時間の4倍の時間待機することになる・・・？
      if (not ::WinHttpSetTimeouts(session.get(), timeout_int, timeout_int, timeout_int, timeout_int)) {
        return detail::error_code{ ::GetLastError() };
      }

      // HTTPバージョンの設定
      {
        using enum cfg::http_version;

        switch (version)
        {
        case http2:
        {
          // HTTP2を常に使用する
          DWORD http2_opt = WINHTTP_PROTOCOL_FLAG_HTTP2;
          http_ver_str = L"HTTP/2";
          if (not ::WinHttpSetOption(session.get(), WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &http2_opt, sizeof(http2_opt))) {
            return detail::error_code{ ::GetLastError() };
          }
          break;
        }
        /*case http3 :
          {
            // HTTP3を常に使用する
            DWORD http3_opt = WINHTTP_PROTOCOL_FLAG_HTTP3;
            http_ver_str = L"HTTP/3";
            if (not ::WinHttpSetOption(session.get(), WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &http3_opt, sizeof(http3_opt))) {
              return ::GetLastError();
            }
            break;
          }*/
        case http1_1:
          // 1.1はデフォルト
          http_ver_str = L"HTTP/1.1";
          break;
        default:
          std::unreachable();
        }
      }

      // ドメイン部分までのURL分解
      url_component = { .dwStructSize = sizeof(::URL_COMPONENTS), .dwSchemeLength = (DWORD)-1, .dwHostNameLength = (DWORD)-1, .dwUserNameLength = (DWORD)-1, .dwPasswordLength = (DWORD)-1, .dwUrlPathLength = (DWORD)-1, .dwExtraInfoLength = (DWORD)-1 };

      if (not ::WinHttpCrackUrl(url.data(), static_cast<DWORD>(url.length()), 0, &url_component)) {
        return detail::error_code{ ::GetLastError() };
      }

      // null終端をするためにwstringに移す
      host_name = { url_component.lpszHostName, url_component.dwHostNameLength };

      // 接続ハンドルの取得
      connect.reset(::WinHttpConnect(session.get(), host_name.c_str(), url_component.nPort, 0));

      if (not connect) {
        return detail::error_code{ ::GetLastError() };
      }

      return detail::error_code{};
    }

    auto init(std::wstring_view url_head, underlying::agent_impl::dummy_buffer, const detail::config::agent_initial_config& init_cfg) & -> detail::error_code {
      return this->init(url_head, init_cfg.proxy, init_cfg.timeout, init_cfg.version);
    }

    auto init(std::string_view url_head, detail::wstring_buffer& cnv_buf, const detail::config::agent_initial_config& init_cfg) & -> detail::error_code {
      return cnv_buf.use([&, this](wstring_t& converted_url) {
          if (char_to_wchar(url_head, converted_url) == false) {
            return this->init(converted_url, init_cfg.proxy, init_cfg.timeout, init_cfg.version);
          } else {
            return detail::error_code{ ::GetLastError() };
          }
        });
    }
  };
}

namespace chttpp::underlying::terse {

  using namespace chttpp::underlying;


  template<typename MethodTag>
  auto request_impl(winhttp_session_state&& state, auto&& conf, [[maybe_unused]] std::span<const char> req_body, MethodTag) -> http_result {
    // メソッドタイプ判定
    constexpr bool has_request_body = detail::tag::has_reqbody_method<MethodTag>;

    // メソッド判定
    constexpr bool is_get = std::is_same_v<detail::tag::get_t, MethodTag>;
    constexpr bool is_head = std::is_same_v<detail::tag::head_t, MethodTag>;
    constexpr bool is_opt = std::is_same_v<detail::tag::options_t, MethodTag>;
    constexpr bool is_trace = std::is_same_v<detail::tag::trace_t, MethodTag>;
    constexpr bool is_post = std::is_same_v<detail::tag::post_t, MethodTag>;
    constexpr bool is_put = std::is_same_v<detail::tag::put_t, MethodTag>;
    constexpr bool is_del = std::is_same_v<detail::tag::delete_t, MethodTag>;

    auto& connect = state.connect;
    auto& url_component = state.url_component;
    auto& http_ver_str = state.http_ver_str;

    // パラメータを設定したパスとクエリの構成
    const auto path_and_query = build_path_and_query({ url_component.lpszUrlPath, url_component.dwUrlPathLength }, { url_component.lpszExtraInfo, url_component.dwExtraInfoLength }, conf.params);

    // httpsの時だけWINHTTP_FLAG_SECUREを設定する（こうしないとWinHttpSendRequestでコケる）
    const DWORD openreq_flag = ((url_component.nPort == 80) ? 0 : WINHTTP_FLAG_SECURE) | WINHTTP_FLAG_ESCAPE_PERCENT | WINHTTP_FLAG_REFRESH;
    hinet request{};
    if constexpr (is_get) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"GET", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_head) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"HEAD", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_opt) {
      // OPTIONSリクエストの対象を指定する
      LPCWSTR target = (url_component.dwUrlPathLength == 0) ? L"*" : url_component.lpszUrlPath;
      request.reset(::WinHttpOpenRequest(connect.get(), L"OPTIONS", target, http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_trace) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"TRACE", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_post) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"POST", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_put) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"PUT", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_del) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"DELETE", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else {
      static_assert([] { return false; }(), "not implemented.");
    }

    if (not request) {
      return http_result{ ::GetLastError() };
    }

    // 認証情報の取得とセット
    if (not common_authentication_setting(request.get(), conf.auth, url_component, state.buffer, WINHTTP_AUTH_TARGET_SERVER)) {
      return http_result{ ::GetLastError() };
    }

    // プロクシ認証情報の設定
    if (not conf.proxy.address.empty()) {
      if (not proxy_authentication_setting(request.get(), conf.proxy.auth, state.buffer)) {
        return http_result{ ::GetLastError() };
      }
    }

    if constexpr (has_request_body or is_get or is_opt) {
      // レスポンスデータを自動で解凍する
      DWORD auto_decomp_opt = WINHTTP_DECOMPRESSION_FLAG_ALL;
      if (not ::WinHttpSetOption(request.get(), WINHTTP_OPTION_DECOMPRESSION, &auto_decomp_opt, sizeof(auto_decomp_opt))) {
        return http_result{ ::GetLastError() };
      }
    }

    auto& headers = conf.headers;

    if constexpr (has_request_body) {
      // Content-Typeヘッダの追加
      // ヘッダ指定されたものを優先する
      std::input_or_output_iterator auto i = std::ranges::find_if(headers, [](auto name) { return name == "Content-Type" or name == "content-type"; }, &std::pair<std::string_view, std::string_view>::first);
      if (i == headers.end()) {
        headers.emplace_back("content-type", conf.content_type);
      }
      // この時点で、headersは空ではない
    }

    LPCWSTR add_header = WINHTTP_NO_ADDITIONAL_HEADERS;
    wstring_t send_header_buf;

#pragma warning(push)
#pragma warning(disable:4127)
    if (has_request_body or not headers.empty()) {
#pragma warning(pop)
      // ヘッダ名+ヘッダ値+デリミタ（2文字）+\r\n（2文字）
      const std::size_t total_len = std::transform_reduce(headers.begin(), headers.end(), 0ull, std::plus<>{}, [](const auto& p) { return p.first.length() + p.second.length() + 2 + 2; });

      const bool failed = state.char_buf.use([&](string_t& tmp_buf) {
        tmp_buf.resize(total_len, '\0');

        auto pos = tmp_buf.begin();

        // ヘッダ文字列を連結して送信するヘッダ文字列を構成する
        for (auto& [name, value] : headers) {
          // name: value\r\n のフォーマット
          pos = std::format_to(pos, "{:s}: {:s}\r\n", name, value);
        }

        // まとめてWCHAR文字列へ文字コード変換
        return char_to_wchar(tmp_buf, send_header_buf);
      });

      if (failed) {
        return http_result{ ::GetLastError() };
      }

      add_header = send_header_buf.c_str();
    }

    // リクエスト送信とレスポンスの受信
    if constexpr (has_request_body) {
      if (not ::WinHttpSendRequest(request.get(), add_header, static_cast<DWORD>(send_header_buf.length()), const_cast<char*>(req_body.data()), static_cast<DWORD>(req_body.size_bytes()), static_cast<DWORD>(req_body.size_bytes()), 0) or
          not ::WinHttpReceiveResponse(request.get(), nullptr)) {
        return http_result{ ::GetLastError() };
      }
    } else {
      if (not ::WinHttpSendRequest(request.get(), add_header, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) or
          not ::WinHttpReceiveResponse(request.get(), nullptr)) {
        return http_result{ ::GetLastError() };
      }
    }

    // レスポンスデータの取得
    vector_t<char> body{};
    if constexpr (has_request_body or is_get or is_opt) {
      DWORD read_len{};
      std::size_t total_read{};

      // QueryDataAvailableもReadDataも少しづつ（8000バイトちょい）しか読み込んでくれないので、全部読み取るにはデータがなくなるまでループする
      // ここでの読み込みバイト数は解凍後のものなので、Content-Lengthの値とは異なりうる
      do {
        DWORD data_len{};
        if (not ::WinHttpQueryDataAvailable(request.get(), &data_len)) {
          return http_result{ ::GetLastError() };
        }

        // 実際にはここで止まるらしい
        if (data_len == 0) break;

        body.resize(total_read + data_len);

        // WinHttpReadDataが最初に0にセットするらしいよ
        //read_len = 0;
        if (not ::WinHttpReadData(request.get(), body.data() + total_read, data_len, &read_len)) {
          return http_result{ ::GetLastError() };
        }

        total_read += read_len;
      } while (0 < read_len);
    }

    // ステータスコードの取得
    DWORD status_code{};
    DWORD dowrd_len = sizeof(status_code);
    if (not ::WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &dowrd_len, WINHTTP_NO_HEADER_INDEX)) {
      return http_result{ ::GetLastError() };
    }

    // レスポンスヘッダの取得
    DWORD header_bytes{};
    ::WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &header_bytes, WINHTTP_NO_HEADER_INDEX);
    // 生ヘッダはUTF-16文字列として得られる（null終端されている）
    wstring_t header_buf;
    header_buf.resize(header_bytes / sizeof(wchar_t));
    ::WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, header_buf.data(), &header_bytes, WINHTTP_NO_HEADER_INDEX);

    // ヘッダの変換、レスポンスヘッダに非Ascii文字が無いものと仮定しない
    const std::size_t converted_len = ::WideCharToMultiByte(CP_ACP, 0, header_buf.data(), -1, nullptr, 0, nullptr, nullptr);
    string_t converted_header{};
    converted_header.resize(converted_len);
    if (::WideCharToMultiByte(CP_ACP, 0, header_buf.data(), -1, converted_header.data(), static_cast<int>(converted_len), nullptr, nullptr) == 0) {
      return http_result{ ::GetLastError() };
    }

    return http_result{ chttpp::detail::http_response{.body = std::move(body), .headers = chttpp::detail::parse_response_header_on_winhttp(converted_header), .status_code{status_code} } };
  }

  template<typename... Args>
  auto request_impl(std::wstring_view url, auto&& conf, Args&&... args) -> http_result {
    winhttp_session_state session_obj;

    if (auto ec = session_obj.init(url, conf.proxy, conf.timeout, conf.version); ec != ERROR_SUCCESS) {
      return http_result{ ec };
    }

    return request_impl(std::move(session_obj), std::move(conf), std::forward<Args>(args)...);
  }

  template<typename... Args>
  auto request_impl(std::string_view url, Args&&... args) -> http_result {

    const auto [converted_url, failed] = char_to_wchar(url);

    if (failed) {
      return http_result{ ::GetLastError() };
    }

    return request_impl(converted_url, std::forward<Args>(args)...);
  }

}

namespace chttpp::underlying::agent_impl {

  using namespace chttpp::underlying;

  using session_state = winhttp_session_state;

  template<>
  struct determin_buffer<char> {
    using type = detail::wstring_buffer;
  };

  template<>
  struct determin_buffer<wchar_t> {
    using type = dummy_buffer;
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

    // agentの状態詳細など（agentでしか使わないバッファなどはsession_stateに置かないようにする）
    winhttp_session_state state{};
    // URLの簡易チェックと抽出
    detail::url_info request_url;
    // URL変換用
    detail::string_buffer url_conv_buf;
    // クッキー並べ替え用
    //detail::vector_buffer<detail::cookie_ref> cookie_buf{};
    // クッキーヘッダを構成するためのバッファ
    detail::pinned_buffer<string_t> cookie_header;
    // ヘッダ構成用
    detail::pinned_buffer<umap_t<std::string_view, std::string_view, string_hash>> tmp_header;
    // ヘッダ文字列化用
    detail::pinned_buffer<wstring_t> send_header;
  };


  template<typename MethodTag>
  auto request_impl(std::wstring_view url_path, agent_resource& resource, detail::agent_request_config&& req_cfg, [[maybe_unused]] std::span<const char> req_body, MethodTag) -> http_result {
    // メソッドタイプ判定
    constexpr bool has_request_body = detail::tag::has_reqbody_method<MethodTag>;

    // メソッド判定
    constexpr bool is_get = std::is_same_v<detail::tag::get_t, MethodTag>;
    constexpr bool is_head = std::is_same_v<detail::tag::head_t, MethodTag>;
    constexpr bool is_opt = std::is_same_v<detail::tag::options_t, MethodTag>;
    constexpr bool is_trace = std::is_same_v<detail::tag::trace_t, MethodTag>;
    [[maybe_unused]]
    constexpr bool is_post = std::is_same_v<detail::tag::post_t, MethodTag>;
    [[maybe_unused]]
    constexpr bool is_put = std::is_same_v<detail::tag::put_t, MethodTag>;
    [[maybe_unused]]
    constexpr bool is_del = std::is_same_v<detail::tag::delete_t, MethodTag>;
    [[maybe_unused]]
    constexpr bool is_patch = std::is_same_v<detail::tag::patch_t, MethodTag>;

    auto& state = resource.state;

    // ホスト名がきちんとしていないとWinHttpCrackUrl()は分解してくれない（エラーコード 12006 で失敗する）
    hinet request{state.buffer.use([&](wstring_t& full_url) -> ::HINTERNET {
      // パス解析のためにURLの頭はでっちあげる
      full_url = L"http://example.com";

      // ホストとパスの間の/を補う
      if (url_path.front() != L'/') {
        full_url.append(1, L'/');
      }

      full_url.append(url_path);

      // pathの解析
      // URL_COMPONENTSは元の文字列の一部を参照するだけなので、full_urlはこれが使用される間は生存していなければならない
      ::URL_COMPONENTS url_path_component{ .dwStructSize = sizeof(::URL_COMPONENTS), .dwUrlPathLength = (DWORD)-1, .dwExtraInfoLength = (DWORD)-1 };

      if (not ::WinHttpCrackUrl(full_url.data(), static_cast<DWORD>(full_url.length()), 0, &url_path_component)) {
        return nullptr;
      }

      // パラメータを設定したパスとクエリの構成
      const auto path_and_query = build_path_and_query({ url_path_component.lpszUrlPath, url_path_component.dwUrlPathLength }, { url_path_component.lpszExtraInfo, url_path_component.dwExtraInfoLength }, req_cfg.params);

      // httpsの時だけWINHTTP_FLAG_SECUREを設定する（こうしないとWinHttpSendRequestでコケる）
      const DWORD openreq_flag = ((state.url_component.nPort == 80) ? 0 : WINHTTP_FLAG_SECURE) | WINHTTP_FLAG_ESCAPE_PERCENT | WINHTTP_FLAG_REFRESH;
      // ここでのWINHTTP_FLAG_REFRESHの使用は要検討

      // HTTPバージョン文字列
      auto& http_ver_str = state.http_ver_str;
      
      if constexpr (is_get) {
        return ::WinHttpOpenRequest(state.connect.get(), L"GET", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag);
      } else if constexpr (is_head) {
        return ::WinHttpOpenRequest(state.connect.get(), L"HEAD", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag);
      } else if constexpr (is_opt) {
        // OPTIONSリクエストの対象を指定する
        LPCWSTR target = (url_path_component.dwUrlPathLength == 0) ? L"*" : url_path_component.lpszUrlPath;
        return ::WinHttpOpenRequest(state.connect.get(), L"OPTIONS", target, http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag);
      } else if constexpr (is_trace) {
        return ::WinHttpOpenRequest(state.connect.get(), L"TRACE", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag);
      } else if constexpr (is_post) {
        return ::WinHttpOpenRequest(state.connect.get(), L"POST", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag);
      } else if constexpr (is_put) {
        return ::WinHttpOpenRequest(state.connect.get(), L"PUT", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag);
      } else if constexpr (is_del) {
        return ::WinHttpOpenRequest(state.connect.get(), L"DELETE", path_and_query.c_str(), http_ver_str.data(), WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag);
      } else {
        static_assert([] { return false; }(), "not implemented.");
      }
    })};

    if (not request) {
      return http_result{ ::GetLastError() };
    }

    // 認証情報の取得とセット
    if (not common_authentication_setting(request.get(), req_cfg.auth, state.url_component, state.buffer, WINHTTP_AUTH_TARGET_SERVER)) {
      return http_result{ ::GetLastError() };
    }

    // プロクシ認証情報の設定
    if (not resource.config.proxy.address.empty()) {
      if (not proxy_authentication_setting(request.get(), resource.config.proxy.auth, state.buffer)) {
        return http_result{ ::GetLastError() };
      }
    }

    if constexpr (has_request_body or is_get or is_opt) {
      if (resource.auto_decomp.enabled()) {
      // レスポンスデータを自動で解凍する
        // libcurlと異なりwinhttpの場合、ここの設定はユーザーのヘッダ指定を上書きする
        // 従って、Accept-Encodingの指定と自動解凍設定を分離できない・・・
      DWORD auto_decomp_opt = WINHTTP_DECOMPRESSION_FLAG_ALL;
      if (not ::WinHttpSetOption(request.get(), WINHTTP_OPTION_DECOMPRESSION, &auto_decomp_opt, sizeof(auto_decomp_opt))) {
        return http_result{ ::GetLastError() };
      }
    }
    }

    if (resource.follow_redirect == follow_redirects::disable) {
      DWORD disable_redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
      // 失敗してもスルーで（いいかな？
      ::WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &disable_redirect, sizeof(disable_redirect));
    }

    // クッキー管理が無効化されていても、クッキーの送信は行う
    if (resource.cookie_management.enabled()) {
      // 期限切れクッキー削除
      resource.cookie_vault.remove_expired_cookies();
    }

    // cookieヘッダの構成、ヘッダ全体が文字列化されるまで保持する必要がある
    const auto [cookie_header, token] = resource.cookie_header.pin([&](string_t& cookie_header_str) {

      // 並べ替え
      vector_t<detail::cookie_ref> sorted_cookie;
      resource.cookie_vault.create_cookie_list_to(sorted_cookie, req_cfg.cookies, resource.request_url);

      for (auto& cookie : sorted_cookie) {
        cookie_header_str.append(cookie.name());
        cookie_header_str.append("=");
        cookie_header_str.append(cookie.value());
        cookie_header_str.append("; ");
      }

      if (not cookie_header_str.empty()) {
        // 末尾の"; "を削除
        cookie_header_str.pop_back();
        cookie_header_str.pop_back();
      }

      return std::string_view{cookie_header_str};
    });

    // ヘッダを構成、文字列化されるまで保持
    const auto [send_headers, token2] = resource.tmp_header.pin([&](auto& tmp_header) {

      tmp_header.insert(resource.headers.begin(), resource.headers.end());

      // まずヘッダをマージする
      // agent本体設定を上書きする形でリクエスト時ヘッダを指定
      for (const auto& [key, value] : req_cfg.headers) {
        tmp_header.insert_or_assign(key, value);
      }

      if (not cookie_header.empty()) {
        // cookieヘッダは上書き（これでよい？
        tmp_header["cookie"] = cookie_header;
      }

      if constexpr (has_request_body) {
        // Content-Typeヘッダの追加
        const bool content_type_exists = tmp_header.contains("content-type") or tmp_header.contains("Content-Type");

        // ヘッダ指定されたものを優先する
        if (not content_type_exists) {
          tmp_header.emplace("content-type", req_cfg.content_type);
        }
      }

      return std::views::all(tmp_header);
    });

    // ヘッダ文字列化中の文字コード変換の失敗を検知
    bool headerstr_convert_failed = false;

    // ヘッダを文字列化、送信完了まで保持
    const auto [send_header_str, token3] = resource.send_header.pin([&](wstring_t& send_header_buf) {

#pragma warning(push)
#pragma warning(disable:4127)
      if (has_request_body or (not send_headers.empty())) {
#pragma warning(pop)
        // ヘッダ名+ヘッダ値+デリミタ（2文字）+\r\n（2文字）
        const std::size_t total_len = std::transform_reduce(send_headers.begin(), send_headers.end(), 0ull, std::plus<>{}, [](const auto& p) { return p.first.length() + p.second.length() + 2 + 2; });

        state.char_buf.use([&](string_t& tmp_buf) {
          tmp_buf.resize(total_len, '\0');

          auto pos = tmp_buf.begin();

          // ヘッダ文字列を連結して送信するヘッダ文字列を構成する
          for (auto& [name, value] : send_headers) {
            // name: value\r\n のフォーマット
            pos = std::format_to(pos, "{:s}: {:s}\r\n", name, value);
          }

          // まとめてWCHAR文字列へ文字コード変換
          headerstr_convert_failed = char_to_wchar(tmp_buf, send_header_buf);
        });
      }

      return std::wstring_view{send_header_buf};
    });

    // ヘッダ文字列化中の変換エラー
    if (headerstr_convert_failed) {
      return http_result{ ::GetLastError() };
    }

    // リクエスト送信とレスポンスの受信
    if constexpr (has_request_body) {
      if (not ::WinHttpSendRequest(request.get(), send_header_str.data(), (DWORD)-1, const_cast<char*>(req_body.data()), static_cast<DWORD>(req_body.size_bytes()), static_cast<DWORD>(req_body.size_bytes()), 0) or
          not ::WinHttpReceiveResponse(request.get(), nullptr)) {
        return http_result{ ::GetLastError() };
      }
    } else {
      if (not ::WinHttpSendRequest(request.get(), send_header_str.data(), (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) or
          not ::WinHttpReceiveResponse(request.get(), nullptr)) {
        return http_result{ ::GetLastError() };
      }
    }
    
    // レスポンスデータの取得
    vector_t<char> body{};
    if constexpr (has_request_body or is_get or is_opt) {
      DWORD read_len{};
      std::size_t total_read{};

      // QueryDataAvailableもReadDataも少しづつ（8000バイトちょい）しか読み込んでくれないので、全部読み取るにはデータがなくなるまでループする
      // ここでの読み込みバイト数は解凍後のものなので、Content-Lengthの値とは異なりうる
      do {
        DWORD data_len{};
        if (not ::WinHttpQueryDataAvailable(request.get(), &data_len)) {
          return http_result{ ::GetLastError() };
        }

        // 実際にはここで止まるらしい
        if (data_len == 0) break;

        body.resize(total_read + data_len);

        // WinHttpReadDataが最初に0にセットするらしいよ
        //read_len = 0;
        if (not ::WinHttpReadData(request.get(), body.data() + total_read, data_len, &read_len)) {
          return http_result{ ::GetLastError() };
        }

        total_read += read_len;
      } while (0 < read_len);
    }

    // ステータスコードの取得
    DWORD status_code{};
    DWORD dowrd_len = sizeof(status_code);
    if (not ::WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &dowrd_len, WINHTTP_NO_HEADER_INDEX)) {
      return http_result{ ::GetLastError() };
    }

    // レスポンスヘッダの取得
    DWORD header_bytes{};
    ::WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &header_bytes, WINHTTP_NO_HEADER_INDEX);
    // 生ヘッダはUTF-16文字列として得られる（null終端されている）
    // char文字列へ変換して返す（利便性及び一貫性のため）

    return detail::use_multiple_buffer([&](wstring_t& header_buf, string_t& converted_header) {
      header_buf.resize(header_bytes / sizeof(wchar_t));
      ::WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, header_buf.data(), &header_bytes, WINHTTP_NO_HEADER_INDEX);

      // ヘッダの変換、レスポンスヘッダに非Ascii文字が無いものと仮定しない
      const std::size_t converted_len = ::WideCharToMultiByte(CP_ACP, 0, header_buf.data(), -1, nullptr, 0, nullptr, nullptr);

      converted_header.resize(converted_len);

      if (::WideCharToMultiByte(CP_ACP, 0, header_buf.data(), -1, converted_header.data(), static_cast<int>(converted_len), nullptr, nullptr) == 0) {
        return http_result{ ::GetLastError() };
      }

      // ヘッダの分割
      auto headers = chttpp::detail::parse_response_header_on_winhttp(converted_header);

      if (resource.cookie_management.enabled()) {
        // クッキーの取り出し
        if (const auto pos = headers.find("set-cookie"); pos != headers.end()) {
          resource.cookie_vault.insert_from_set_cookie((*pos).second, resource.request_url.host());
        }
      }

      return http_result{ chttpp::detail::http_response{.body = std::move(body), .headers = std::move(headers), .status_code{status_code}}};
    }, state.buffer, state.char_buf);
  }

  template<typename... Args>
  auto request_impl(std::wstring_view url_path, dummy_buffer, agent_resource& resource, Args&&... args) -> http_result {
    return resource.url_conv_buf.use([](string_t& path) {
      // 追加するパスの変換
      if (wchar_to_char(url_path, path)) {
        return http_result{ ::GetLastError() };
      }

      // URL情報の構成
      [[maybe_unused]]
      auto pinning_fullurl = resource.request_url.append_path(path);

      // bufferをはがすだけ
      return request_impl(url_path, resource, std::forward<Args>(args)...);
    });

  }

  template<typename... Args>
  auto request_impl(std::string_view url_path, detail::wstring_buffer& buffer, agent_resource& resource, Args&&... args) -> http_result {
    // path文字列をwchar_tへ変換する
    return buffer.use([&](wstring_t& converted_url) {
        // URL情報の構成
        [[maybe_unused]]
        auto pinning_fullurl = resource.request_url.append_path(url_path);

        if (char_to_wchar(url_path, converted_url)) {
          return http_result{ ::GetLastError() };
        }

        return request_impl(converted_url, resource, std::forward<Args>(args)...);
      });
  }

}