#pragma once

#include "chttpp.hpp"

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

/* curl レスポンスヘッダ例（example.com）
HTTP/2 200 
content-encoding: gzip
accept-ranges: bytes
age: 546118
cache-control: max-age=604800
content-type: text/html; charset=UTF-8
date: Fri, 17 Sep 2021 08:38:37 GMT
etag: "3147526947"
expires: Fri, 24 Sep 2021 08:38:37 GMT
last-modified: Thu, 17 Oct 2019 07:18:26 GMT
server: ECS (sab/5740)
vary: Accept-Encoding
x-cache: HIT
content-length: 648
*/

void underlying_test() {
  using namespace boost::ut::literals;
  using namespace boost::ut::operators::terse;
  using namespace std::string_view_literals;

  "parse_response_header"_test = [] {
    using chttpp::detail::parse_response_header_on_curl;

    constexpr std::string_view test_header[] = {
        "HTTP/2 200 \r\n",
        "content-encoding: gzip\r\n",
        "accept-ranges: bytes\r\n",
        "age: 546118\r\n",
        "cache-control: max-age=604800\r\n",
        "content-type: text/html; charset=UTF-8\r\n",
        "date: Fri, 17 Sep 2021 08:38:37 GMT\r\n",
        "etag: \"3147526947\"\r\n",
        "expires: Fri, 24 Sep 2021 08:38:37 GMT\r\n",
        "last-modified: Thu, 17 Oct 2019 07:18:26 GMT\r\n",
        "server: ECS (sab/5740)\r\n",
        "vary: Accept-Encoding\r\n",
        "x-cache: HIT\r\n",
        "content-length: 648\r\n" };

    chttpp::header_t headers;

    for (auto header : test_header) {
      parse_response_header_on_curl(headers, header.data(), header.length());
    }

    ut::expect(headers.size() == 14);

    constexpr std::string_view names[] = {
      "HTTP Ver",
      "content-encoding",
      "accept-ranges",
      "age",
      "cache-control",
      "content-type",
      "date",
      "etag",
      "expires",
      "last-modified",
      "server",
      "vary",
      "x-cache",
      "content-length"
    };

    for (auto name : names) {
      ut::expect(headers.contains(name.data()));
    }

  };

  "ters_get"_test = [] {
    {
      auto result = chttpp::get("https://example.com");

      !ut::expect(bool(result));
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() >= 648_i);
    }
    {
      auto result = chttpp::get("http://example.com");

      !ut::expect(bool(result));
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() >= 648_i);
    }
    {
      auto result = chttpp::get(L"https://example.com");

      !ut::expect(bool(result));
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() >= 648_i);
    }
  };
}