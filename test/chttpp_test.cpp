//#define CHTTPP_NOT_GLOBAL_INIT_CURL
#include "chttpp.hpp"

#include <type_traits>
#include <cassert>

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

namespace ut = boost::ut;

#ifdef _MSC_VER

#include "winhttp_test.hpp"

#else

#include "libcurl_test.hpp"

#endif

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
    ut::expect(headers.contains("ETag"));
    ut::expect(headers["ETag"] == R"("3147526947+ident")"sv);

    // :の後のスペース有無のテスト
    parse_response_header_oneline(headers, "Age:    515403");

    ut::expect(headers.size() == 4);
    ut::expect(headers.contains("Age"));
    ut::expect(headers["Age"] == "515403"sv);

    parse_response_header_oneline(headers, "date:Fri, 17 Sep 2021 08:38:37 GMT");

    ut::expect(headers.size() == 5);
    ut::expect(headers.contains("date"));
    ut::expect(headers["date"] == "Fri, 17 Sep 2021 08:38:37 GMT"sv);

  };

  "null_terminated_string_view"_test = [] {
    using namespace std::string_literals;
    using chttpp::nt_string_view;

    static_assert(std::is_trivially_copyable_v<nt_string_view>);
    static_assert(sizeof(nt_string_view) == sizeof(std::string_view));

    nt_string_view str1{"test string"};
    ut::expect(str1.str_view().size() == 11);
    ut::expect(str1.str_view() == "test string"sv);

    nt_string_view str2{"test string"sv};
    ut::expect(str2.str_view().size() == 11);
    ut::expect(str2.str_view() == "test string"sv);

    nt_string_view str3{"test string"s};
    ut::expect(str3.str_view().size() == 11);
    ut::expect(str3.str_view() == "test string"sv);
  };

  underlying_test();
}