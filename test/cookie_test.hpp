#pragma once

#include <string_view>
#include <ranges>
#include <algorithm>

#include "chttpp.hpp"

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

namespace ut = boost::ut;

/*
ChatGPTに教えてもらったテストケース

HTTPのSet-Cookieヘッダに設定できる値の例を10個挙げます。

名前と値を含む単一のクッキー： Set-Cookie: name=value
セッションクッキー： Set-Cookie: name=value; Path=/
期限付きのクッキー： Set-Cookie: name=value; Expires=Wed, 21 Oct 2015 07:28:00 GMT
セキュアクッキー： Set-Cookie: name=value; Secure
HTTP専用クッキー： Set-Cookie: name=value; HttpOnly
サードパーティークッキー： Set-Cookie: name=value; Domain=example.com
クッキーの値を複数設定する： Set-Cookie: name1=value1; name2=value2
クッキーの有効期限を設定する： Set-Cookie: name=value; Max-Age=3600
サブドメイン専用のクッキー： Set-Cookie: name=value; Domain=example.com; Path=/; Domain=blog.example.com
クッキーの優先度を指定する： Set-Cookie: name=value; SameSite=Strict

以下に、より複雑な例をいくつか挙げます。

クッキー名にスペースを含む場合： Set-Cookie: user id=12345; Expires=Wed, 21 Oct 2015 07:28:00 GMT
クッキーの値にセミコロンを含む場合： Set-Cookie: name="John; Smith"; Expires=Wed, 21 Oct 2015 07:28:00 GMT
複数のクッキーを同時に設定する場合： Set-Cookie: name1=value1; Expires=Wed, 21 Oct 2015 07:28:00 GMT, name2=value2; Expires=Wed, 21 Oct 2015 07:28:00 GMT
クッキーの値にUTF-8エンコードされた文字を含む場合： Set-Cookie: name=%E3%81%82%E3%81%84%E3%81%86%E3%81%88; Expires=Wed, 21 Oct 2015 07:28:00 GMT
クッキーの値にBase64エンコードされたバイナリデータを含む場合： Set-Cookie: name=YmFzZTY0IGRhdGE=; Expires=Wed, 21 Oct 2015 07:28:00 GMT
複数の属性を同時に設定する場合： Set-Cookie: name=value; Expires=Wed, 21 Oct 2015 07:28:00 GMT; Path=/; Domain=example.com; Secure; HttpOnly; SameSite=Strict

以下に、より複雑なHTTPのSet-Cookieヘッダの例をいくつか挙げます。

SameSite属性をStrictからLaxに設定する場合： Set-Cookie: name=value; SameSite=Lax; Path=/; Domain=example.com; Expires=Wed, 21 Oct 2015 07:28:00 GMT
クッキーの値をJSON形式で設定する場合： Set-Cookie: session={"id":12345,"name":"John"}; Path=/; HttpOnly; SameSite=Strict
セキュア属性とSameSite属性を同時に設定する場合： Set-Cookie: name=value; Secure; SameSite=Strict; Path=/; Domain=example.com
複数のドメインにクッキーを設定する場合： Set-Cookie: name=value; Domain=example.com,example.org; Path=/
クッキーの有効期限を現在の日時から1日後に設定する場合： Set-Cookie: name=value; Expires=" + new Date(Date.now() + 86400000).toUTCString() + "; Path=/; Domain=example.com; HttpOnly; SameSite=Strict
Secure属性が設定されたクッキーをHTTP経由で送信した場合にエラーを発生させる場合： Set-Cookie: name=value; Secure; Path=/; Domain=example.com; SameSite=Strict; __Secure- flag
プライバシー関連の属性を設定する場合： Set-Cookie: name=value; Path=/; Domain=example.com; SameSite=Lax; Expires=Wed, 21 Oct 2015 07:28:00 GMT; Secure; HttpOnly; __Host- prefix; Priority=High

*/

void cookie_test() {
  using namespace boost::ut::literals;
  using chttpp::detail::cookie_store;
  using chttpp::detail::cookie;
  using chttpp::detail::cookie_ref;

  constexpr auto cmp_cookie_all = [](const cookie& lhs, const cookie& rhs) -> bool {
    return lhs == rhs and lhs.value == rhs.value and lhs.expires == rhs.expires and lhs.secure == rhs.secure;
  };

  "simple_cookie"_test = [cmp_cookie_all]
  {
    cookie_store cookies{};

    cookies.insert_from_set_cookie("name=value");
    {
      cookie c{.name = "name", .value = "value"};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }
    cookies.insert_from_set_cookie("name1=value1; Path=/path");
    {
      ut::expect(cookies.size() == 2);

      cookie c{.name = "name1", .value = "value1", .path="/path"};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }
    cookies.insert_from_set_cookie("name2=value2; Secure");
    {
      ut::expect(cookies.size() == 3);

      cookie c{.name = "name2", .value = "value2", .secure = true};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }
    cookies.insert_from_set_cookie("name3=value3; HttpOnly");
    {
      ut::expect(cookies.size() == 4);

      cookie c{.name = "name3", .value = "value3"};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }
    cookies.insert_from_set_cookie("name4=value4; Domain=example.com");
    {
      ut::expect(cookies.size() == 5);

      cookie c{.name = "name4", .value = "value4", .domain = "example.com"};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }
    cookies.insert_from_set_cookie("name5=value5; name6=value6; name7=value7");
    {
      ut::expect(cookies.size() == 8);
      cookie cs[] = {{.name = "name5", .value = "value5"}, {.name = "name6", .value = "value6"}, {.name = "name7", .value = "value7"}};

      for (const auto& c : cs)
      {
        const auto pos = cookies.find(c);

        ut::expect(pos != cookies.end());
        ut::expect(cmp_cookie_all(*pos, c));
      }
    }
    cookies.insert_from_set_cookie("name=value; Expires=Wed, 21 Oct 2015 07:28:00 GMT");
    {
      // 上書きしてる
      ut::expect(cookies.size() == 8);
#if 201907L <= __cpp_lib_chrono
      using namespace std::chrono_literals;

      constexpr std::chrono::sys_days ymd = 2015y/10/21d;
      auto time = std::chrono::clock_cast<std::chrono::system_clock>(ymd) + 7h + 28min + 0s;
#else
      std::tm tm{ .tm_sec = 0, .tm_min = 28, .tm_hour = 7, .tm_mday = 21, .tm_mon = 10 - 1, .tm_year = 2015 - 1900, .tm_wday = -1, .tm_yday = -1, .tm_isdst = 0, .tm_gmtoff{}, .tm_zone{} };
      const auto time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
#endif
      cookie c{.name = "name", .value = "value", .expires = time};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }
    cookies.insert_from_set_cookie("Path=/path; Domain=example.com; Expires=Wed, 21 Oct 2015 07:28:00 GMT; name1=skip; HttpOnly; Path=/path; Secure");
    {
      ut::expect(cookies.size() == 8);

      cookie c{.name = "name1", .value = "skip", .path = "/path", .secure = true};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }
    {
      // 少なくとも処理前時刻+max-age以上の値になるはず
      auto time = std::chrono::system_clock::now();
      time += std::chrono::seconds{3600};
      
      cookies.insert_from_set_cookie("name3=maxage; Max-Age=3600");
      
      ut::expect(cookies.size() == 8);

      cookie c{.name = "name3", .value = "maxage", .expires = time};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());

      const auto expires = (*pos).expires;
      ut::expect(time <= expires);
      ut::expect(expires < std::chrono::system_clock::time_point::max());
    }
    cookies.insert_from_set_cookie("hostspec=test; Path=/path; Secure", "example.com");
    {
      cookie c{.name = "hostspec", .value = "test", .domain = "example.com", .path = "/path", .secure = true};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }
  };

  "duplicate_cookie"_test = [] {
    cookie_store cookies{};

    // name, path, domainの3つ組によってクッキーの等価性が決まる
    cookies.insert_from_set_cookie("name=value1");
    cookies.insert_from_set_cookie("name=value2; Path=/path/path");
    cookies.insert_from_set_cookie("name=value3; Domain=example.com");
    cookies.insert_from_set_cookie("name=value4; Domain=example.com; Path=/path/path");

    ut::expect(cookies.size() == 4);
  };

  "invalid_cookie"_test = [] {
    cookie_store cookies{};

    // これらは全て受理されない
    cookies.insert_from_set_cookie("");
    cookies.insert_from_set_cookie("; ");
    cookies.insert_from_set_cookie("    ;      ");
    cookies.insert_from_set_cookie(";");
    cookies.insert_from_set_cookie("=; =");
    cookies.insert_from_set_cookie("Expires=Wed, 21 Oct 2015 07:28:00 GMT; Secure; HttpOnly");
    cookies.insert_from_set_cookie("=NoName");
    cookies.insert_from_set_cookie("noeq");
    cookies.insert_from_set_cookie("noeq;");
    cookies.insert_from_set_cookie("noeq; ");

    ut::expect(cookies.size() == 0) << cookies.size();
  };

  "overlapping attributes"_test = [cmp_cookie_all] {
    cookie_store cookies{};

    cookies.insert_from_set_cookie("name=value; Secure; Secure; Secure; HttpOnly; HttpOnly");
    {
      ut::expect(cookies.size() == 1);

      cookie c{ .name = "name", .value = "value", .secure = true };

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }

    cookies.insert_from_set_cookie("domain=test; Domain=example.com; Domain=example.jp; Domain=example.net; Domain=google.com");
    {
      ut::expect(cookies.size() == 2);

      cookie c{ .name = "domain", .value = "test", .domain="google.com"};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }

    cookies.insert_from_set_cookie("path=test; Path=/path/path; Path=/path/path/path; Path=/; Path=/test/path/test");
    {
      ut::expect(cookies.size() == 3);

      cookie c{ .name = "path", .value = "test", .path = "/test/path/test" };

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }
    {
      // 少なくとも処理前時刻+max-age以上の値になるはず
      auto time = std::chrono::system_clock::now();
      time += std::chrono::seconds{ 1000 };

      cookies.insert_from_set_cookie("maxage=test; Max-Age=0; Max-Age=1; Max-Age=3600; Max-Age=1000");
      ut::expect(cookies.size() == 4);

      cookie c{ .name = "maxage", .value = "test" };

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());

      const auto expires = (*pos).expires;
      ut::expect(time <= expires);
      ut::expect(expires < std::chrono::system_clock::time_point::max());
    }
    cookies.insert_from_set_cookie("expires=test; Expires=Sun, 23 Sep 2001 17:09:32 GMT; Expires=Tue, 16 Feb 1993 07:02:53 GMT; Expires=Wed, 21 Oct 2015 07:28:00 GMT");
    {
      ut::expect(cookies.size() == 5);

#if 201907L <= __cpp_lib_chrono
      using namespace std::chrono_literals;

      constexpr std::chrono::sys_days ymd = 2015y / 10 / 21d;
      auto time = std::chrono::clock_cast<std::chrono::system_clock>(ymd) + 7h + 28min + 0s;
#else
      std::tm tm{ .tm_sec = 0, .tm_min = 28, .tm_hour = 7, .tm_mday = 21, .tm_mon = 10 - 1, .tm_year = 2015 - 1900, .tm_wday = -1, .tm_yday = -1, .tm_isdst = 0, .tm_gmtoff{}, .tm_zone{} };
      const auto time = std::chrono::system_clock::from_time_t(std::mktime(&tm));
#endif

      cookie c{ .name = "expires", .value = "test", .expires = time };

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }
  };

  "strange cookie"_test = [cmp_cookie_all] {
    cookie_store cookies{};

    cookies.insert_from_set_cookie("user id=12345");
    {
      ut::expect(cookies.size() == 1);

      cookie c{ .name = "user id", .value = "12345" };

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }

    cookies.insert_from_set_cookie(R"(name="John; Smith"; Path=/path/)");
    {
      ut::expect(cookies.size() == 2);

      cookie c{ .name = "name", .value = R"("John)" };

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }

    cookies.insert_from_set_cookie("Inva(l)idToken=Inva(l)idToken");
    {
      ut::expect(cookies.size() == 3);

      cookie c{ .name = "Inva(l)idToken", .value = "Inva(l)idToken"};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }

    cookies.insert_from_set_cookie("InvalidSyntax = InvalidSyntax;Secure;");
    {
      ut::expect(cookies.size() == 4);

      cookie c{ .name = "InvalidSyntax", .value = "InvalidSyntax", .secure = true };

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());
      ut::expect(cmp_cookie_all(*pos, c));
    }

    {
      // MAx-ageは変化しない（負の数指定は無視される）
      const auto time = std::chrono::system_clock::now();

      cookies.insert_from_set_cookie("foo=bar; Domain=example.com; Path=/; Max-Age=-10");

      ut::expect(cookies.size() == 5);

      cookie c{ .name = "foo", .value = "bar", .domain = "example.com", .expires = time};

      const auto pos = cookies.find(c);

      ut::expect(pos != cookies.end());

      const auto expires = (*pos).expires;
      ut::expect(time <= expires);
      ut::expect(expires < std::chrono::system_clock::time_point::max());
    }

    cookies.insert_from_set_cookie(R"($Version=1;Customer="WILE_E_COYOTE"    ; $Path= "/acme";   Part_Number ="Rocket_Launcher_0001" ; Shipping        =      "FedEx")");
    {
      ut::expect(cookies.size() == 10);

      std::pair<chttpp::string_t, chttpp::string_t> namevalue[] = {
        {"$Version", "1"},
        {"Customer", R"("WILE_E_COYOTE")"},
        {"$Path", R"("/acme")"},
        {"Part_Number", R"("Rocket_Launcher_0001")"},
        {"Shipping", R"("FedEx")"}
      };

      for (auto&& [name, value] : namevalue) {
        cookie c{ .name = name, .value = value };

        const auto pos = cookies.find(c);

        ut::expect(pos != cookies.end());
        ut::expect(cmp_cookie_all(*pos, c));
      }
    }
  };

  "cookie_ref"_test = [] {
    {
      std::vector<cookie_ref> for_sort;
      for_sort.emplace_back(std::make_pair("samename2", "ord6"));
      for_sort.emplace_back(std::make_pair("samename", "ord4"), "/lll");
      for_sort.emplace_back(std::make_pair("samename", "ord5"));
      for_sort.emplace_back(std::make_pair("samename3", "ord7"), "/abcd");
      for_sort.emplace_back(std::make_pair("samename3", "ord9"), "/ab");

      const auto epoch0 = std::chrono::system_clock::now();
      const auto epoch1 = std::chrono::system_clock::now() + std::chrono::seconds{1};

      cookie cooks[] = {
        { .name = "samename", .value = "ord3", .path = "/llll", .create_time = epoch1 },
        { .name = "samename", .value = "ord1", .path = "/lllll" , .create_time = epoch0 },
        { .name = "a", .value = "ord0", .path = "/lllll" , .create_time = epoch0 },
        { .name = "samename3", .value = "ord8", .path = "/efg" , .create_time = epoch0 },
        { .name = "samename", .value = "ord2", .path = "/llll" , .create_time = epoch0 }
      };

      for (const auto& c : cooks) {
        for_sort.emplace_back(c);
      }

      // 名前は昇順、Pathは長さによる降順、作成時刻は昇順
      std::ranges::sort(for_sort);

      std::string_view corect[] = {
          "ord0",
          "ord1",
          "ord2",
          "ord3",
          "ord4",
          "ord5",
          "ord6",
          "ord7",
          "ord8",
          "ord9"
      };

      ut::expect(std::ranges::size(corect) == for_sort.size()) << for_sort.size();

      // クッキー値の一致を見ることで間接的にソートの正確性をチェック
      ut::expect(std::ranges::equal(corect, for_sort, {}, {}, &cookie_ref::value));
    }
  };

  "create_cookie_list_to"_test = []
  {
    using chttpp::detail::url_info;

    cookie_store cookies{};
    std::array<std::pair<std::string_view, std::string_view>, 0> add_cookie{};

    // ドメイン名のマッチング確認
    {
      cookies.insert(cookie{.name = "test1", .value = "v", .domain = "example.com", .path = "/"});
      cookies.insert(cookie{.name = "test2", .value = "v", .domain = "www.example.com", .path = "/"});
      cookies.insert(cookie{.name = "test3", .value = "v", .domain = "www.abcdef.example.com", .path = "/"});
      cookies.insert(cookie{.name = "test4", .value = "v", .domain = "", .path = "/"}); // 例外対応
      cookies.insert(cookie{.name = "ignore1", .value = "vv", .domain = "google.com", .path = "/"});
      cookies.insert(cookie{.name = "ignore2", .value = "vv", .domain = "bing.com", .path = "/"});
      cookies.insert(cookie{.name = "ignore3", .value = "vv", .domain = "wwwexample.com", .path = "/"});

      ut::expect(cookies.size() == 7u);

      std::vector<cookie_ref> for_sort;
      url_info ui{"http://example.com/"};
      cookies.create_cookie_list_to(for_sort, add_cookie, ui);

      ut::expect(for_sort.size() == 4u) << for_sort.size();

      std::string_view corect[] = {
          "test1",
          "test2",
          "test3",
          "test4"
      };
      // クッキー名の一致を見ることで間接的にソートの正確性をチェック
      ut::expect(std::ranges::equal(corect, for_sort, {}, {}, &cookie_ref::name));
    }

    cookies.clear();

    // パスのマッチング確認
    {
      cookies.insert(cookie{.name = "test1", .value = "v", .domain = "example.com", .path = "/"});
      cookies.insert(cookie{.name = "test2", .value = "v", .domain = "example.com", .path = "/abc/"});
      cookies.insert(cookie{.name = "test3", .value = "v", .domain = "example.com", .path = "/abc"});
      cookies.insert(cookie{.name = "test4", .value = "v", .domain = "example.com", .path = "/abc/def"});
      cookies.insert(cookie{.name = "ignore1", .value = "v", .domain = "example.com", .path = "/abcdef"});
      cookies.insert(cookie{.name = "ignore2", .value = "v", .domain = "example.com", .path = "/ab"});
      cookies.insert(cookie{.name = "ignore3", .value = "v", .domain = "example.com", .path = "/abc/def/ghi"});
      // 多分これはマッチしないはず・・・
      // https://please-sleep.cou929.nu/cookie-path-behavior-difference-of-browsers.html
      cookies.insert(cookie{.name = "ignore4", .value = "v", .domain = "example.com", .path = "/abc/def/"});

      ut::expect(cookies.size() == 8u);

      std::vector<cookie_ref> for_sort;
      url_info ui{"http://example.com/abc/def"};
      cookies.create_cookie_list_to(for_sort, add_cookie, ui);

      ut::expect(for_sort.size() == 4u) << for_sort.size();

      std::string_view corect[] = {
          "test1",
          "test2",
          "test3",
          "test4"
      };
      // クッキー名の一致を見ることで間接的にソートの正確性をチェック
      ut::expect(std::ranges::equal(corect, for_sort, {}, {}, &cookie_ref::name));
    }

    cookies.clear();

    // Secure属性の確認
    {
      // Scure属性を持つクッキーはhttpで送信されない
      cookies.insert(cookie{.name = "test1", .value = "v", .domain = "example.com", .path = "/", .secure = false});
      cookies.insert(cookie{.name = "ignore1", .value = "v", .domain = "example.com", .path = "/", .secure = true});

      ut::expect(cookies.size() == 2u);

      std::vector<cookie_ref> for_sort;
      url_info ui{"http://example.com/"};
      cookies.create_cookie_list_to(for_sort, add_cookie, ui);

      ut::expect(for_sort.size() == 1u) << for_sort.size();

      std::string_view corect[] = {
          "test1"
      };
      // クッキー名の一致を見ることで間接的にソートの正確性をチェック
      ut::expect(std::ranges::equal(corect, for_sort, {}, {}, &cookie_ref::name));
    }
  };

  "url_info"_test = [] {
    using chttpp::detail::url_info;

    // ホスト部分のテスト
    {
      url_info ui1{"https://example.com"};
      url_info ui2{"http://example.com"};
      url_info ui3{"example.com"};
      url_info ui4{"https://example.com/"};
      url_info ui5{"http://example.com#fragment"};
      url_info ui6{"example.com?query"};

      ut::expect(ui1.is_valid());
      ut::expect(ui2.is_valid());
      ut::expect(ui3.is_valid());
      ut::expect(ui4.is_valid());
      ut::expect(ui5.is_valid());
      ut::expect(ui6.is_valid());

      ut::expect(ui1.secure());
      ut::expect(not ui2.secure());
      ut::expect(ui3.secure());
      ut::expect(ui4.secure());
      ut::expect(not ui5.secure());
      ut::expect(ui6.secure());

      ut::expect(not ui1.is_ip_host());
      ut::expect(not ui2.is_ip_host());
      ut::expect(not ui3.is_ip_host());
      ut::expect(not ui4.is_ip_host());
      ut::expect(not ui5.is_ip_host());
      ut::expect(not ui6.is_ip_host());

      ut::expect(ui1.host() == "example.com");
      ut::expect(ui2.host() == "example.com");
      ut::expect(ui3.host() == "example.com");
      ut::expect(ui4.host() == "example.com");
      ut::expect(ui5.host() == "example.com");
      ut::expect(ui6.host() == "example.com");

      ut::expect(ui1.request_path() == "/");
      ut::expect(ui2.request_path() == "/");
      ut::expect(ui3.request_path() == "/");
      ut::expect(ui4.request_path() == "/");
      ut::expect(ui5.request_path() == "/");
      ut::expect(ui6.request_path() == "/");
    }

    // パス部分のテスト
    {
      url_info ui1{"https://example.com/path/path/path"};
      url_info ui2{"http://example.com/path/path/path"};
      url_info ui3{"example.com/path/path/path"};

      ut::expect(ui1.is_valid());
      ut::expect(ui2.is_valid());
      ut::expect(ui3.is_valid());

      ut::expect(ui1.secure());
      ut::expect(not ui2.secure());
      ut::expect(ui3.secure());

      ut::expect(not ui1.is_ip_host());
      ut::expect(not ui2.is_ip_host());
      ut::expect(not ui3.is_ip_host());

      ut::expect(ui1.host() == "example.com");
      ut::expect(ui2.host() == "example.com");
      ut::expect(ui3.host() == "example.com");

      ut::expect(ui1.request_path() == "/path/path/path");
      ut::expect(ui2.request_path() == "/path/path/path");
      ut::expect(ui3.request_path() == "/path/path/path");
    }

    // IPアドレスによるホスト構成のテスト
    {
      url_info ui1{"http://127.0.0.1:8080"};
      url_info ui2{"http://[::1]:8080"};
      url_info ui3{"https://192.168.100.141/path"};
      url_info ui4{"https://[2001:DB8:0:0:8:800:200C:417A]/path"};
      url_info ui5{"user:pass@127.0.0.1:8080/"};
      url_info ui6{"user:pass@[::1]:8080/"};
      url_info ui7{"http://255.255.255.255"};
      url_info ui8{"http://0.0.0.0"};

      ut::expect(ui1.is_valid());
      ut::expect(ui2.is_valid());
      ut::expect(ui3.is_valid());
      ut::expect(ui4.is_valid());
      ut::expect(ui5.is_valid());
      ut::expect(ui6.is_valid());
      ut::expect(ui7.is_valid());
      ut::expect(ui8.is_valid());

      ut::expect(not ui1.secure());
      ut::expect(not ui2.secure());
      ut::expect(ui3.secure());
      ut::expect(ui4.secure());
      ut::expect(ui5.secure());
      ut::expect(ui6.secure());
      ut::expect(not ui7.secure());
      ut::expect(not ui8.secure());

      ut::expect(ui1.is_ip_host());
      ut::expect(ui2.is_ip_host());
      ut::expect(ui3.is_ip_host());
      ut::expect(ui4.is_ip_host());
      ut::expect(ui5.is_ip_host());
      ut::expect(ui6.is_ip_host());
      ut::expect(ui7.is_ip_host());
      ut::expect(ui8.is_ip_host());

      ut::expect(ui1.is_ipv4_host());
      ut::expect(ui2.is_ipv6_host());
      ut::expect(ui3.is_ipv4_host());
      ut::expect(ui4.is_ipv6_host());
      ut::expect(ui5.is_ipv4_host());
      ut::expect(ui6.is_ipv6_host());
      ut::expect(ui7.is_ipv4_host());
      ut::expect(ui8.is_ipv4_host());

      ut::expect(ui1.host() == "127.0.0.1:8080");
      ut::expect(ui2.host() == "[::1]:8080");
      ut::expect(ui3.host() == "192.168.100.141");
      ut::expect(ui4.host() == "[2001:DB8:0:0:8:800:200C:417A]");
      ut::expect(ui5.host() == "127.0.0.1:8080");
      ut::expect(ui6.host() == "[::1]:8080");
      ut::expect(ui7.host() == "255.255.255.255");
      ut::expect(ui8.host() == "0.0.0.0");

      ut::expect(ui1.request_path() == "/");
      ut::expect(ui2.request_path() == "/");
      ut::expect(ui3.request_path() == "/path");
      ut::expect(ui4.request_path() == "/path");
      ut::expect(ui5.request_path() == "/");
      ut::expect(ui6.request_path() == "/");
      ut::expect(ui7.request_path() == "/");
      ut::expect(ui8.request_path() == "/");
    }

    {
      url_info ui1{"://example.com"};
      url_info ui2{"httpexample.com"};
      url_info ui3{""};
      url_info ui4{":/example.com"};
      url_info ui5{"/example.com"};
      url_info ui6{"http:example.com"};
      url_info ui7{"http:/example.com"};
      url_info ui8{"http//example.com"};
      url_info ui9{"http/example.com"};
      url_info ui10{"httpsexample.com"};
      url_info ui11{"https:example.com"};
      url_info ui12{"https:/example.com"};
      url_info ui13{"https//example.com"};
      url_info ui14{"https/example.com"};
      url_info ui15{"http://user:pass@/"};

      ut::expect(ui1.is_valid() == false);
      ut::expect(ui2.is_valid() == false);
      ut::expect(ui3.is_valid() == false);
      ut::expect(ui4.is_valid() == false);
      ut::expect(ui5.is_valid() == false);
      ut::expect(ui6.is_valid() == false);
      ut::expect(ui7.is_valid() == false);
      ut::expect(ui8.is_valid() == false);
      ut::expect(ui9.is_valid() == false);
      ut::expect(ui10.is_valid() == false);
      ut::expect(ui11.is_valid() == false);
      ut::expect(ui12.is_valid() == false);
      ut::expect(ui13.is_valid() == false);
      ut::expect(ui14.is_valid() == false);
      ut::expect(ui15.is_valid() == false);
    }
  };

  "url_info::append_path()"_test = [] {
    using chttpp::detail::url_info;

    url_info ui1{"https://example.com"};

    ut::expect(ui1.request_path() == "/");
    {
      [[maybe_unused]]
      auto token = ui1.append_path("append/path/path");

      auto path = ui1.request_path();

      ut::expect(path == "/append/path/path");
    }
    ut::expect(ui1.request_path() == "/");
    {
      [[maybe_unused]]
      auto token = ui1.append_path("/another/path");

      auto path = ui1.request_path();

      ut::expect(path == "/another/path");
    }
    ut::expect(ui1.request_path() == "/");


    // 最初からパスがある場合
    url_info ui2{"https://example.com/base/path"};

    ut::expect(ui2.request_path() == "/base/path");
    {
      [[maybe_unused]]
      auto token = ui2.append_path("/addpath/path");
      ut::expect(ui2.request_path() == "/base/path/addpath/path");
    }
    ut::expect(ui2.request_path() == "/base/path");
    {
      [[maybe_unused]]
      auto token = ui2.append_path("continue/path");
      ut::expect(ui2.request_path() == "/base/pathcontinue/path");
    }
    ut::expect(ui2.request_path() == "/base/path");
    {
      // クエリ文字列は無視される
      [[maybe_unused]]
      auto token = ui2.append_path("/query?param=value");
      ut::expect(ui2.request_path() == "/base/path/query");
    }
    ut::expect(ui2.request_path() == "/base/path");
    {
      // アンカーも無視される
      [[maybe_unused]]
      auto token = ui2.append_path("/anchor#abcdefg");
      ut::expect(ui2.request_path() == "/base/path/anchor");
    }
    ut::expect(ui2.request_path() == "/base/path");


    // '/'で終わる場合
    url_info ui3{"https://httpbin.org/"};
    ut::expect(ui3.request_path() == "/");
    {
      [[maybe_unused]] auto token = ui3.append_path("/redirect-to");

      auto path = ui3.request_path();

      ut::expect(path == "/redirect-to");
    }
    ut::expect(ui3.request_path() == "/");

  };

  "agent test check"_test = [] {
    using chttpp::detail::url_info;

    url_info ui{"https://httpbin.org/cookies"};
    cookie_store cookies{};
    cookies.insert(cookie{.name = "preset", .value = "cookie", .domain = "httpbin.org", .path = "/cookies"});

    std::array<std::pair<std::string_view, std::string_view>, 0> add_cookie{};

    std::vector<cookie_ref> for_sort;
    cookies.create_cookie_list_to(for_sort, add_cookie, ui);

    ut::expect(for_sort.size() == 1u);
  };
}