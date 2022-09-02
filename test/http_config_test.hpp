#pragma once

#include <string_view>

#include "chttpp.hpp"
#include "mime_types.hpp"

void test_req(std::string_view, chttpp::detail::request_config = {}) {}

// 呼び出しが意図通りに行えるかのチェックのみ（コンパイルが通ればそれでﾖｼ）
void http_config_test() {
  using namespace chttpp::mime_types;
  using namespace std::chrono_literals;

  test_req("https://example.com");
  test_req("https://example.com", {});
  test_req("https://example.com", {.mime_string = "application/json"});
  test_req("https://example.com", {.mime_string = application/json});
  test_req("https://example.com", {.headers = {{"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"},
                                               {"Content-Type", "text/plain"},
                                               {"Content-Language", "ja-JP"}}});
  test_req("https://example.com", {.params = {{"param1", "value1"}, {"param2", "value2"}}, .timeout = 1000ms});
  test_req("https://example.com", {.params = {{"param1", "value1"}, {"param2", "value2"}}, .auth = {.username = "test", .password = "pw"}});
  test_req("https://example.com", {.mime_string = application/json, .proxy = {.url = "https://localhost:7777", .auth = {.username = "prxy_user", .password = "prxy_pw"}}});

  // 全部のせ
  test_req("https://example.com", {.mime_string = application/json,
                                   .headers = {{"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"},
                                               {"Content-Type", "text/plain"},
                                               {"Content-Language", "ja-JP"}},
                                   .params = {{"param1", "value1"}, {"param2", "value2"}},
                                   .timeout = 1000ms,
                                   .auth = {.username = "test", .password = "pw"},
                                   .proxy = {.url = "https://localhost:7777", .auth = {.username = "prxy_user", .password = "prxy_pw"}}
                                  });
}