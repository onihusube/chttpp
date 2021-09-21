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


  "ters_get"_test = [] {
    {
      auto result = chttpp::get(L"https://example.com");

      !ut::expect(bool(result));
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() >= 1256_i);
    }
    {
      auto result = chttpp::get(L"http://example.com");

      !ut::expect(bool(result));
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() >= 1256_i);
    }
    {
      auto result = chttpp::get("https://example.com");

      !ut::expect(bool(result));
      ut::expect(result.status_code() == 200_i);
      ut::expect(result.response_body().length() >= 1256_i);
    }
  };
}