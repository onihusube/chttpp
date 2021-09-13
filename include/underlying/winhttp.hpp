#pragma once

#include <string_view>
#include <memory>
#include <cassert>
#include <iostream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")

namespace chttpp {

  struct hinet_deleter {
    using pointer = HINTERNET;

    void operator()(HINTERNET hi) const noexcept {
      WinHttpCloseHandle(hi);
    }
  };

  using hinet = std::unique_ptr<HINTERNET, hinet_deleter>;


  auto err_msg(HRESULT hr) {
    return std::system_category().message(hr);
  }

  auto err_msg(DWORD ec) {
    return err_msg(HRESULT_FROM_WIN32(ec));
  }


  auto test_get(std::wstring_view url) {

    hinet session{ WinHttpOpen(L"Mozilla/5.0 Test", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_NAME, 0) };

    if (session) {
      auto errc = ::GetLastError();

      std::cout << err_msg(errc) << std::endl;

      std::exit(errc);
    }

    ::URL_COMPONENTS url_component{ .dwStructSize = sizeof(::URL_COMPONENTS), .dwHostNameLength = (DWORD)-1, .dwUrlPathLength = (DWORD)-1};

    if (not ::WinHttpCrackUrl(url.data(), static_cast<DWORD>(url.length()), 0, &url_component)) {
      auto errc = ::GetLastError();

      std::cout << err_msg(errc) << std::endl;

      std::exit(errc);
    }

    hinet connect{ ::WinHttpConnect(session.get(), url_component.lpszHostName, url_component.nPort, 0) };

    if (connect) {
      auto errc = ::GetLastError();

      std::cout << err_msg(errc) << std::endl;

      std::exit(errc);
    }


  }
}