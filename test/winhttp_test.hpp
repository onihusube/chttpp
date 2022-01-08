#pragma once

#include "chttpp.hpp"

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

/* wihttpレスポンスヘッダ例（example.com）
HTTP/1.1 200 OK
Cache-Control: max-age=604800
Date: Sun, 19 Sep 2021 17:23:50 GMT
Content-Length: 1256
Content-Type: text/html; charset=UTF-8
Expires: Sun, 26 Sep 2021 17:23:50 GMT
Last-Modified: Thu, 17 Oct 2019 07:18:26 GMT
Age: 515403
ETag: "3147526947+ident"
Server: ECS (oxr/8328)
Vary: Accept-Encoding
X-Cache: HIT
*/

void underlying_test() {

  using namespace boost::ut::literals;
  using namespace boost::ut::operators::terse;
  using namespace std::string_view_literals;

  "parse_response_header_on_winhttp"_test = [] {
    using chttpp::detail::parse_response_header_on_winhttp;

    constexpr std::string_view test_header = {
        "HTTP/1.1 200 OK\r\n"
        "Cache-Control: max-age=604800\r\n"
        "Date: Sun, 19 Sep 2021 17:23:50 GMT\r\n"
        "Content-Length: 1256\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Expires: Sun, 26 Sep 2021 17:23:50 GMT\r\n"
        "Last-Modified: Thu, 17 Oct 2019 07:18:26 GMT\r\n"
        "Age: 515403\r\n"
        "ETag: \"3147526947+ident\"\r\n"
        "Server: ECS (oxr/8328)\r\n"
        "Vary: Accept-Encoding\r\n"
        "X-Cache: HIT\r\n\r\n"
    };

    chttpp::header_t headers = parse_response_header_on_winhttp(test_header);

    ut::expect(headers.size() == 12_ull);

    constexpr std::string_view names[] = {
      "HTTP Ver",
      "cache-control",
      "date",
      "content-length",
      "content-type",
      "expires",
      "last-modified",
      "age",
      "etag",
      "server",
      "vary",
      "x-cache",
    };

    for (auto name : names) {
      ut::expect(headers.contains(name.data()));
    }

  };

  "ters_get"_test = [] {
    {
      auto result = chttpp::get(L"https://example.com");

      ut::expect(bool(result));
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() >= 1256_ull);

      const auto& headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);

      {
        const auto httpver = result.response_header("HTTP Ver");
        ut::expect(httpver == "HTTP/1.1 200 "sv); // なぜか後ろにスペースが入る
      }
      {
        const auto clen = result.response_header("content-type");
        ut::expect(clen == "text/html; charset=UTF-8"sv);
      }
    }
    {
      auto result = chttpp::get(L"http://example.com");

      ut::expect(bool(result));
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() >= 1256_ull);

      const auto& headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);
    }
    {
      auto result = chttpp::get("https://example.com");

      ut::expect(bool(result));
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() >= 1256_ull);

      const auto& headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);
    }
  };

  "terse_head"_test = [] {
    {
      auto result = chttpp::head(L"https://example.com");

      ut::expect(bool(result));
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() == 0_ull);

      const auto& headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);

      {
        const auto httpver = result.response_header("HTTP Ver");
        ut::expect(httpver == "HTTP/1.1 200 "sv); // なぜか後ろにスペースが入る
      }
      {
        const auto clen = result.response_header("content-type");
        ut::expect(clen == "text/html; charset=UTF-8"sv);
      }

    }
  };
}