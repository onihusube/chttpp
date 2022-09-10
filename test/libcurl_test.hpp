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

  "terse_head"_test = [] {
    {
      auto result = chttpp::head("https://example.com");

      ut::expect(bool(result) >> ut::fatal);
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() == 0_i);

      const auto &headers = result.response_header();
      ut::expect(headers.size() >= 13_i);

      {
        const auto httpver = result.response_header("HTTP Ver");
        ut::expect(httpver == "HTTP/2 200 "sv); // なぜか後ろにスペースが入る
      }
      {
        const auto clen = result.response_header("content-length");
        ut::expect(clen.length() >= 3_i);
      }

    }
  };

  "rebuild_url"_test = [] {
    using chttpp::detail::rebuild_url;
    using chttpp::underlying::terse::unique_curlurl;
    using chttpp::underlying::terse::unique_curlchar;
    using chttpp::types::vector_t;
    using chttpp::types::string_t;

    {
      unique_curlurl hurl{::curl_url()};

      ut::expect(bool(hurl) >> ut::fatal);


      const std::string_view url = "https://example.com";
      vector_t<std::pair<std::string_view, std::string_view>> params = { {"param", "value"} };
      string_t buffer{};

      ut::expect((::curl_url_set(hurl.get(), CURLUPART_URL, url.data(), 0) == CURLUE_OK) >> ut::fatal);

      unique_curlchar ptr{rebuild_url(hurl.get(), params, buffer)};
      std::string_view res = ptr.get();

      ut::expect(res == "https://example.com/?param=value");
    }
    {
      unique_curlurl hurl{::curl_url()};

      ut::expect(bool(hurl) >> ut::fatal);

      const std::string_view url = "https://example.com/path/path?param1=value1";
      vector_t<std::pair<std::string_view, std::string_view>> params = { {"param2", "value2"}, {"param3", "value3"} };
      string_t buffer{};

      ut::expect((::curl_url_set(hurl.get(), CURLUPART_URL, url.data(), 0) == CURLUE_OK) >> ut::fatal);

      unique_curlchar ptr{rebuild_url(hurl.get(), params, buffer)};
      std::string_view res = ptr.get();

      ut::expect(res == "https://example.com/path/path?param1=value1&param2=value2&param3=value3");
    }
    {
      unique_curlurl hurl{::curl_url()};

      ut::expect(bool(hurl) >> ut::fatal);

      const std::string_view url = "https://user:pass@example.com/path#anchor";
      vector_t<std::pair<std::string_view, std::string_view>> params = {{"param", "value"}};
      string_t buffer{};

      ut::expect((::curl_url_set(hurl.get(), CURLUPART_URL, url.data(), 0) == CURLUE_OK) >> ut::fatal);

      unique_curlchar ptr{rebuild_url(hurl.get(), params, buffer)};
      std::string_view res = ptr.get();

      ut::expect(res == "https://example.com/path?param=value");
    }
    {
      unique_curlurl hurl{::curl_url()};

      ut::expect(bool(hurl) >> ut::fatal);

      const std::string_view url = "https://example.com/path";
      vector_t<std::pair<std::string_view, std::string_view>> params = {};
      string_t buffer{};

      ut::expect((::curl_url_set(hurl.get(), CURLUPART_URL, url.data(), 0) == CURLUE_OK) >> ut::fatal);

      unique_curlchar ptr{rebuild_url(hurl.get(), params, buffer)};
      std::string_view res = ptr.get();

      ut::expect(res == url);
    }
  };
}