#pragma once

#include <string_view>

#include "chttpp.hpp"
#include "mime_types.hpp"
#include "http_headers.hpp"

void test_req(std::string_view, chttpp::detail::request_config = {}) {}

void test_agent_req(std::string_view, chttpp::detail::agent_request_config = {}) {}

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
  test_req("https://example.com", {.version = chttpp::cfg::http_version::http1_1});
  test_req("https://example.com", {.params = {{"param1", "value1"}, {"param2", "value2"}}, .timeout = 1000ms});
  test_req("https://example.com", {.params = {{"param1", "value1"}, {"param2", "value2"}}, .auth = {.username = "test", .password = "pw"}});
  test_req("https://example.com", {.content_type = application/json, .proxy = {.address = "localhost:7777", .auth = {.username = "prxy_user", .password = "prxy_pw"}}});

  // 全部のせ
  test_req("https://example.com", {.content_type = application/json,
                                   .headers = { user_agent = "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)",
                                                content_type = text/plain,
                                                content_language = "ja-JP" },
                                   .params = {{"param1", "value1"}, {"param2", "value2"}},
                                   .version = chttpp::cfg::http_version::http2,
                                   .timeout = 1000ms,
                                   .auth = {.username = "test", .password = "pw"},
                                   .proxy = {.address = "localhost:7777", .scheme = chttpp::cfg::proxy_scheme::http, .auth = {.username = "prxy_user", .password = "prxy_pw"}}
                                  });

  // chrono::durationの変換
  test_req("https://example.com", { .timeout = 10s });
  test_req("https://example.com", { .timeout = 1min });

  // httpバージョンの数値指定（コンパイル時検査付き）
  test_req("https://example.com", { .version = 2 });
  test_req("https://example.com", { .version = 1.1 });
  test_req("https://example.com", { .version = 2.0 });

  // ngな値（上記3つ以外）
  //test_req("https://example.com", { .version = 1 });
  //test_req("https://example.com", { .version = 11 });
  //test_req("https://example.com", { .version = 3 });
  //test_req("https://example.com", { .version = 1.0 });

  // agentのリクエスト時設定のテスト

  //test_agent_req("https://example.com", { .streaming_reciever = reentrant_function{[](std::span<const char>) {}} });
  test_agent_req("https://example.com", { .streaming_receiver = [](std::span<const char>) {} });
}