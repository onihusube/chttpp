//#define CHTTPP_NOT_GLOBAL_INIT_CURL
#include "chttpp.hpp"

#include <type_traits>
#include <cassert>

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

int main() {
  auto result = chttpp::underlying::test_get("https://example.com");

  if (not result) {
    std::cout << result;
    return;
  };

  std::cout << result.status() << std::endl;
  std::cout << result.response() << std::endl;
}