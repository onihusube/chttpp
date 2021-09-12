#pragma once

#include <string_view>
#include <memory>
#include <cassert>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <winhttp.h>

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



  }
}