#pragma once

#include <string_view>

#include "chttpp.hpp"
#include "mime_types.hpp"
#include "http_headers.hpp"

void test_req(std::string_view, chttpp::detail::request_config = {}) {}

// 呼び出しが意図通りに行えるかのチェックのみ（コンパイルが通ればそれでﾖｼ）
void http_config_test() {
  using namespace chttpp::mime_types;
  using namespace chttpp::headers;
  using namespace std::chrono_literals;

  test_req("https://example.com");
  test_req("https://example.com", {});
  test_req("https://example.com", {.content_type = "application/json"});
  test_req("https://example.com", {.content_type = application/json});
  test_req("https://example.com", {.headers = {{"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"},
                                               {content_type, "text/plain"},
                                               {content_language, "ja-JP"}}});
  test_req("https://example.com", {.params = {{"param1", "value1"}, {"param2", "value2"}}, .timeout = 1000ms});
  test_req("https://example.com", {.params = {{"param1", "value1"}, {"param2", "value2"}}, .auth = {.username = "test", .password = "pw"}});
  test_req("https://example.com", {.content_type = application/json, .proxy = {.address = "https://localhost:7777", .auth = {.username = "prxy_user", .password = "prxy_pw"}}});

  // 全部のせ
  test_req("https://example.com", {.content_type = application/json,
                                   .headers = { user_agent = "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)",
                                                content_type = text/plain,
                                                content_language = "ja-JP" },
                                   .params = {{"param1", "value1"}, {"param2", "value2"}},
                                   .timeout = 1000ms,
                                   .auth = {.username = "test", .password = "pw"},
                                   .proxy = {.address = "https://localhost:7777", .auth = {.username = "prxy_user", .password = "prxy_pw"}}
                                  });

  // chrono::durationの変換
  test_req("https://example.com", { .timeout = 10s });
  test_req("https://example.com", { .timeout = 1min });
}