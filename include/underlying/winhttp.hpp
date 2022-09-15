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

#include "common.hpp"

namespace chttpp {
  using http_result = detail::basic_result<DWORD>;

  template<>
  inline auto http_result::error_to_string() const -> string_t {
    // pmr::stringに対応するために内部実装を直接使用
    // 詳細 : https://github.com/microsoft/STL/blob/main/stl/inc/system_error
    //return std::system_category().message(HRESULT_FROM_WIN32(std::get<1>(this->m_either)));
    const auto winec_to_hresult = HRESULT_FROM_WIN32(std::get<1>(this->m_either));
    const std::_System_error_message message(static_cast<unsigned long>(winec_to_hresult));
    if (message._Length == 0) {
      static constexpr char unknown_err[] = "unknown error";
      constexpr size_t unknown_err_length = sizeof(unknown_err) - 1;
      return string_t(unknown_err, unknown_err_length);
      /*
      return string_t{"unknown error"};
      のようにしないのは、文字列とその長さの計算を確実にコンパイル時に終わらせるためだと思われる
      文字列リテラルを与えると長さの計算が実行時になりうる
      また、この長さならSSOも期待できるため、std::stringの構築を固定長コピーに落とせる
      */
    } else {
      return string_t(message._Str, message._Length);
    }
  }

}

namespace chttpp::detail {

  inline auto parse_response_header_on_winhttp(std::string_view header_str) -> header_t {
    using namespace std::literals;

    header_t headers;
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

namespace chttpp::underlying::terse {

  struct hinet_deleter {
    using pointer = HINTERNET;

    void operator()(HINTERNET hi) const noexcept {
      WinHttpCloseHandle(hi);
    }
  };

  using hinet = std::unique_ptr<HINTERNET, hinet_deleter>;

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

  inline bool common_authentication_setting(HINTERNET req_handle, const detail::authorization_config& auth, const ::URL_COMPONENTS& url_component, wstring_t& auth_buf, int target) {
    std::wstring_view user, pw;

    // 設定で指定された方を優先
    if (not auth.username.empty()) {
      // ユーザー名とパスワードのwchar_t変換
      {
        // ユーザー名の長さ
        const std::size_t user_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, auth.username.data(), static_cast<int>(auth.username.length()), nullptr, 0);
        // パスワードの長さ
        const std::size_t pw_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, auth.password.data(), static_cast<int>(auth.password.length()), nullptr, 0);

        // \0を1つ含むようにリサイズ
        auth_buf.resize(user_len + pw_len + 1);

        // ユーザー名の変換
        int res_user = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, auth.username.data(), static_cast<int>(auth.username.length()), auth_buf.data(), static_cast<int>(user_len));
        // パスワードの変換
        int res_pw = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, auth.password.data(), static_cast<int>(auth.password.length()), auth_buf.data() + user_len + 1, static_cast<int>(pw_len));

        assert(res_user == user_len);
        assert(res_pw == pw_len);
        // パスワードは空でもいい、その場合0になる？
        assert(1ull < pw_len);

        auth_buf[user_len] = L'\0';

        // \0を含まない範囲を参照させる
        user = std::wstring_view{ auth_buf.data(), user_len };
        pw = std::wstring_view{ auth_buf.data() + user_len + 1, pw_len };
      }
    } else if (url_component.dwUserNameLength != 0) {
      // URLにユーザー名とパスワードが含まれている場合
      // null終端のためにいったんバッファにコピー

      // \0を1つ含むようにリサイズ
      auth_buf.reserve(static_cast<std::size_t>(url_component.dwUserNameLength) + url_component.dwPasswordLength + 1);
      auth_buf.append(url_component.lpszUserName, url_component.dwUserNameLength);
      auth_buf.append(1, L'\0');
      auth_buf.append(url_component.lpszPassword, url_component.dwPasswordLength);

      user = std::wstring_view{ auth_buf.data(), url_component.dwUserNameLength };
      pw = std::wstring_view{ auth_buf.data() + user.length() + 1, url_component.dwPasswordLength };
    } else {
      // 認証なし
      return true;
    }

    // パスワードは空でもいいらしい
    assert(not user.empty());

    // Basic認証以外の認証では、WinHttpSendRequest()の後に応答を確認しつつ設定し再送する必要がある
    // https://docs.microsoft.com/ja-jp/windows/win32/winhttp/authentication-in-winhttp

    const bool res = ::WinHttpSetCredentials(req_handle, target, WINHTTP_AUTH_SCHEME_BASIC, user.data(), pw.data(), nullptr) == TRUE;

    return res;
  }


  template<typename MethodTag>
    requires (not detail::tag::has_reqbody_method<MethodTag>)
  auto request_impl(std::wstring_view url, detail::request_config_for_get&& cfg, MethodTag) -> http_result {
    // メソッド判定
    constexpr bool is_get = std::same_as<detail::tag::get_t, MethodTag>;
    constexpr bool is_head = std::same_as<detail::tag::head_t, MethodTag>;
    constexpr bool is_opt = std::same_as<detail::tag::options_t, MethodTag>;
    constexpr bool is_trace = std::same_as<detail::tag::trace_t, MethodTag>;

    // Proxyの設定（アドレスのみ）と初期化
    wstring_t prxy_buf{};

    hinet session = [&]() -> hinet {
      if (cfg.proxy.url.empty()) {
        return hinet{ WinHttpOpen(detail::default_UA_w.data(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };
      } else {
        // WinHttpSetOptionでプロクシを設定する場合は、ここの第二引数をWINHTTP_ACCESS_TYPE_NO_PROXYにしておかなければならない
        // なぜここで設定しないのか？ -> ここでのプロクシはおそらく普通のhttpプロクシしか対応してない（ドキュメントではCERN type proxies for HTTPのみと指定されている
        //                          -> httpsをトンネルするタイプのプロクシを使用できないと思われる
        hinet hsession{ WinHttpOpen(detail::default_UA_w.data(), WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };

        // Proxyの設定（アドレスのみ）
        auto [converted_url, failed] = char_to_wchar(cfg.proxy.url);
        prxy_buf = std::move(converted_url);

        if (failed) {
          return nullptr;
        }

        ::WINHTTP_PROXY_INFO prxy_info{
          .dwAccessType = WINHTTP_ACCESS_TYPE_NAMED_PROXY,
          .lpszProxy = prxy_buf.data(),
          .lpszProxyBypass = const_cast<wchar_t*>(L"<local>") // ??
        };

        // プロクシアドレスの設定
        if (not ::WinHttpSetOption(hsession.get(), WINHTTP_OPTION_PROXY, &prxy_info, sizeof(::WINHTTP_PROXY_INFO))) {
          return nullptr;
        }

        // 暗黙ムーブ
        return hsession;
      }

      //std::unreachable();
    }();

    if (not session) {
      return http_result{ ::GetLastError() };
    }

    // タイムアウトの設定
    {
      const int timeout = static_cast<int>(cfg.timeout.count());

      // セッション全体ではなく各点でのタイムアウト指定になる
      // 最悪の場合、指定時間の4倍の時間待機することになる・・・？
      if (not ::WinHttpSetTimeouts(session.get(), timeout, timeout, timeout, timeout)) {
        return http_result{ ::GetLastError() };
      }
    }

    {
      // HTTP2を常に使用する
      DWORD http2_opt = WINHTTP_PROTOCOL_FLAG_HTTP2;
      if (not ::WinHttpSetOption(session.get(), WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &http2_opt, sizeof(http2_opt))) {
        return http_result{ ::GetLastError() };
      }
    }

    ::URL_COMPONENTS url_component{ .dwStructSize = sizeof(::URL_COMPONENTS), .dwSchemeLength = (DWORD)-1, .dwHostNameLength = (DWORD)-1, .dwUserNameLength = (DWORD)-1, .dwPasswordLength = (DWORD)-1, .dwUrlPathLength = (DWORD)-1, .dwExtraInfoLength = (DWORD)-1 };

    if (not ::WinHttpCrackUrl(url.data(), static_cast<DWORD>(url.length()), 0, &url_component)) {
      return http_result{ ::GetLastError() };
    }

    // WinHttpCrackUrlはポインタ位置を合わせてくれるだけで文字列をコピーしていない
    // したがって、バッファを指定しない場合は元の文字列のどこかを指しておりnull終端されていない
    wstring_t host_name(url_component.lpszHostName, url_component.dwHostNameLength);

    hinet connect{ ::WinHttpConnect(session.get(), host_name.c_str(), url_component.nPort, 0) };

    if (not connect) {
      return http_result{ ::GetLastError() };
    }

    // パラメータを設定したパスとクエリの構成
    const auto path_and_query = build_path_and_query({ url_component.lpszUrlPath, url_component.dwUrlPathLength }, { url_component.lpszExtraInfo, url_component.dwExtraInfoLength }, cfg.params);

    // httpsの時だけWINHTTP_FLAG_SECUREを設定する（こうしないとWinHttpSendRequestでコケる）
    const DWORD openreq_flag = ((url_component.nPort == 80) ? 0 : WINHTTP_FLAG_SECURE) | WINHTTP_FLAG_ESCAPE_PERCENT | WINHTTP_FLAG_REFRESH;
    hinet request{};
    if constexpr (is_get) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"GET", path_and_query.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_head) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"HEAD", path_and_query.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_opt) {
      // OPTIONSリクエストの対象を指定する
      LPCWSTR target = (url_component.dwUrlPathLength == 0) ? L"*" : url_component.lpszUrlPath;
      request.reset(::WinHttpOpenRequest(connect.get(), L"OPTIONS", target, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_trace) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"TRACE", path_and_query.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else {
      static_assert([] { return false; }(), "not implemented.");
    }

    if (not request) {
      return http_result{ ::GetLastError() };
    }

    // 認証情報の取得とセット
    wstring_t auth_buf{};
    if (not common_authentication_setting(request.get(), cfg.auth, url_component, auth_buf, WINHTTP_AUTH_TARGET_SERVER)) {
      return http_result{ ::GetLastError() };
    }

    // プロクシ認証情報の設定
    wstring_t prxy_auth_buf{};
    if (not cfg.proxy.url.empty()) {
      // プロクシアドレスからユーザー名とパスワード（あれば）を抽出
      ::URL_COMPONENTS prxy_url_component{ .dwStructSize = sizeof(::URL_COMPONENTS), .dwUserNameLength = (DWORD)-1, .dwPasswordLength = (DWORD)-1 };
      if (not ::WinHttpCrackUrl(prxy_buf.data(), static_cast<DWORD>(prxy_buf.length()), 0, &prxy_url_component)) {
        return http_result{ ::GetLastError() };
      }

      if (not common_authentication_setting(request.get(), cfg.proxy.auth, prxy_url_component, prxy_auth_buf, WINHTTP_AUTH_TARGET_PROXY)) {
        return http_result{ ::GetLastError() };
      }
    }

    if constexpr (is_get or is_opt) {
      // レスポンスデータを自動で解凍する
      DWORD auto_decomp_opt = WINHTTP_DECOMPRESSION_FLAG_ALL;
      if (not ::WinHttpSetOption(request.get(), WINHTTP_OPTION_DECOMPRESSION, &auto_decomp_opt, sizeof(auto_decomp_opt))) {
        return http_result{ ::GetLastError() };
      }
    }

    LPCWSTR add_header = WINHTTP_NO_ADDITIONAL_HEADERS;
    std::wstring send_header_buf{};
    auto& headers = cfg.headers;

    if (not headers.empty()) {

      // ヘッダ名+ヘッダ値+デリミタ（2文字）+\r\n（2文字）
      const std::size_t total_len = std::transform_reduce(headers.begin(), headers.end(), 0ull, std::plus<>{}, [](const auto& p) { return p.first.length() + p.second.length() + 2 + 2; });

      std::string tmp_buf(total_len, '\0');
      auto pos = tmp_buf.begin();

      // ヘッダ文字列を連結して送信するヘッダ文字列を構成する
      for (auto& [name, value] : headers) {
        // name: value\r\n のフォーマット
        pos = std::format_to(pos, "{:s}: {:s}\r\n", name, value);
      }

      // まとめてWCHAR文字列へ文字コード変換
      auto&& [wstr, failed] = char_to_wchar(tmp_buf);
      
      if (failed) {
        return http_result{ ::GetLastError() };
      }

      send_header_buf = std::move(wstr);
      add_header = send_header_buf.c_str();
    }

    // リクエスト送信とレスポンスの受信
    if (not ::WinHttpSendRequest(request.get(), add_header, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) or
        not ::WinHttpReceiveResponse(request.get(), nullptr)) {
      return http_result{ ::GetLastError() };
    }

    // レスポンスデータの取得
    vector_t<char> body{};
    if constexpr (is_get or is_opt) {
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

    return http_result{ chttpp::detail::http_response{.body = std::move(body), .headers = chttpp::detail::parse_response_header_on_winhttp(converted_header), .status_code = static_cast<std::uint16_t>(status_code)} };
  }

  template<detail::tag::has_reqbody_method Tag>
  auto request_impl(std::wstring_view url, std::span<const char> req_dody, detail::request_config&& cfg, Tag) -> http_result {

    constexpr bool is_post = std::same_as<Tag, detail::tag::post_t>;
    constexpr bool is_put = std::same_as<Tag, detail::tag::put_t>;
    constexpr bool is_del = std::same_as<Tag, detail::tag::delete_t>;

    hinet session{ WinHttpOpen(detail::default_UA_w.data(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0) };

    if (not session) {
      return http_result{ ::GetLastError() };
    }

    // タイムアウトの設定
    {
      const int timeout = static_cast<int>(cfg.timeout.count());

      // セッション全体ではなく各点でのタイムアウト指定になる
      // 最悪の場合、指定時間の4倍の時間待機することになる・・・？
      if (not ::WinHttpSetTimeouts(session.get(), timeout, timeout, timeout, timeout)) {
        return http_result{ ::GetLastError() };
      }
    }

    {
      // HTTP2を常に使用する
      DWORD http2_opt = WINHTTP_PROTOCOL_FLAG_HTTP2;
      if (not ::WinHttpSetOption(session.get(), WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &http2_opt, sizeof(http2_opt))) {
        return http_result{ ::GetLastError() };
      }
    }

    ::URL_COMPONENTS url_component{ .dwStructSize = sizeof(::URL_COMPONENTS), .dwHostNameLength = (DWORD)-1, .dwUrlPathLength = (DWORD)-1 };

    if (not ::WinHttpCrackUrl(url.data(), static_cast<DWORD>(url.length()), 0, &url_component)) {
      return http_result{ ::GetLastError() };
    }

    // WinHttpCrackUrlはポインタ位置を合わせてくれるだけで文字列をコピーしていない
    // したがって、バッファを指定しない場合は元の文字列のどこかを指しておりnull終端されていない
    wstring_t host_name(url_component.lpszHostName, url_component.dwHostNameLength);

    hinet connect{ ::WinHttpConnect(session.get(), host_name.c_str(), url_component.nPort, 0) };

    if (not connect) {
      return http_result{ ::GetLastError() };
    }

    // パラメータを設定したパスとクエリの構成
    const auto path_and_query = build_path_and_query({ url_component.lpszUrlPath, url_component.dwUrlPathLength }, { url_component.lpszExtraInfo, url_component.dwExtraInfoLength }, cfg.params);

    // httpsの時だけWINHTTP_FLAG_SECUREを設定する（こうしないとWinHttpSendRequestでコケる）
    const DWORD openreq_flag = ((url_component.nPort == 80) ? 0 : WINHTTP_FLAG_SECURE) | WINHTTP_FLAG_ESCAPE_PERCENT | WINHTTP_FLAG_REFRESH;
    hinet request{};
    if constexpr (is_post) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"POST", path_and_query.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_put) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"PUT", path_and_query.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_del) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"DELETE", path_and_query.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else {
      static_assert([] { return false; }(), "not implemented.");
    }

    if (not request) {
      return http_result{ ::GetLastError() };
    }

    // 認証情報の取得とセット
    wstring_t auth_buf{};
    if (not common_authentication_setting(request.get(), cfg.auth, url_component, auth_buf, WINHTTP_AUTH_TARGET_SERVER)) {
      return http_result{ ::GetLastError() };
    }

    {
      // レスポンスデータを自動で解凍する
      DWORD auto_decomp_opt = WINHTTP_DECOMPRESSION_FLAG_ALL;
      if (not ::WinHttpSetOption(request.get(), WINHTTP_OPTION_DECOMPRESSION, &auto_decomp_opt, sizeof(auto_decomp_opt))) {
        return http_result{ ::GetLastError() };
      }
    }

    auto& headers = cfg.headers;

    // Content-Typeヘッダの追加
    // ヘッダ指定されたものを優先する
    {
      std::input_or_output_iterator auto i = std::ranges::find_if(headers, 
                                                                  [](const auto& header) { return header == "Content-Type" or header == "content-type"; },
                                                                  &std::pair<std::string_view, std::string_view>::first);
      if (i == headers.end()) {
        headers.emplace_back("Content-Type", cfg.content_type);
      }
    }

    // ヘッダ名+ヘッダ値+デリミタ（2文字）+\r\n（2文字）
    const std::size_t total_len = std::transform_reduce(headers.begin(), headers.end(), 0ull, std::plus<>{}, [](const auto& p) { return p.first.length() + p.second.length() + 2 + 2; });

    std::string tmp_buf(total_len, '\0');
    auto pos = tmp_buf.begin();

    // ヘッダ文字列を連結して送信するヘッダ文字列を構成する
    for (auto& [name, value] : headers) {
      // name: value\r\n のフォーマット
      pos = std::format_to(pos, "{:s}: {:s}\r\n", name, value);
    }

    // まとめてWCHAR文字列へ文字コード変換
    auto&& [send_header_buf, failed] = char_to_wchar(tmp_buf);

    if (failed) {
      return http_result{ ::GetLastError() };
    }

    // リクエストの送信（送信とは言ってない）
    if (not ::WinHttpSendRequest(request.get(), send_header_buf.c_str(), static_cast<DWORD>(send_header_buf.length()), const_cast<char*>(req_dody.data()), static_cast<DWORD>(req_dody.size_bytes()), static_cast<DWORD>(req_dody.size_bytes()), 0) or
        not ::WinHttpReceiveResponse(request.get(), nullptr)) {
      return http_result{ ::GetLastError() };
    }

    // レスポンスデータの取得
    DWORD data_len{};
    if (not ::WinHttpQueryDataAvailable(request.get(), &data_len)) {
      return http_result{ ::GetLastError() };
    }

    vector_t<char> body{};
    if (0 < data_len) {
      body.resize(data_len);
      DWORD read_len{};

      if (not ::WinHttpReadData(request.get(), body.data(), data_len, &read_len)) {
        return http_result{ ::GetLastError() };
      }
      assert(read_len == data_len);
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

    return http_result{ chttpp::detail::http_response{.body = std::move(body), .headers = chttpp::detail::parse_response_header_on_winhttp(converted_header), .status_code = static_cast<std::uint16_t>(status_code)} };
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