//#define CHTTPP_NOT_GLOBAL_INIT_CURL
#include "chttpp.hpp"

#include <type_traits>
#include <cassert>

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

int main() {
  auto result = chttpp::get(L"https://example.com");

  if (not result) {
    std::cout << result;
    return -1;
  };

  std::cout << result.status() << std::endl;
  std::cout << result.response_body() << std::endl;
}