#pragma once

#include <string_view>
#include <memory>
#include <cassert>
#include <iostream>
#include <memory_resource>
#include <algorithm>
#include <functional>
#include <format>

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


  template<typename MethodTag>
    requires (not detail::tag::has_reqbody_method<MethodTag>)
  auto request_impl(std::wstring_view url, const vector_t<std::pair<std::string_view, std::string_view>>& headers, MethodTag) -> http_result {
    // メソッド判定
    constexpr bool is_get = std::same_as<detail::tag::get_t, MethodTag>;
    constexpr bool is_head = std::same_as<detail::tag::head_t, MethodTag>;
    constexpr bool is_opt = std::same_as<detail::tag::options_t, MethodTag>;
    constexpr bool is_trace = std::same_as<detail::tag::trace_t, MethodTag>;

    hinet session{ WinHttpOpen(L"Mozilla/5.0 Test", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_NAME, 0) };

    if (not session) {
      return http_result{ ::GetLastError() };
    }

    {
      // HTTP2を常に使用する
      DWORD http2_opt = WINHTTP_PROTOCOL_FLAG_HTTP2;
      if (not ::WinHttpSetOption(session.get(), WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &http2_opt, sizeof(http2_opt))) {
        return http_result{ ::GetLastError() };
      }
    }

    ::URL_COMPONENTS url_component{ .dwStructSize = sizeof(::URL_COMPONENTS), .dwHostNameLength = (DWORD)-1, .dwUrlPathLength = (DWORD)-1};

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

    // httpsの時だけWINHTTP_FLAG_SECUREを設定する（こうしないとWinHttpSendRequestでコケる）
    const DWORD openreq_flag = ((url_component.nPort == 80) ? 0 : WINHTTP_FLAG_SECURE) | WINHTTP_FLAG_REFRESH;
    hinet request{};
    if constexpr (is_get) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"GET", url_component.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_head) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"HEAD", url_component.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_opt) {
      // OPTIONSリクエストの対象を指定する
      LPCWSTR target = (url_component.dwUrlPathLength == 0) ? L"*" : url_component.lpszUrlPath;
      request.reset(::WinHttpOpenRequest(connect.get(), L"OPTIONS", target, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else if constexpr (is_trace) {
      request.reset(::WinHttpOpenRequest(connect.get(), L"TRACE", url_component.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag));
    } else {
      static_assert([] { return false; }(), "not implemented.");
    }

    if (not request) {
      return http_result{ ::GetLastError() };
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

    if (not headers.empty()) {
      std::string tmp_buf{};

      // ヘッダ文字列を連結して送信するヘッダ文字列を構成する
      for (auto& [name, value] : headers) {
        // name: value\r\n のフォーマット
        tmp_buf += std::format("{:s}: {:s}\r\n", name, value);
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
      DWORD data_len{};
      if (not ::WinHttpQueryDataAvailable(request.get(), &data_len)) {
        return http_result{ ::GetLastError() };
      }

      if (0 < data_len) {
        body.resize(data_len);
        DWORD read_len{};

        if (not ::WinHttpReadData(request.get(), body.data(), data_len, &read_len)) {
          return http_result{ ::GetLastError() };
        }
        assert(read_len == data_len);
      }
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

  auto request_impl(std::wstring_view url, [[maybe_unused]] std::string_view mime, std::span<const char> req_dody, const vector_t<std::pair<std::string_view, std::string_view>>& headers, detail::tag::post_t) -> http_result {

    hinet session{ WinHttpOpen(L"Mozilla/5.0 Test", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_NAME, 0) };

    if (not session) {
      return http_result{ ::GetLastError() };
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

    // httpsの時だけWINHTTP_FLAG_SECUREを設定する（こうしないとWinHttpSendRequestでコケる）
    const DWORD openreq_flag = ((url_component.nPort == 80) ? 0 : WINHTTP_FLAG_SECURE) | WINHTTP_FLAG_REFRESH;
    hinet request{ ::WinHttpOpenRequest(connect.get(), L"POST", url_component.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag) };

    if (not request) {
      return http_result{ ::GetLastError() };
    }

    {
      // レスポンスデータを自動で解凍する
      DWORD auto_decomp_opt = WINHTTP_DECOMPRESSION_FLAG_ALL;
      if (not ::WinHttpSetOption(request.get(), WINHTTP_OPTION_DECOMPRESSION, &auto_decomp_opt, sizeof(auto_decomp_opt))) {
        return http_result{ ::GetLastError() };
      }
    }

    // リクエストの送信（送信とは言ってない）
    constexpr const std::wstring_view add_headers = L"Content-Type: text/plain\r\n";
    if (not ::WinHttpSendRequest(request.get(), add_headers.data(), static_cast<DWORD>(add_headers.length()), const_cast<char*>(req_dody.data()), static_cast<DWORD>(req_dody.size_bytes()), static_cast<DWORD>(req_dody.size_bytes()), 0) or
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