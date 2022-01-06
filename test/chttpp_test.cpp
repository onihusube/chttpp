//#define CHTTPP_NOT_GLOBAL_INIT_CURL
#include "chttpp.hpp"

#include <type_traits>
#include <cassert>
#include <forward_list>

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

namespace ut = boost::ut;

#ifdef _MSC_VER

#include "winhttp_test.hpp"

#else

#include "libcurl_test.hpp"

#endif

namespace chttpp_test {

  struct wrap_vec1 {
    std::vector<int> v1;
    std::vector<int> v2;

    auto as_byte_seq() const -> std::span<const char> {
      return { reinterpret_cast<const char*>(this->v1.data()), sizeof(int) * this->v1.size()};
    }
  };

  struct wrap_vec2 {
    std::vector<int> v1;
    std::vector<int> v2;

    friend auto as_byte_seq(const wrap_vec2& self) -> std::span<const char> {
      return {reinterpret_cast<const char *>(self.v2.data()), sizeof(int) * self.v2.size()};
    }
  };
};

int main() {
  using namespace boost::ut::literals;
  using namespace boost::ut::operators::terse;
  using namespace std::string_view_literals;

  "parse_response_header_oneline"_test = [] {
    using chttpp::detail::parse_response_header_oneline;

    chttpp::header_t headers;

    parse_response_header_oneline(headers, "HTTP/1.1 200 OK"sv);

    ut::expect(headers.size() == 1);
    ut::expect(headers.contains("HTTP Ver"));
    ut::expect(headers["HTTP Ver"] == "HTTP/1.1 200 OK"sv);

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
  };

#ifndef _MSC_VER
  "null_terminated_string_view"_test = [] {
    using namespace std::string_literals;
    using chttpp::nt_string_view;

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

  "terse_options"_test = [] {
    auto result = chttpp::options(L"https://example.com");

    !ut::expect(bool(result));
    const auto status_code = result.status_code();
    ut::expect(200 <= status_code and status_code < 300);

    const auto allow = result.response_header("allow");
    ut::expect(5_ull < allow.size());
    ut::expect(allow == "OPTIONS, GET, HEAD, POST"sv);

  };

  "terse_trace"_test = [] {
    auto result = chttpp::trace("https://example.com");

    !ut::expect(bool(result));
    const auto status_code = result.status_code();
    ut::expect(status_code == 405);

  };

#ifndef _MSC_VER


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
    }
    {
      constexpr auto mime = video/３gpp;
      ut::expect(mime == "video/3gpp"sv);
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

#endif

  "terse post"_test = [] {
    using namespace chttpp::mime_types;
    using namespace std::string_view_literals;

#ifndef _MSC_VER
    // 文字列リテラル直渡しの時、リクエストボディに\0が入る
    // char[N]&で渡ってるせいで、C-like構造体の一種として処理されて、全体がシリアライズされている
    auto result = chttpp::post("https://httpbin.org/post", "field1=value1&field2=value2", text/plain);
    //auto result = chttpp::post("https://httpbin.org/post", "field1=value1&field2=value2"sv, text/plain);
#else
    auto result = chttpp::post("https://httpbin.org/post", "field1=value1&field2=value2", "text/plain");
#endif

    !ut::expect(bool(result));

    std::cout << result.response_body();

  };

  underlying_test();
}