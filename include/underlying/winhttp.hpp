#pragma once

#include <string_view>
#include <memory>
#include <cassert>
#include <iostream>
#include <memory_resource>
#include <algorithm>
#include <functional>

#define WIN32_LEAN_AND_MEAN
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
    const std::_System_error_message _Msg(static_cast<unsigned long>(winec_to_hresult));
    if (_Msg._Length == 0) {
      static constexpr char _Unknown_error[] = "unknown error";
      constexpr size_t _Unknown_error_length = sizeof(_Unknown_error) - 1;
      return string_t(_Unknown_error, _Unknown_error_length);
    } else {
      return string_t(_Msg._Str, _Msg._Length);
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


  auto request_impl(std::wstring_view url, detail::tag::get_t) -> http_result {

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

    hinet connect{ ::WinHttpConnect(session.get(), url_component.lpszHostName, url_component.nPort, 0) };

    if (not connect) {
      return http_result{ ::GetLastError() };
    }

    // httpsの時だけWINHTTP_FLAG_SECUREを設定する（こうしないとWinHttpSendRequestでコケる）
    const DWORD openreq_flag = ((url_component.nPort == 80) ? 0 : WINHTTP_FLAG_SECURE) | WINHTTP_FLAG_REFRESH;
    hinet request{ ::WinHttpOpenRequest(connect.get(), L"GET", url_component.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag) };

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

    if (not ::WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) or
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

  auto request_impl(std::wstring_view url, detail::tag::head_t) -> http_result {

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

    hinet connect{ ::WinHttpConnect(session.get(), url_component.lpszHostName, url_component.nPort, 0) };

    if (not connect) {
      return http_result{ ::GetLastError() };
    }

    // httpsの時だけWINHTTP_FLAG_SECUREを設定する（こうしないとWinHttpSendRequestでコケる）
    const DWORD openreq_flag = ((url_component.nPort == 80) ? 0 : WINHTTP_FLAG_SECURE) | WINHTTP_FLAG_REFRESH;
    hinet request{ ::WinHttpOpenRequest(connect.get(), L"HEAD", url_component.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag) };

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

    if (not ::WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) or
      not ::WinHttpReceiveResponse(request.get(), nullptr)) {
      return http_result{ ::GetLastError() };
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

    return http_result{ chttpp::detail::http_response{.body = {}, .headers = chttpp::detail::parse_response_header_on_winhttp(converted_header), .status_code = static_cast<std::uint16_t>(status_code)} };
  }

  auto request_impl(std::wstring_view url, detail::tag::options_t) -> http_result {

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

    hinet connect{ ::WinHttpConnect(session.get(), url_component.lpszHostName, url_component.nPort, 0) };

    if (not connect) {
      return http_result{ ::GetLastError() };
    }

    // httpsの時だけWINHTTP_FLAG_SECUREを設定する（こうしないとWinHttpSendRequestでコケる）
    const DWORD openreq_flag = ((url_component.nPort == 80) ? 0 : WINHTTP_FLAG_SECURE) | WINHTTP_FLAG_REFRESH;
    // OPTIONSリクエストの対象を指定する
    LPCWSTR target = (url_component.dwUrlPathLength == 0) ? L"*" : url_component.lpszUrlPath;
    hinet request{ ::WinHttpOpenRequest(connect.get(), L"OPTIONS", target, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag) };

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

    if (not ::WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) or
      not ::WinHttpReceiveResponse(request.get(), nullptr)) {
      return http_result{ ::GetLastError() };
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

    return http_result{ chttpp::detail::http_response{.body = {}, .headers = chttpp::detail::parse_response_header_on_winhttp(converted_header), .status_code = static_cast<std::uint16_t>(status_code)} };
  }

  auto request_impl(std::wstring_view url, detail::tag::trace_t) -> http_result {

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

    hinet connect{ ::WinHttpConnect(session.get(), url_component.lpszHostName, url_component.nPort, 0) };

    if (not connect) {
      return http_result{ ::GetLastError() };
    }

    // httpsの時だけWINHTTP_FLAG_SECUREを設定する（こうしないとWinHttpSendRequestでコケる）
    const DWORD openreq_flag = ((url_component.nPort == 80) ? 0 : WINHTTP_FLAG_SECURE) | WINHTTP_FLAG_REFRESH;
    hinet request{ ::WinHttpOpenRequest(connect.get(), L"TRACE", url_component.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, openreq_flag) };

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

    if (not ::WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) or
      not ::WinHttpReceiveResponse(request.get(), nullptr)) {
      return http_result{ ::GetLastError() };
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

    return http_result{ chttpp::detail::http_response{.body = {}, .headers = chttpp::detail::parse_response_header_on_winhttp(converted_header), .status_code = static_cast<std::uint16_t>(status_code)} };
  }


  auto char_to_wchar(std::string_view cstr) -> std::pair<wstring_t, int> {
    const std::size_t converted_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, cstr.data(), static_cast<int>(cstr.length()), nullptr, 0);
    wstring_t converted_str{};
    converted_str.resize(converted_len);
    int res = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, cstr.data(), static_cast<int>(cstr.length()), converted_str.data(), static_cast<int>(converted_len));

    return { std::move(converted_str), res };
  }

  template<typename MethodTag>
  auto request_impl(std::string_view url, MethodTag tag) -> http_result {

    const auto [converted_url, res] = char_to_wchar(url);

    if (res == 0) {
      return http_result{ ::GetLastError() };
    }

    return request_impl(converted_url, tag);
  }
}