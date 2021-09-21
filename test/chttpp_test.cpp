//#define CHTTPP_NOT_GLOBAL_INIT_CURL
#include "chttpp.hpp"

#include <type_traits>
#include <cassert>

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

/* curl
HTTP/2 200 
content-encoding: gzip
accept-ranges: bytes
age: 546118
cache-control: max-age=604800
content-type: text/html; charset=UTF-8
date: Fri, 17 Sep 2021 08:38:37 GMT
etag: "3147526947"
expires: Fri, 24 Sep 2021 08:38:37 GMT
last-modified: Thu, 17 Oct 2019 07:18:26 GMT
server: ECS (sab/5740)
vary: Accept-Encoding
x-cache: HIT
content-length: 648
*/

/* wihttp
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

int main() {
  auto result = chttpp::get(L"http://example.com");

  if (not result) {
    std::cout << result;
    return -1;
  };

  std::cout << result.status_code() << std::endl;
  std::cout << result.response_body() << std::endl;
}