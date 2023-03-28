#pragma once

#include <string_view>

#include "chttpp.hpp"

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

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
  //using chttpp::detail::apply_set_cookie;
}