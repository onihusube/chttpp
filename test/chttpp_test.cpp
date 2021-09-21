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
  /*auto result = chttpp::get(L"https://example.com");

  if (not result) {
    std::cout << result;
    return -1;
  };

  //std::cout << result.status_code() << std::endl;
  //std::cout << result.response_body() << std::endl;
*/

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

  underlying_test();
}