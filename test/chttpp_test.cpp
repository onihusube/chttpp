//#define CHTTPP_NOT_GLOBAL_INIT_CURL
#include "chttpp.hpp"
#include "mime_types.hpp"
#include "http_headers.hpp"

#include <type_traits>
#include <cassert>
#include <forward_list>

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

#include <picojson.h>

namespace ut = boost::ut;

#ifdef _MSC_VER

#include "winhttp_test.hpp"

#else

#include "libcurl_test.hpp"

#endif

#include "http_result_test.hpp"
#include "http_config_test.hpp"
#include "status_code_test.hpp"
#include "cookie_test.hpp"
#include "exptr_wrapper_test.hpp"

namespace chttpp_test {

  struct wrap_vec1 {
    std::vector<int> v1;
    std::vector<int> v2;

    auto as_byte_seq() const -> std::span<const char> {
      return { reinterpret_cast<const char*>(this->v1.data()), sizeof(int) * this->v1.size()};
    }

    static constexpr std::string_view ContentType = "video/mp4";
  };

  struct wrap_vec2 {
    std::vector<int> v1;
    std::vector<int> v2;

    friend auto as_byte_seq(const wrap_vec2& self) -> std::span<const char> {
      return {reinterpret_cast<const char *>(self.v2.data()), sizeof(int) * self.v2.size()};
    }
  };
}

template <>
inline constexpr std::string_view chttpp::traits::query_content_type<chttpp_test::wrap_vec2> = "application/x-www-form-urlencoded";

int main() {
  using namespace boost::ut::literals;
  using namespace boost::ut::operators::terse;
  using namespace std::string_view_literals;
  using namespace std::chrono_literals;

  "parse_response_header_oneline"_test = [] {
    using chttpp::detail::parse_response_header_oneline;

    chttpp::header_t headers;

    parse_response_header_oneline(headers, "HTTP/1.1 200 OK"sv);

    ut::expect(headers.size() == 1);
    ut::expect(headers.contains("http-status-line"));
    ut::expect(headers["http-status-line"] == "HTTP/1.1 200 OK"sv);

    parse_response_header_oneline(headers, "cache-control: max-age=604800");

    ut::expect(headers.size() == 2);
    ut::expect(headers.contains("cache-control"));
    ut::expect(headers["cache-control"] == "max-age=604800"sv);

    parse_response_header_oneline(headers, R"(ETag: "3147526947+ident")");

    ut::expect(headers.size() == 3);
    ut::expect(headers.contains("etag"));
    ut::expect(headers["etag"] == R"("3147526947+ident")"sv);

    // :の後のスペース有無のテスト
    parse_response_header_oneline(headers, "Age:    515403");

    ut::expect(headers.size() == 4);
    ut::expect(headers.contains("age"));
    ut::expect(headers["age"] == "515403"sv);

    parse_response_header_oneline(headers, "date:Fri, 17 Sep 2021 08:38:37 GMT");

    ut::expect(headers.size() == 5);
    ut::expect(headers.contains("date"));
    ut::expect(headers["date"] == "Fri, 17 Sep 2021 08:38:37 GMT"sv);

    parse_response_header_oneline(headers, "Content-Length: 1256");

    ut::expect(headers.size() == 6);
    ut::expect(headers.contains("content-length"));
    ut::expect(headers["content-length"] == "1256"sv);

    parse_response_header_oneline(headers, "Vary: Accept-Encoding");
    parse_response_header_oneline(headers, "Vary: User-Agent");

    ut::expect(headers.size() == 7);
    ut::expect(headers.contains("vary"));
    ut::expect(headers["vary"] == "Accept-Encoding, User-Agent"sv);

    parse_response_header_oneline(headers, "Set-Cookie: name1=value1; Expires=Wed, 21 Oct 2015 07:28:00 GMT");
    parse_response_header_oneline(headers, "set-cookie: name2=value2; Secure");
    parse_response_header_oneline(headers, "Set-Cookie: name3=value3");
    ut::expect(headers.size() == 8);
    ut::expect(headers.contains("set-cookie"));
    ut::expect(headers["set-cookie"] == "name1=value1; Expires=Wed, 21 Oct 2015 07:28:00 GMT; name2=value2; Secure; name3=value3"sv) << headers["set-cookie"];
  };

#ifndef _MSC_VER
  "null_terminated_string_view"_test = [] {
    using namespace std::string_literals;
    using chttpp::detail::nt_string_view;

    // ほぼstring_viewの性質を受け継ぐ
    static_assert(std::is_trivially_copyable_v<nt_string_view>);
    static_assert(sizeof(nt_string_view) == sizeof(std::string_view));
    // 右辺値から構築できない
    static_assert(not std::constructible_from<nt_string_view, std::string>);
    static_assert(std::constructible_from<nt_string_view, std::string&>);
    static_assert(std::constructible_from<nt_string_view, std::pmr::string&>);

    static_assert(std::three_way_comparable<nt_string_view>);
    static_assert(std::equality_comparable<nt_string_view>);

    nt_string_view str1{"test string"};
    ut::expect(str1.str_view().size() == 11);
    ut::expect(str1.str_view() == "test string"sv);

    nt_string_view str2{"test string"sv};
    ut::expect(str2.str_view().size() == 11);
    ut::expect(str2.str_view() == "test string"sv);

    const auto dyn_str = "test string"s;
    nt_string_view str3{dyn_str};
    ut::expect(str3.str_view().size() == 11);
    ut::expect(str3.str_view() == "test string"sv);
  };
#endif

  "string_like concept"_test = [] {
    using chttpp::detail::string_like;

    static_assert(string_like<const char *>);
    static_assert(string_like<const wchar_t*>);
    static_assert(string_like<const char8_t*>);
    static_assert(string_like<const char16_t*>);
    static_assert(string_like<const char32_t *>);
    static_assert(string_like<decltype("test")>);
    static_assert(string_like<decltype(L"test")>);
    static_assert(string_like<decltype(u8"test")>);
    static_assert(string_like<decltype(u"test")>);
    static_assert(string_like<decltype(U"test")>);
    static_assert(string_like<std::string>);
    static_assert(string_like<std::wstring>);
    static_assert(string_like<std::u8string>);
    static_assert(string_like<std::u16string>);
    static_assert(string_like<std::u32string>);
    static_assert(string_like<std::string_view>);
    static_assert(string_like<std::wstring_view>);
    static_assert(string_like<std::u8string_view>);
    static_assert(string_like<std::u16string_view>);
    static_assert(string_like<std::u32string_view>);
    static_assert(string_like<const std::string>);
    static_assert(string_like<const std::string&>);
    static_assert(string_like<std::string&>);
    static_assert(string_like<std::string&&>);
    static_assert(string_like<const std::string_view>);
    static_assert(string_like<const std::string_view&>);
    static_assert(string_like<std::string_view&>);
    static_assert(string_like<std::string_view&&>);

    static_assert(not string_like<char*>);
    static_assert(not string_like<char>);
    static_assert(not string_like<int>);
    static_assert(not string_like<const int*>);
  };

  "as_byte_seq"_test = [] {
    static_assert(chttpp::byte_serializable<std::string>);
    static_assert(chttpp::byte_serializable<std::string&>);
    static_assert(chttpp::byte_serializable<std::string_view>);
    static_assert(chttpp::byte_serializable<std::span<std::byte>>);
    static_assert(chttpp::byte_serializable<std::span<int>>);
    static_assert(chttpp::byte_serializable<std::vector<char>>);
    static_assert(chttpp::byte_serializable<std::vector<int>>);
    static_assert(chttpp::byte_serializable<const char*>);

    static_assert(not chttpp::byte_serializable<int*>);
    static_assert(not chttpp::byte_serializable<const int*>);
    static_assert(not chttpp::byte_serializable<const int*&>);

    {
      std::string str = "test";
      std::span<const char> sp = chttpp::cpo::as_byte_seq(str);

      ut::expect(sp.size() == 4_ull);
    }
    {
      std::wstring str = L"test";
      std::span<const char> sp = chttpp::cpo::as_byte_seq(str);

      ut::expect(sp.size() == (sizeof(wchar_t) * 4));
    }
    {
      std::string_view str = "test";
      std::span<const char> sp = chttpp::cpo::as_byte_seq(str);

      ut::expect(sp.size() == 4_ull);
    }
    {
      std::span<const char> sp = chttpp::cpo::as_byte_seq("test");

      ut::expect(sp.size() == 4_ull);
    }
    {
      const char* str = "test";
      std::span<const char> sp = chttpp::cpo::as_byte_seq(str);

      ut::expect(sp.size() == 4_ull);
    }
    {
      std::vector vec = {1, 2, 3, 4};
      std::span<const char> sp = chttpp::cpo::as_byte_seq(vec);

      ut::expect(sp.size() == 16_ull);
    }
    {
      double d = 3.14;
      std::span<const char> sp = chttpp::cpo::as_byte_seq(d);

      ut::expect(sp.size() == 8_ull);
    }
    {
      std::vector vec = {1, 2, 3, 4};
      std::span sp1 = vec;
      std::span<const char> sp = chttpp::cpo::as_byte_seq(sp1);

      ut::expect(sp.size() == 16_ull);
    }

    {
      using chttpp_test::wrap_vec1;

      wrap_vec1 wv{.v1 = {1, 2, 3, 4}, .v2 = {}};
      std::span<const char> sp = chttpp::cpo::as_byte_seq(wv);

      ut::expect(sp.size() == 16_ull);
    }

    {
      using chttpp_test::wrap_vec2;

      wrap_vec2 wv{ .v1 = {}, .v2 = {1, 2, 3, 4}};
      std::span<const char> sp = chttpp::cpo::as_byte_seq(wv);

      ut::expect(sp.size() == 16_ull);
    }
  };

  "load_byte_seq"_test = [] {
    {
      int n = 10;
      std::span<const char> sp = chttpp::cpo::as_byte_seq(n);

      int m = 0;
      chttpp::cpo::load_byte_seq(m, sp);

      ut::expect(m == n);
    }
    {
      double d = 3.14115;
      std::span<const char> sp = chttpp::cpo::as_byte_seq(d);

      double d2 = 0;
      chttpp::cpo::load_byte_seq(d2, sp);

      ut::expect(d2 == d);
    }
    {
      std::vector vec = {1, 2, 3, 4};
      std::span<const char> sp = chttpp::cpo::as_byte_seq(vec);

      std::vector<int> vec2 = {5, 6, 7, 8};
      chttpp::cpo::load_byte_seq(vec2, sp);

      ut::expect(vec2 == vec);
    }
    {
      std::vector vec = {1, 2, 3, 4};
      std::span<const char> sp = chttpp::cpo::as_byte_seq(vec);

      std::forward_list fl = {5, 6, 7, 8};
      chttpp::cpo::load_byte_seq(fl, sp);

      ut::expect(std::ranges::equal(fl, vec));
    }
  };

  "fetch_content_type"_test = []
  {
    {
      std::string str = "test";

      ut::expect(chttpp::traits::query_content_type<decltype(str)> == "text/plain") << chttpp::traits::query_content_type<decltype(str)>;
    }
    {
      std::wstring str = L"test";
      ut::expect(chttpp::traits::query_content_type<decltype(str)> == "text/plain") << chttpp::traits::query_content_type<decltype(str)>;
    }
    {
      std::string_view str = "test";
      ut::expect(chttpp::traits::query_content_type<decltype(str)> == "text/plain") << chttpp::traits::query_content_type<decltype(str)>;
    }
    {
      ut::expect(chttpp::traits::query_content_type<decltype("test")> == "text/plain") << chttpp::traits::query_content_type<decltype("test")>;
    }
    {
      [[maybe_unused]]
      const char *str = "test";
      ut::expect(chttpp::traits::query_content_type<decltype(str)> == "text/plain") << chttpp::traits::query_content_type<decltype(str)>;
    }
    {
      ut::expect(chttpp::traits::query_content_type<const char*> == "text/plain") << chttpp::traits::query_content_type<const char*>;
    }
    {
      std::vector vec = {1, 2, 3, 4};
      ut::expect(chttpp::traits::query_content_type<decltype(vec)> == "application/octet_stream") << chttpp::traits::query_content_type<decltype(vec)>;
    }

    {
      [[maybe_unused]]
      double d = 3.14;

      ut::expect(chttpp::traits::query_content_type<decltype(d)> == "application/octet_stream") << chttpp::traits::query_content_type<decltype(d)>;
    }
    {
      std::vector vec = {1, 2, 3, 4};
      std::span sp1 = vec;

      ut::expect(chttpp::traits::query_content_type<decltype(sp1)> == "application/octet_stream") << chttpp::traits::query_content_type<decltype(sp1)>;
    }
    {
      using chttpp_test::wrap_vec1;

      wrap_vec1 wv{.v1 = {1, 2, 3, 4}, .v2 = {}};

      ut::expect(chttpp::traits::query_content_type<decltype(wv)> == "video/mp4") << chttpp::traits::query_content_type<decltype(wv)>;
    }
    {
      using chttpp_test::wrap_vec2;

      wrap_vec2 wv{.v1 = {}, .v2 = {1, 2, 3, 4}};

      ut::expect(chttpp::traits::query_content_type<decltype(wv)> == "application/x-www-form-urlencoded") << chttpp::traits::query_content_type<decltype(wv)>;
    }
  };

  "terse_options"_test = [] {
    auto result = chttpp::options(L"https://example.com");

    ut::expect(bool(result));
    const auto status_code = result.status_code();
    ut::expect(status_code.is_successful());

    const auto allow = result.response_header("allow");
    ut::expect(5_ull < allow.size());
    ut::expect(allow == "OPTIONS, GET, HEAD, POST"sv);

  };

  "terse_trace"_test = [] {
    auto result = chttpp::trace("https://example.com");

    ut::expect(bool(result));
    const auto status_code = result.status_code();
    ut::expect(status_code == 405);

  };

  "mime type"_test = [] {
    using namespace chttpp::mime_types;
    using namespace std::string_view_literals;

    {
      constexpr auto mime = application/x_www_form_urlencoded;
      ut::expect(mime == "application/x-www-form-urlencoded"sv);
    }
    {
      constexpr auto mime = text/javascript;
      ut::expect(mime == "text/javascript"sv);
    }
    {
      constexpr auto mime = font/ttf;
      ut::expect(mime == "font/ttf"sv);
    }
    {
      constexpr auto mime1 = video/ogg;
      ut::expect(mime1 == "video/ogg"sv);
      constexpr auto mime2 = audio/ogg;
      ut::expect(mime2 == "audio/ogg"sv);
      constexpr auto mime3 = application/ogg;
      ut::expect(mime3 == "application/ogg"sv);
      constexpr auto mime4 = application/octet_stream;
      ut::expect(mime4 == "application/octet-stream"sv);
      constexpr auto mime5 = application/json;
      ut::expect(mime5 == "application/json"sv);
    }
    {
      //constexpr auto mime = video/３gpp;
      //ut::expect(mime == "video/3gpp"sv);
    }
    {
      // xmlは単独でsubtypeになるし、+で結合してsubtypeを完成させる
      constexpr auto mime1 = image/svg+xml;
      ut::expect(mime1 == "image/svg+xml"sv);
      constexpr auto mime2 = text/xml;
      ut::expect(mime2 == "text/xml"sv);
      constexpr auto mime3 = application/atom+xml;
      ut::expect(mime3 == "application/atom+xml"sv);
    }
    {
      constexpr auto mime = application/vnd.apple.installer+xml;
      ut::expect(mime == "application/vnd.apple.installer+xml"sv);
    }

  };

  "predefinde header"_test = [] {
    using namespace chttpp::headers;
    using namespace chttpp::mime_types;

    // 全部はテストせず、基本的or複雑な一部のみチェック（header_baseの動作がバグってないことを確かめるのみ）
    static_assert(std::string_view{content_length} == "content-length");
    static_assert(std::string_view{content_type} == "content-type");
    static_assert(std::string_view{user_agent} == "user-agent");
    static_assert(std::string_view{etag} == "etag");
    static_assert(std::string_view{access_control_allow_origin} == "access-control-allow-origin");

    using P = std::pair<std::string_view, std::string_view>;
    {
      constexpr auto p = content_type = "text/plain";

      static_assert(P{p}.first == "content-type");
      static_assert(P{p}.second == "text/plain");
    }
    {
      constexpr auto p = content_type = text/plain;
      
      static_assert(P{p}.first == "content-type");
      static_assert(P{p}.second == "text/plain");
    }
  };

  // jsonレスポンスをpicojsonのvalueオブジェクトへ変換する
  const auto to_json = [](std::string_view str_view) -> picojson::value {
    picojson::value result{};

    [[maybe_unused]]
    auto err = picojson::parse(result, std::string{str_view});

    ut::expect(err.empty()) << err;

    return result;
  };


  "ters_get"_test = [to_json] {
    {
      auto result = chttpp::get("https://example.com");

      ut::expect(bool(result) >> ut::fatal);
      ut::expect(result.status_code() == 200) << result.status_code().value();
      ut::expect(result.response_body().length() >= 648_ull);
      
      const auto &headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);
    }
    {
      auto result = chttpp::get("http://example.com");

      ut::expect(bool(result) >> ut::fatal);
      ut::expect(result.status_code() == 200) << result.status_code().value();
      ut::expect(result.response_body().length() >= 648_ull);

      const auto &headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);
    }
    {
      auto result = chttpp::get(L"https://example.com");

      ut::expect(bool(result) >> ut::fatal);
      ut::expect(result.status_code() == 200) << result.status_code().value();
      ut::expect(result.response_body().length() >= 648_ull);

      const auto &headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);
    }
    {
      auto result = chttpp::get("https://example.com", { .headers = {{"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"}} });

      ut::expect(bool(result) >> ut::fatal);
      ut::expect(result.status_code() == 200) << result.status_code().value();
      ut::expect(result.response_body().length() >= 648_ull);

      const auto &headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);
    }
    {
      auto result = chttpp::get("https://httpbin.org/get", { .params = {
                                                                {"param1", "value1"},
                                                                {"param2", "value2"},
                                                                {"param3", "value3"}
                                                              }
                                                           });

      ut::expect(bool(result) >> ut::fatal);
      ut::expect(result.status_code() == 200) << result.status_code().value();
      auto res_json = result | to_json;

      // jsonデコードとその結果チェック
      ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);
      const auto &obj = res_json.get<picojson::value::object>();
      ut::expect(obj.contains("args") >> ut::fatal);

      // 送ったパラメータのチェック
      ut::expect(obj.at("url").get<std::string>() == "https://httpbin.org/get?param1=value1&param2=value2&param3=value3");
    }
    {
      auto result = chttpp::get("https://httpbin.org/get?param1=value1", { .params = {
                                                                              {"param2", "value2"},
                                                                              {"param3", "value3"}
                                                                            }
                                                                         });

      ut::expect(bool(result) >> ut::fatal);
      ut::expect(result.status_code() == 200) << result.status_code().value();
      auto res_json = result | to_json;

      // jsonデコードとその結果チェック
      ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);
      const auto &obj = res_json.get<picojson::value::object>();
      ut::expect(obj.contains("url") >> ut::fatal);

      // 送ったパラメータのチェック
      ut::expect(obj.at("url").get<std::string>() == "https://httpbin.org/get?param1=value1&param2=value2&param3=value3");
    }
  };

  "terse post"_test = [to_json]
  {
    using namespace chttpp::mime_types;
    using namespace std::string_view_literals;

    {
      auto result = chttpp::post("https://httpbin.org/post", "field1=value1&field2=value2", { .content_type = text/plain });

      ut::expect(bool(result) >> ut::fatal) << result.error_message();
      ut::expect(result.status_code() == 200) << result.status_code().value();
      // std::cout << result.response_body();
      /*
      なんかこんな感じのが得られるはず
      {
        "args": {},
        "data": "field1=value1&field2=value2",
        "files": {},
        "form": {},
        "headers": {
          "Accept": "＊/＊",
          "Accept-Encoding": "deflate, gzip",
          "Content-Length": "27",
          "Content-Type": "text/plain",
          "Host": "httpbin.org",
          "X-Amzn-Trace-Id": "Root=1-61d7ed99-4ce197f010a8866b75072a7e"
        },
        "json": null,
        "origin": "www.xxx.yyy.zzz",
        "url": "https://httpbin.org/post"
      }
      */

      auto res_json = result | to_json;

      ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);

      const auto &obj = res_json.get<picojson::value::object>();

      ut::expect(std::ranges::size(obj) == 8_ull);
      ut::expect(obj.contains("data") >> ut::fatal);
      ut::expect(obj.contains("url") >> ut::fatal);
      ut::expect(obj.contains("headers") >> ut::fatal);

      // json要素のチェック
      ut::expect(obj.at("data").get<std::string>() == "field1=value1&field2=value2");
      ut::expect(obj.at("url").get<std::string>() == "https://httpbin.org/post");

      const auto &headers = obj.at("headers").get<picojson::value::object>();

      //ut::expect(std::ranges::size(headers) == 6_ull);
      ut::expect(headers.contains("Content-Length"));
      ut::expect(headers.contains("Content-Type"));
      ut::expect(headers.contains("User-Agent"));

      ut::expect(headers.at("Content-Length").get<std::string>() == "27");
      ut::expect(headers.at("Content-Type").get<std::string>() == "text/plain");
      ut::expect(headers.at("User-Agent").get<std::string>() == chttpp::detail::default_UA);
    }
    {
      // リクエストボディとURLパラメータの同時送信
      auto result = chttpp::post("https://httpbin.org/post", "post data", { .content_type = text/plain,
                                                                            .params = {
                                                                              {"param1", "value1"},
                                                                              {"param2", "value2"},
                                                                              {"param3", "value3"}
                                                                            }
                                                                          });

      ut::expect(bool(result) >> ut::fatal) << result.error_message();
      ut::expect(result.status_code() == 200) << result.status_code().value();

      auto res_json = result | to_json;

      ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);

      const auto &obj = res_json.get<picojson::value::object>();
      ut::expect(obj.contains("data") >> ut::fatal);
      ut::expect(obj.contains("url") >> ut::fatal);

      // リクエストボディのチェック
      ut::expect(obj.at("data").get<std::string>() == "post data");

      // 送ったパラメータのチェック
      ut::expect(obj.at("url").get<std::string>() == "https://httpbin.org/post?param1=value1&param2=value2&param3=value3");
    }
  };

  "terse post header"_test = [to_json] {
    using namespace chttpp::mime_types;
    using namespace std::string_view_literals;

    auto result = chttpp::post("https://httpbin.org/post", "field1=value1&field2=value2", { .headers = {{"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"}} });

    ut::expect(bool(result) >> ut::fatal) << result.error_message();
    ut::expect((result.status_code() == 200) >> ut::fatal) << result.status_code().value();

    auto res_json = result | to_json;

    ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);

    const auto &obj = res_json.get<picojson::value::object>();

    ut::expect(std::ranges::size(obj) == 8_ull);
    ut::expect(obj.contains("data") >> ut::fatal);
    ut::expect(obj.contains("url") >> ut::fatal);
    ut::expect(obj.contains("headers") >> ut::fatal);

    // json要素のチェック
    ut::expect(obj.at("data").get<std::string>() == "field1=value1&field2=value2");
    ut::expect(obj.at("url").get<std::string>() == "https://httpbin.org/post");

    const auto &headers = obj.at("headers").get<picojson::value::object>();

    // ut::expect(std::ranges::size(headers) == 6_ull);
    ut::expect(headers.contains("Content-Length"));
    ut::expect(headers.contains("Content-Type"));
    ut::expect(headers.contains("User-Agent"));

    ut::expect(headers.at("Content-Length").get<std::string>() == "27");
    ut::expect(headers.at("Content-Type").get<std::string>() == "text/plain");
    ut::expect(headers.at("User-Agent").get<std::string>() == "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)");
  };

  "terse post headers"_test = [to_json] {
    using namespace chttpp::mime_types;
    using namespace std::string_view_literals;

    auto result = chttpp::post("https://httpbin.org/post", "field1=value1&field2=value2", { .content_type = image/svg ,
                                                                                            .headers = {{"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"},
                                                                                                       {"Content-Type", "text/plain"},
                                                                                                       {"Content-Language", "ja-JP"}}
                                                                                          });

    ut::expect(bool(result) >> ut::fatal) << result.error_message();
    ut::expect(result.status_code() == 200) << result.status_code().value();

    auto res_json = result | to_json;

    //std::cout << result.response_body() << std::endl;

    ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);

    const auto &obj = res_json.get<picojson::value::object>();

    ut::expect(std::ranges::size(obj) == 8_ull);
    ut::expect(obj.contains("data") >> ut::fatal);
    ut::expect(obj.contains("url") >> ut::fatal);
    ut::expect(obj.contains("headers") >> ut::fatal);

    // json要素のチェック
    ut::expect(obj.at("data").get<std::string>() == "field1=value1&field2=value2");
    ut::expect(obj.at("url").get<std::string>() == "https://httpbin.org/post");

    const auto &headers = obj.at("headers").get<picojson::value::object>();

    // ut::expect(std::ranges::size(headers) == 6_ull);
    ut::expect(headers.contains("Content-Length"));
    ut::expect(headers.contains("Content-Type"));
    ut::expect(headers.contains("User-Agent"));
    ut::expect(headers.contains("Content-Language"));

    ut::expect(headers.at("Content-Length").get<std::string>() == "27");
    ut::expect(headers.at("Content-Type").get<std::string>() == "text/plain");  // ヘッダ設定のmime_typeが優先される
    ut::expect(headers.at("User-Agent").get<std::string>() == "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)");
    ut::expect(headers.at("Content-Language").get<std::string>() == "ja-JP");
  };

  "terse put"_test = [to_json]
  {
    using namespace chttpp::mime_types;
    using namespace std::string_view_literals;

    auto result = chttpp::put("https://httpbin.org/put", "<p>put test</p>", {.content_type = text/html });

    ut::expect(bool(result) >> ut::fatal) << result.error_message();
    ut::expect(result.status_code() == 200) << result.status_code().value();
/*
なんかこんな感じのが得られるはず
{
  "args": {}, 
  "data": "<p>put test</p>", 
  "files": {}, 
  "form": {}, 
  "headers": {
    "Accept": "＊/＊", 
    "Accept-Encoding": "deflate, gzip", 
    "Content-Length": "15", 
    "Content-Type": "text/html", 
    "Host": "httpbin.org", 
    "X-Amzn-Trace-Id": "Root=1-61d800fe-7a213f0f0a3fe8a0594c4a74"
  }, 
  "json": null, 
  "origin": "www.xxx.yyy.zzz", 
  "url": "https://httpbin.org/put"
}
*/

    auto res_json = result | to_json;

    ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);

    const auto &obj = res_json.get<picojson::value::object>();

    ut::expect(std::ranges::size(obj) == 8_ull);
    ut::expect(obj.contains("data") >> ut::fatal);
    ut::expect(obj.contains("url") >> ut::fatal);
    ut::expect(obj.contains("headers") >> ut::fatal);

    // json要素のチェック
    ut::expect(obj.at("data").get<std::string>() == "<p>put test</p>");
    ut::expect(obj.at("url").get<std::string>() == "https://httpbin.org/put");

    const auto &headers = obj.at("headers").get<picojson::value::object>();

    //ut::expect(std::ranges::size(headers) == 6_ull);
    ut::expect(headers.contains("Content-Length"));
    ut::expect(headers.contains("Content-Type"));

    ut::expect(headers.at("Content-Length").get<std::string>() == "15");
    ut::expect(headers.at("Content-Type").get<std::string>() == "text/html");
  };

  "terse delete"_test = [to_json]
  {
    using namespace chttpp::mime_types;
    using namespace std::string_view_literals;

    auto result = chttpp::delete_("https://httpbin.org/delete", "delete test", {.content_type = text/plain });

    ut::expect(bool(result) >> ut::fatal) << result.error_message();
    ut::expect(result.status_code() == 200) << result.status_code().value();
/*
なんかこんな感じのが得られるはず
{
  "args": {}, 
  "data": "delete test", 
  "files": {}, 
  "form": {}, 
  "headers": {
    "Accept": "＊/＊", 
    "Accept-Encoding": "deflate, gzip", 
    "Content-Length": "11", 
    "Content-Type": "text/plain", 
    "Host": "httpbin.org", 
    "X-Amzn-Trace-Id": "Root=1-61d80377-3a314ce12aa66d95192130cc"
  }, 
  "json": null, 
  "origin": "www.xxx.yyy.zzz", 
  "url": "https://httpbin.org/delete"
}
*/

    auto res_json = result | to_json;
    ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);

    const auto &obj = res_json.get<picojson::value::object>();

    ut::expect(std::ranges::size(obj) == 8_ull);
    ut::expect(obj.contains("data") >> ut::fatal);
    ut::expect(obj.contains("url") >> ut::fatal);
    ut::expect(obj.contains("headers") >> ut::fatal);

    // json要素のチェック
    ut::expect(obj.at("data").get<std::string>() == "delete test");
    ut::expect(obj.at("url").get<std::string>() == "https://httpbin.org/delete");

    const auto &headers = obj.at("headers").get<picojson::value::object>();

    //ut::expect(std::ranges::size(headers) == 6_ull);
    ut::expect(headers.contains("Content-Length"));
    ut::expect(headers.contains("Content-Type"));

    ut::expect(headers.at("Content-Length").get<std::string>() == "11");
    ut::expect(headers.at("Content-Type").get<std::string>() == "text/plain");
  };

  "timeout"_test = []{
    {
      auto res = chttpp::get("https://httpbin.org/delay/5", { .timeout = 100ms });

      ut::expect((not res.has_response()) >> ut::fatal);

#ifdef _MSC_VER
      ut::expect(res.error() == 12002);
#else
      ut::expect(res.error() == CURLE_OPERATION_TIMEDOUT);
#endif
    }
    {
      auto res = chttpp::post("https://httpbin.org/delay/5", "post", { .timeout = 100ms });

      ut::expect((not res.has_response()) >> ut::fatal);

#ifdef _MSC_VER
      ut::expect(res.error() == 12002);
#else
      ut::expect(res.error() == CURLE_OPERATION_TIMEDOUT);
#endif
    }

  };

  "http auth"_test = [to_json] {
    using namespace chttpp::mime_types;
    {
      auto res = chttpp::get("https://httpbin.org/basic-auth/authtest_user/authtest_pw", { .auth = {
                                                                                             .username = "authtest_user",
                                                                                             .password = "authtest_pw",
                                                                                             .scheme = chttpp::cfg_auth::basic
                                                                                           }
                                                                                         });

      ut::expect(res.has_response() >> ut::fatal) << res.error_message();
      ut::expect(res.status_code() == 200) << res.status_code().value();

      //std::cout << res.response_body() << "\n";
/*
こんなのが帰ってくる
{
  "authenticated": true, 
  "user": "authtest_user"
}
*/

      auto res_json = res | to_json;
      ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);

      const auto &obj = res_json.get<picojson::value::object>();

      ut::expect(obj.at("authenticated").get<bool>());
      ut::expect(obj.at("user").get<std::string>() == "authtest_user");
    }
    {
      auto res = chttpp::get("https://authtest_user:authtest_pw@httpbin.org/basic-auth/authtest_user/authtest_pw");

      ut::expect(res.has_response() >> ut::fatal);
      ut::expect(res.status_code().OK()) << res.status_code().value();
    }
    // POSTリクエスト時の認証はhttpbinだとテストできない（405が帰ってくる）
    {
      // 参考 : https://softmoco.com/basics/how-to-make-http-request-basic-auth-json.php
      auto res = chttpp::post("https://softmoco.com/getItemInfo.php", R"({ "itemCode": "ABC" })",
                              { .content_type = application/json,
                                .auth = {
                                   .username = "APIUser",
                                   .password = "APIPassword123",
                                   .scheme = chttpp::cfg::authentication_scheme::basic
                                }
                              });

      ut::expect(res.has_response() >> ut::fatal) << res.error_message();
      ut::expect(res.status_code().OK()) << res.status_code().value();

      //std::cout << res.response_body() << "\n";
/*
こんなのが帰ってくる
{
  "success":true,
  "message":"ItemCode:ABC found.",
  "item":{
     "itemCode":"ABC",
     "itemName":"ABC Item Name",
     "unitPrice":150
  }
}
*/

      auto res_json = res | to_json;
      ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);

      const auto &obj = res_json.get<picojson::value::object>();

      ut::expect(obj.at("success").get<bool>());
      ut::expect(obj.at("message").get<std::string>() == "ItemCode:ABC found.");
    }
  };

  "proxy"_test = [to_json] {
    // Free Proxy List の稼働率90%以上のものの中から選択
    // https://www.freeproxylists.net/ja/?c=&pt=&pr=&a%5B%5D=0&a%5B%5D=1&a%5B%5D=2&u=90
    // 公開proxyサイトの比較
    // https://www.softwaretestinghelp.com/free-http-and-https-proxies/

    // Proxy経由のアクセスには4パターンありえる
    // 1. httpアクセス  : httpプロクシ  -> 普通のプロクシ
    // 2. httpsアクセス : httpプロクシ  -> トンネルモード（CONNECTによる中継）
    // 3. httpアクセス  : httpsプロクシ -> ありえない？
    // 4. httpsアクセス : httpsプロクシ -> いわゆる中間者攻撃のような状態のプロクシ

    // 4のテストにはmitmproxyのような串が必要だが、そんなもの公開して提供してる人がいるんだろうか？

    // 1. http proxy による httpアクセス
    {
      auto result = chttpp::get("http://example.com", { .timeout = 10000ms, .proxy = { .address = "165.154.235.178:80" } });

      ut::expect(result.has_response() >> ut::fatal) << " : " << result.error_message();
      ut::expect(result.status_code().OK()) << result.status_code().value();
      ut::expect(result.response_body().length() >= 648_ull);

      const auto &headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);
    }
    // 2. http proxy による httpsアクセス
    {
      auto result = chttpp::get("https://example.com", { .timeout = 10000ms, .proxy = { .address = "140.227.80.237:3180", .scheme = chttpp::cfg_prxy::http } });

      ut::expect(result.has_response() >> ut::fatal) << result.error_message();
      ut::expect(result.status_code().OK()) << result.status_code().value();
      ut::expect(result.response_body().length() >= 648_ull);

      const auto &headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);
    }

    // socks proxy による httpアクセス
    // httpsアクセスはCURLE_PEER_FAILED_VERIFICATIONでうまくいかない・・・
    {
      auto result = chttpp::get("http://example.com", {.timeout = 10000ms, .proxy = { .address = "192.111.139.163:19404", .scheme = chttpp::cfg_prxy::socks5 } });

      ut::expect(result.has_response() >> ut::fatal) << result.error_message();
      ut::expect(result.status_code().OK()) << result.status_code().value();
      ut::expect(result.response_body().length() >= 648_ull);

      const auto& headers = result.response_header();
      ut::expect(headers.size() >= 11_ull);
    }
    using namespace chttpp::mime_types;
    using namespace std::string_view_literals;
    // postのテスト
    {
      auto result = chttpp::post("http://httpbin.org/post", "proxy test", { .content_type = text/plain, .proxy = { .address = "140.227.80.237:3180" } });

      ut::expect(bool(result) >> ut::fatal) << result.error_message();
      ut::expect(result.status_code().OK()) << result.status_code().value();

      auto res_json = result | to_json;

      ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);

      const auto &obj = res_json.get<picojson::value::object>();
      ut::expect(obj.contains("data") >> ut::fatal);
      // json要素のチェック
      ut::expect(obj.at("data").get<std::string>() == "proxy test");
    }
  };

  "http version"_test = []
  {
    using namespace chttpp::headers;
    {
      auto result = chttpp::get("https://example.com", { .version = chttpp::cfg_ver::http1_1 });

      ut::expect(bool(result) >> ut::fatal);
      ut::expect(result.status_code().OK()) << result.status_code().value();
      const auto ver = result.response_header(http_status);
      ut::expect(ver == "HTTP/1.1 200 OK") << ver;
    }
    {
      using namespace chttpp::mime_types;

      auto result = chttpp::post("https://httpbin.org/post", "test", { .content_type = text/plain, .version = chttpp::cfg_ver::http1_1 });

      ut::expect(bool(result) >> ut::fatal) << result.error_message();
      ut::expect(result.status_code().OK()) << result.status_code().value();
      const auto ver = result.response_header(http_status);
      ut::expect(ver == "HTTP/1.1 200 OK") << ver;
    }
  };

  "agent"_test = [to_json] {
    {
      // CTADのチェック
      [[maybe_unused]]
      chttpp::agent req1{ "https://example.com", {} };
      [[maybe_unused]]
      chttpp::agent req2{ L"https://example.com", {} };

      const std::string url = "https://example.com";
      [[maybe_unused]]
      chttpp::agent req3{url};
    }
    {
      using namespace chttpp::headers;

      // headers()の引数私のチェック
      const std::string ua = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.6.1 Safari/605.1.15";
      [[maybe_unused]]
      auto test_header = chttpp::agent("https://example.com")
                      .headers({content_type = "application/atom+xml;type=entry;charset=utf-8",
                                authorization = "token...", 
                                {"user-agent", ua}});
      auto r = test_header.inspect_header();

      std::unordered_map<std::string_view, std::string_view> headers{r.begin(), r.end()};

      ut::expect(headers.size() == 3);
      ut::expect(headers.at(content_type) == "application/atom+xml;type=entry;charset=utf-8");
      ut::expect(headers.at(authorization) == "token...");
      ut::expect(headers.at("user-agent") == ua);
    }
    using namespace chttpp::method_object;

    auto req = chttpp::agent{"https://httpbin.org/", {}}
                            .headers({{"User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.6.1 Safari/605.1.15"}})
                            .configs(chttpp::cookie_management::disable);

    req.set_configs(chttpp::cookie_management::enable);

    {
      auto result = req.request<get>("get", {.params = {
                                                {"param1", "value1"},
                                                {"param2", "value2"},
                                                {"param3", "value3"}}
                                            });

      ut::expect(bool(result)) << result.error_message();
      if (result) {
        ut::expect(result.status_code().value() == 200_i);

        auto res_json = result | to_json;

        ut::expect(res_json.is<picojson::value::object>() >> ut::fatal);

        const auto &obj = res_json.get<picojson::value::object>();
        ut::expect(obj.contains("url") >> ut::fatal);

        // 送ったパラメータのチェック
        ut::expect(obj.at("url").get<std::string>() == "https://httpbin.org/get?param1=value1&param2=value2&param3=value3") << obj.at("url").get<std::string>();
      }
    }
    {
      using namespace chttpp::mime_types;
      using namespace std::string_view_literals;

      req.set_headers({{"User-Agent", "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)"}, 
                       {"Content-Type", "text/plain"}});

      auto result = req.request<post>("post", "agent post test", 
                                              { .content_type = image/svg,
                                                .headers = {
                                                  {"Content-Language", "ja-JP"}
                                                },
                                                .params = {
                                                   {"param1", "value1"},
                                                   {"param2", "value2"},
                                                   {"param3", "value3"}
                                                }
                                              });

      ut::expect(bool(result)) << result.error_message();

      if (result) {
        ut::expect(result.status_code() == 200) << result.status_code().value();

        auto res_json = result | to_json;

        ut::expect(res_json.is<picojson::value::object>());

        if (res_json.is<picojson::value::object>()) {
          const auto &obj = res_json.get<picojson::value::object>();

          ut::expect(std::ranges::size(obj) == 8_ull);
          ut::expect(obj.contains("data") >> ut::fatal);
          ut::expect(obj.contains("url") >> ut::fatal);
          ut::expect(obj.contains("headers") >> ut::fatal);

          // json要素のチェック
          ut::expect(obj.at("data").get<std::string>() == "agent post test");
          ut::expect(obj.at("url").get<std::string>() == "https://httpbin.org/post?param1=value1&param2=value2&param3=value3") << obj.at("url").get<std::string>();

          const auto &headers = obj.at("headers").get<picojson::value::object>();

          // ut::expect(std::ranges::size(headers) == 6_ull);
          ut::expect(headers.contains("Content-Length"));
          ut::expect(headers.contains("Content-Type"));
          ut::expect(headers.contains("User-Agent"));
          ut::expect(headers.contains("Content-Language"));

          ut::expect(headers.at("Content-Length").get<std::string>() == "15");
          ut::expect(headers.at("Content-Type").get<std::string>() == "text/plain"); // ヘッダ設定のmime_typeが優先される
          ut::expect(headers.at("User-Agent").get<std::string>() == "Mozilla/5.0 (compatible; MSIE 9.0; Windows Phone OS 7.5; Trident/5.0; IEMobile/9.0)");
          ut::expect(headers.at("Content-Language").get<std::string>() == "ja-JP");
        }

      }
    }
    {
      // agent本体にクッキーをセット
      req.set_cookies({{.name = "preset", .value = "cookie", .domain = "httpbin.org", .path = "/cookies"}});
      // サーバからクッキーを取得
      {
        auto res = req.request<get>("/cookies/set/set_cookie/test");
        ut::expect(bool(res)) << res.error_message();
        // 厳密にはリダイレクトが起きてるっぽい、返答がよくわからないのでとりあえず細かくチェックしない
        //ut::expect(result.status_code().OK()) << result.status_code().value();
      }
      // リクエスト時にクッキーを指定
      auto result = req.request<get>("cookies", { .cookies = { {"request", "cookie"} } });

      ut::expect(bool(result)) << result.error_message();

      if (result) {
        ut::expect(result.status_code().OK()) << result.status_code().value();

        //std::cout << result.response_body() << '\n';

        auto res_json = result | to_json;
      
        ut::expect(res_json.is<picojson::value::object>());

        if (res_json.is<picojson::value::object>()) {

          const auto &obj = res_json.get<picojson::value::object>();
          ut::expect(obj.contains("cookies") >> ut::fatal);

          const auto &cookies = obj.at("cookies").get<picojson::value::object>();

          ut::expect(cookies.contains("preset") >> ut::fatal);
          ut::expect(cookies.contains("set_cookie") >> ut::fatal);
          ut::expect(cookies.contains("request") >> ut::fatal);

          // 送ったパラメータのチェック
          ut::expect(cookies.at("preset").get<std::string>() == "cookie");
          ut::expect(cookies.at("set_cookie").get<std::string>() == "test");
          ut::expect(cookies.at("request").get<std::string>() == "cookie");
        }
      }
    }
    if (false) {
      // コンパイルが通るかのチェック
      assert(false);
      std::ignore = req.post("", "payload", {});
    }
  };

  "agent config"_test = [] {
    using namespace chttpp::method_object;

    auto req = chttpp::agent{L"https://httpbin.org/", {}}
                   .configs(chttpp::follow_redirects::disable, chttpp::cookie_management::disable);

    {
      auto [initconfig, managecookie, redirect, autodecomp] = req.inspect_config();

      ut::expect(managecookie == chttpp::cookie_management::disable);
      ut::expect(redirect == chttpp::follow_redirects::disable);
      ut::expect(autodecomp == chttpp::automatic_decompression::enable);
    }

    auto result = req.request<get>(L"/redirect-to", { .params = {{"url", "https://www.google.com/"}} });

    ut::expect(bool(result)) << result.error_message();

    if (result) {
      ut::expect(result.status_code().Found()) << result.status_code().value();
    }

    req.set_configs(chttpp::automatic_decompression::disable);

    {
      auto [initconfig, managecookie, redirect, autodecomp] = req.inspect_config();

      ut::expect(managecookie == chttpp::cookie_management::disable);
      ut::expect(redirect == chttpp::follow_redirects::disable);
      ut::expect(autodecomp == chttpp::automatic_decompression::disable);
    }
  };

  "chunk encoding"_test = [] {
    using namespace chttpp::method_object;
    using namespace std::chrono_literals;

    auto req = chttpp::agent{"http://anglesharp.azurewebsites.net/Chunked", { .version = chttpp::cfg_ver::http1_1, .timeout = 5s }}
                   .configs(chttpp::cookie_management::disable);

    std::vector<std::string> chunked_response;

    auto callback = [&](std::span<const char> data) {
      chunked_response.emplace_back(data.begin(), data.end());
    };

    req.get("", { .streaming_receiver = callback })
      .then([&](auto &&response) {
        ut::expect(response.status_code.OK()) << response.status_code.value();
        ut::expect(response.body.empty());

        const auto len = chunked_response.size();
        ut::expect(4u < len && len <= 6u) << chunked_response.size();

        for (std::string_view str : chunked_response) {
          std::cout << "|" << str << "|\n";
        }
      }).catch_error([](auto&& ec) {
        ut::expect(false) << ec.message();
      }).catch_exception([](auto&&) {
        ut::expect(false);
      });

  };

  underlying_test();
  http_result_test();
  http_config_test();
  status_code_test();
  cookie_test();
  exptr_wrapper_test();
}