#pragma once

#include <string_view>
#include <memory>
#include <cassert>
#include <iostream>
#include <memory_resource>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")

#include "common.hpp"

namespace chttpp {
  using http_result = detail::basic_result<DWORD>;

  template<>
  inline auto http_result::error_to_string() const -> std::pmr::string {
    // pmr::stringに対応するために内部実装を直接使用
    return std::_Syserror_map(std::get<1>(this->m_either));
    //return std::system_category().message(std::get<1>(this->m_either));
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


  auto get_impl(std::wstring_view url) -> http_result {

    hinet session{ WinHttpOpen(L"Mozilla/5.0 Test", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_NAME, 0) };

    if (not session) {
      return { ::GetLastError(), 0 };
    }

    ::URL_COMPONENTS url_component{ .dwStructSize = sizeof(::URL_COMPONENTS), .dwHostNameLength = (DWORD)-1, .dwUrlPathLength = (DWORD)-1};

    if (not ::WinHttpCrackUrl(url.data(), static_cast<DWORD>(url.length()), 0, &url_component)) {
      return { ::GetLastError(), 0 };
    }

    hinet connect{ ::WinHttpConnect(session.get(), url_component.lpszHostName, url_component.nPort, 0) };

    if (not connect) {
      return { ::GetLastError(), 0 };
    }

    hinet request{ ::WinHttpOpenRequest(connect.get(), L"GET", L"/", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH) };

    if (not request) {
      return { ::GetLastError(), 0 };
    }

    if (not ::WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) or
        not ::WinHttpReceiveResponse(request.get(), nullptr)) {
      return { ::GetLastError(), 0 };
    }

    DWORD data_len{};
    if (not ::WinHttpQueryDataAvailable(request.get(), &data_len)) {
      return { ::GetLastError(), 0 };
    }

    std::pmr::vector<char> body{};
    body.resize(data_len);
    DWORD read_len{};

    if (not ::WinHttpReadData(request.get(), body.data(), data_len, &read_len)) {
      return { ::GetLastError(), 0 };
    }

    DWORD status_code{};
    DWORD dowrd_len = sizeof(status_code);
    if (not ::WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &dowrd_len, WINHTTP_NO_HEADER_INDEX)) {
      return { ::GetLastError(), 0 };
    }

    return { chttpp::detail::http_response{.body = std::move(body), .headers = {}} , static_cast<std::uint16_t>(status_code) };
  }

  auto get_impl(std::string_view url) -> http_result {
    const std::size_t converted_len = ::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, url.data(), static_cast<int>(url.length()), nullptr, 0);
    std::pmr::wstring converted_url{};
    converted_url.resize(converted_len);

    if (::MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, url.data(), static_cast<int>(url.length()), converted_url.data(), static_cast<int>(converted_len)) == 0) {
      return { ::GetLastError(), 0 };
    }

    return get_impl(converted_url);
  }
}