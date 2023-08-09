#pragma once

#include "chttpp.hpp"

#define BOOST_UT_DISABLE_MODULE
#include <boost/ut.hpp>

void status_code_test() {
  using namespace boost::ut::literals;
  using namespace boost::ut::operators::terse;

  "http_status_code"_test = []
  {
    namespace ut = boost::ut;
    using chttpp::detail::http_status_code;

    {
      http_status_code st{200};

      ut::expect(st.OK());

      ut::expect(not st.Unauthorized());
      ut::expect(not st.Forbidden());
      ut::expect(not st.NotFound());
      ut::expect(not st.RequestTimeout());
      ut::expect(not st.InternalServerError());
      ut::expect(not st.ServiceUnavailable());
    }
    {
      http_status_code st{401};

      ut::expect(st.Unauthorized());

      ut::expect(not st.OK());
      ut::expect(not st.Forbidden());
      ut::expect(not st.NotFound());
      ut::expect(not st.RequestTimeout());
      ut::expect(not st.InternalServerError());
      ut::expect(not st.ServiceUnavailable());
    }
    {
      http_status_code st{403};

      ut::expect(st.Forbidden());

      ut::expect(not st.OK());
      ut::expect(not st.Unauthorized());
      ut::expect(not st.NotFound());
      ut::expect(not st.RequestTimeout());
      ut::expect(not st.InternalServerError());
      ut::expect(not st.ServiceUnavailable());
    }
    {
      http_status_code st{404};

      ut::expect(st.NotFound());

      ut::expect(not st.OK());
      ut::expect(not st.Unauthorized());
      ut::expect(not st.Forbidden());
      ut::expect(not st.RequestTimeout());
      ut::expect(not st.InternalServerError());
      ut::expect(not st.ServiceUnavailable());
    }
    {
      http_status_code st{408};

      ut::expect(st.RequestTimeout());

      ut::expect(not st.OK());
      ut::expect(not st.Unauthorized());
      ut::expect(not st.Forbidden());
      ut::expect(not st.NotFound());
      ut::expect(not st.InternalServerError());
      ut::expect(not st.ServiceUnavailable());
    }
    {
      http_status_code st{500};

      ut::expect(st.InternalServerError());

      ut::expect(not st.OK());
      ut::expect(not st.Unauthorized());
      ut::expect(not st.Forbidden());
      ut::expect(not st.NotFound());
      ut::expect(not st.RequestTimeout());
      ut::expect(not st.ServiceUnavailable());
    }
    {
      http_status_code st{503};

      ut::expect(st.ServiceUnavailable());

      ut::expect(not st.OK());
      ut::expect(not st.Unauthorized());
      ut::expect(not st.Forbidden());
      ut::expect(not st.NotFound());
      ut::expect(not st.RequestTimeout());
      ut::expect(not st.InternalServerError());
    }
    {
      http_status_code st_l{100};
      http_status_code st_u{103};

      ut::expect(st_l.is_informational());
      ut::expect(st_u.is_informational());

      ut::expect(not st_l.is_successful());
      ut::expect(not st_u.is_successful());
      ut::expect(not st_l.is_redirection());
      ut::expect(not st_u.is_redirection());
      ut::expect(not st_l.is_client_error());
      ut::expect(not st_u.is_client_error());
      ut::expect(not st_l.is_server_error());
      ut::expect(not st_u.is_server_error());
    }
    {
      http_status_code st_l{200};
      http_status_code st_u{226};

      ut::expect(st_l.is_successful());
      ut::expect(st_u.is_successful());

      ut::expect(not st_l.is_informational());
      ut::expect(not st_u.is_informational());
      ut::expect(not st_l.is_redirection());
      ut::expect(not st_u.is_redirection());
      ut::expect(not st_l.is_client_error());
      ut::expect(not st_u.is_client_error());
      ut::expect(not st_l.is_server_error());
      ut::expect(not st_u.is_server_error());
    }
    {
      http_status_code st_l{300};
      http_status_code st_u{308};

      ut::expect(st_l.is_redirection());
      ut::expect(st_u.is_redirection());

      ut::expect(not st_l.is_informational());
      ut::expect(not st_u.is_informational());
      ut::expect(not st_l.is_successful());
      ut::expect(not st_u.is_successful());
      ut::expect(not st_l.is_client_error());
      ut::expect(not st_u.is_client_error());
      ut::expect(not st_l.is_server_error());
      ut::expect(not st_u.is_server_error());
    }
    {
      http_status_code st_l{400};
      http_status_code st_u{451};

      ut::expect(st_l.is_client_error());
      ut::expect(st_u.is_client_error());

      ut::expect(not st_l.is_informational());
      ut::expect(not st_u.is_informational());
      ut::expect(not st_l.is_successful());
      ut::expect(not st_u.is_successful());
      ut::expect(not st_l.is_redirection());
      ut::expect(not st_u.is_redirection());
      ut::expect(not st_l.is_server_error());
      ut::expect(not st_u.is_server_error());
    }
    {
      http_status_code st_l{500};
      http_status_code st_u{511};

      ut::expect(st_l.is_server_error());
      ut::expect(st_u.is_server_error());

      ut::expect(not st_l.is_informational());
      ut::expect(not st_u.is_informational());
      ut::expect(not st_l.is_successful());
      ut::expect(not st_u.is_successful());
      ut::expect(not st_l.is_redirection());
      ut::expect(not st_u.is_redirection());
      ut::expect(not st_l.is_client_error());
      ut::expect(not st_u.is_client_error());
    }
  };
}