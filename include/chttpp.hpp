#pragma once

#include <string_view>
#include <chrono>
#include <functional>
#include <utility>

#include "underlying/common.hpp"
#include "null_terminated_string_view.hpp"
//#include "mime_types.hpp"

namespace chttpp::underlying::agent_impl {

  struct dummy_buffer {};

  template<typename CharT>
  struct determin_buffer;

  template<typename CharT>
  using determin_buffer_t = determin_buffer<CharT>::type;
}

#ifdef _MSC_VER

#include "underlying/winhttp.hpp"

#else

#include "underlying/libcurl.hpp"

#endif


namespace chttpp {

  namespace detail {

    template<typename T>
    inline constexpr bool is_specialization_of_span_v = false;

    template<typename T>
    inline constexpr bool is_specialization_of_span_v<std::span<T>> = true;

    template<typename T>
    inline constexpr bool is_character_ptr_v = false;

    template<character T>
    inline constexpr bool is_character_ptr_v<const T*> = true;

    template<typename T>
    inline constexpr bool is_character_literal_v = false;

    template<character T, std::size_t N>
    inline constexpr bool is_character_literal_v<const T(&)[N]> = true;

    template<character T, std::size_t N>
    inline constexpr bool is_character_literal_v<const T[N]> = true;

    template<character T, std::size_t N>
    inline constexpr bool is_character_literal_v<T[N]> = true;

    template<typename T>
    inline constexpr bool is_specialization_of_string_view_v = false;

    template<typename T>
    inline constexpr bool is_specialization_of_string_view_v<std::basic_string_view<T>> = true;

    template<typename T>
    inline constexpr bool is_specialization_of_string_v = false;

    template<typename T, typename Alloc>
    inline constexpr bool is_specialization_of_string_v<std::basic_string<T, std::char_traits<T>, Alloc>> = true;

    template <typename T>
    concept string_like =
      is_character_ptr_v<std::remove_cvref_t<T>> or 
      is_character_literal_v<T> or 
      is_specialization_of_string_view_v<std::remove_cvref_t<T>> or
      is_specialization_of_string_v<std::remove_cvref_t<T>>;

    template<typename T>
    struct string_like_traits;

    template <typename T>
    struct string_like_traits<std::basic_string_view<T>> {
      using element_type = T;
    };

    template <typename T, typename Alloc>
    struct string_like_traits<std::basic_string<T, std::char_traits<T>, Alloc>> {
      using element_type = T;
    };

    template<character T>
    struct string_like_traits<const T*> {
      using element_type = T;
    };

    template<character T, std::size_t N>
    struct string_like_traits<T[N]> {
      using element_type = T;
    };

    struct as_byte_seq_impl {

      /**
      * @brief 0. 文字列をバイト列へ変換する
      * @details 利用側はこの結果を直接span<const char>を受け取る関数へ渡すことを想定するので右辺値が来ても良い
      */
      template<string_like S>
      [[nodiscard]]
      auto operator()(S&& str) const noexcept -> std::span<const char> {
        using CharT = string_like_traits<std::remove_cvref_t<S>>::element_type;

        // string_viewに変換してからシリアライズする
        std::basic_string_view<CharT> str_view{str};
        return {reinterpret_cast<const char *>(std::ranges::data(str_view)), sizeof(CharT) * std::ranges::size(str_view)};
      }

      /**
      * @brief 1. contiguous_rangeな範囲をバイト列へ変換する
      * @details 利用側はこの結果を直接span<const char>を受け取る関数へ渡すことを想定するので右辺値が来ても良い
      */
      template<std::ranges::contiguous_range R>
        requires (not string_like<R>) and
                 requires(R& t) {
                   std::ranges::data(t);
                 } and
                 std::ranges::sized_range<R>
      [[nodiscard]]
      auto operator()(R&& t) const noexcept -> std::span<const char> {
        return {reinterpret_cast<const char*>(std::ranges::data(t)), sizeof(std::ranges::range_value_t<R>) * std::ranges::size(t)};
      }

      /**
      * @brief 2. as_byte_seq()メンバ関数を呼び出し、その結果をその型の値のバイト列として取得する
      * @details 利用側はこの結果を直接span<const char>を受け取る関数へ渡すことを想定
      * @details 従ってユーザー定義as_byte_seq()は右辺値を返しても良い
      */
      template<typename T>
        requires requires(T&& t) {
          { std::forward<T>(t).as_byte_seq() } -> std::convertible_to<std::span<const char>>;
        }
      [[nodiscard]]
      decltype(auto) operator()(T&& t) const noexcept(noexcept(std::forward<T>(t).as_byte_seq())) {
        return std::forward<T>(t).as_byte_seq();
      }

      /**
      * @brief 3. as_byte_seq()非メンバ関数を呼び出し、その結果をその型の値のバイト列として取得する
      * @details 利用側はこの結果を直接span<const char>を受け取る関数へ渡すことを想定
      * @details 従ってユーザー定義as_byte_seq()は右辺値を返しても良い
      */
      template<typename T>
        requires requires(T&& t) {
          { as_byte_seq(std::forward<T>(t)) } -> std::convertible_to<std::span<const char>>;
        }
      [[nodiscard]]
      decltype(auto) operator()(T&& t) const noexcept(noexcept(as_byte_seq(std::forward<T>(t)))) {
        return as_byte_seq(std::forward<T>(t));
      }

      /**
      * @brief 4. C-likeな構造体のオブジェクトをそのままシリアライズする
      */
      template<substantial T>
        requires (not is_specialization_of_span_v<T>) and (not string_like<const T&>)
      [[nodiscard]]
      auto operator()(const T& t) const noexcept -> std::span<const char> {
        return {reinterpret_cast<const char*>(std::addressof(t)), sizeof(t)};
      }

      /**
      * @brief 5. span<T>をspan<const char>へ変換する
      */
      template<typename T>
      [[nodiscard]]
      auto operator()(std::span<T> s) const noexcept -> std::span<const char> {
        return {reinterpret_cast<const char*>(s.data()), s.size_bytes()};
      }
    };

    struct load_byte_seq_impl {

      template<typename T>
        requires requires(T& t, std::span<const char> bytes) {
          t.load_byte_seq(bytes);
        }
      void operator()(T& t, std::span<const char> bytes) const noexcept(noexcept(t.load_byte_seq(bytes))) {
        t.load_byte_seq(bytes);
      }

      template<typename T>
        requires requires(T& t, std::span<const char> bytes) {
          load_byte_seq(t, bytes);
        }
      void operator()(T& t, std::span<const char> bytes) const noexcept(noexcept(load_byte_seq(t, bytes))) {
        load_byte_seq(t, bytes);
      }

      /**
      * @brief 3. C-likeな構造体のオブジェクト1つをバイト列からロードする
      */
      template<substantial T>
        requires (not is_specialization_of_span_v<T>)
      void operator()(T& t, std::span<const char> bytes) const {
        assert(sizeof(T) <= bytes.size());
        // memcpyによってデシリアライズ
        std::memcpy(std::addressof(t), bytes.data(), sizeof(T));
      }

      /**
      * @brief 4. C-likeな構造体の連続範囲へバイト列からロードする
      */
      template<std::ranges::contiguous_range R>
        requires substantial<std::ranges::range_value_t<R>>
      void operator()(R& r, std::span<const char> bytes) const {
        // 要素型
        using T = std::ranges::range_value_t<R>;

        // 宛先に確保されている長さ
        std::size_t dst_len = std::ranges::distance(r);

        // 短い方のサイズに合わせる
        const std::size_t len = std::min(dst_len, bytes.size() / sizeof(T));

        // memcpyによってデシリアライズ
        std::memcpy(std::ranges::data(r), bytes.data(), len * sizeof(T));
      }

      /**
      * @brief 5. C-likeな構造体の範囲へバイト列からロードする
      */
      template<std::ranges::forward_range R>
        requires substantial<std::ranges::range_value_t<R>>
      void operator()(R& r, std::span<const char> bytes) const {
        // 要素型
        using T = std::ranges::range_value_t<R>;

        // 宛先に確保されている長さ
        std::size_t dst_len = std::ranges::distance(r);

        // 短い方のサイズに合わせる
        const std::size_t len = std::min(dst_len, bytes.size() / sizeof(T));

        // 1要素づつコピー
        std::ranges::copy_n(reinterpret_cast<const T*>(bytes.data()), len, std::ranges::begin(r));
      }
    };
  }

  inline namespace cpo {
    /**
    * @brief オブジェクトをバイトシーケンスへ変換する
    * @details as_byte_seq(E);のように呼び出し、Eの示すオブジェクトをバイト列へ変換する
    * @return std::span<const char>へ変換可能な型の値
    */
    inline constexpr detail::as_byte_seq_impl as_byte_seq{};

    /**
    * @brief バイトシーケンスから所望のデータを読み出す
    * @details load_byte_seq(E, bytes);のように呼び出し、Eの示すオブジェクトへbytesから読み込んだ値をロードする
    * @return なし
    */
    inline constexpr detail::load_byte_seq_impl load_byte_seq{};
  }

  inline namespace concepts {

    /**
    * @brief バイト列として扱うことのできる型
    */
    template<typename T>
    concept byte_serializable = requires(T&& t) {
      { cpo::as_byte_seq(t) } -> std::convertible_to<std::span<const char>>;
    };

    /**
    * @brief バイト列からロード可能な型
    */
    template<typename T>
    concept byte_deserializable = requires(T& t, std::span<const char> bytes) {
      cpo::load_byte_seq(t, bytes);
    };
  }
}

namespace chttpp::inline traits {

  /**
   * @brief content-typeヘッダの値を自動取得する
   * @tparam T リクエストボディに指定する型
   * @details リクエストボディの型から取得する、カスタマイズのためには部分特殊化するか、静的メンバ変数ContentTypeを定義する
   */
  template<typename T>
  inline constexpr std::string_view query_content_type = []{ static_assert([]{return false;}(), "Primary template is ill-formed"); }();

  /**
   * @brief デフォルト
   */
  template<byte_serializable T>
  inline constexpr std::string_view query_content_type<T> = "application/octet_stream";

  /**
   * @brief 文字列
   */
  template<byte_serializable T>
    requires detail::string_like<T>
  inline constexpr std::string_view query_content_type<T> = "text/plain";

  /**
   * @brief カスタマイズポイント2、静的メンバ変数ContentTypeから取得
   */
  template<byte_serializable T>
    requires requires {
      { T::ContentType } -> std::convertible_to<std::string_view>;
    }
  inline constexpr std::string_view query_content_type<T> = T::ContentType;
}

namespace chttpp::detail {

  template<typename MethodTag>
  struct terse_req_impl {

    using tag_t = MethodTag;

    auto operator()(nt_string_view URL, request_config_for_get cfg = {}) const noexcept -> http_result {
      return chttpp::underlying::terse::request_impl(URL, std::move(cfg), std::span<const char>{}, MethodTag{});
    }

    auto operator()(nt_wstring_view URL, request_config_for_get cfg = {}) const noexcept -> http_result {
      return chttpp::underlying::terse::request_impl(URL, std::move(cfg), std::span<const char>{}, MethodTag{});
    }
  };

  template<detail::tag::has_reqbody_method MethodTag>
  struct terse_req_impl<MethodTag> {

    using tag_t = MethodTag;

    template<byte_serializable Body>
    auto operator()(nt_string_view URL, Body&& request_body, request_config cfg = {}) const noexcept -> http_result {
      // なければデフォ値をセット（実行時の状態に基づいて決められた方が良い・・・？
      if (cfg.content_type.empty()) {
        cfg.content_type = query_content_type<std::remove_cvref_t<Body>>;
      }
      // ここ、request_bodyの完全転送の必要あるかな・・・？
      return chttpp::underlying::terse::request_impl(URL, std::move(cfg), cpo::as_byte_seq(request_body), MethodTag{});
    }

    template<byte_serializable Body>
    auto operator()(nt_wstring_view URL, Body&& request_body, request_config cfg = {}) const noexcept -> http_result {
      // なければデフォ値をセット（実行時の状態に基づいて決められた方が良い・・・？
      if (cfg.content_type.empty()) {
        cfg.content_type = query_content_type<std::remove_cvref_t<Body>>;
      }
      return chttpp::underlying::terse::request_impl(URL, std::move(cfg), cpo::as_byte_seq(request_body), MethodTag{});
    }
  };
}

namespace chttpp::inline method_object {

  inline constexpr detail::terse_req_impl<detail::tag::get_t> get{};
  inline constexpr detail::terse_req_impl<detail::tag::head_t> head{};
  inline constexpr detail::terse_req_impl<detail::tag::options_t> options{};
  inline constexpr detail::terse_req_impl<detail::tag::trace_t> trace{};

  inline constexpr detail::terse_req_impl<detail::tag::post_t> post{};
  inline constexpr detail::terse_req_impl<detail::tag::put_t> put{};
  inline constexpr detail::terse_req_impl<detail::tag::delete_t> delete_{};
}

namespace chttpp {

  template<typename CharT>
    requires requires {
      requires std::is_same_v<CharT, char> or std::is_same_v<CharT, wchar_t>;
    }
  class agent {
    using string = basic_string_t<CharT>;
    using string_view = std::basic_string_view<CharT>;

    // URLの頭の部分
    string m_base_url;

    // URLの変換に使用するバッファ（CharTによっては空のクラス）
    [[no_unique_address]]
    underlying::agent_impl::determin_buffer_t<CharT> convert_buffer{};

    // session_stateやバッファ等のリソースのまとめ
    underlying::agent_impl::agent_resource m_resource;

    // 初期化や各種設定中に起きたエラーを記録する
    detail::error_code m_config_ec;

  // 基本関数群
  public:

    [[nodiscard]]
    agent(string_view base_url, detail::agent_initial_config initial_cfg = {})
      : m_base_url(base_url)
      , m_resource{ 
                    .config = std::move(initial_cfg),
                    .cookie_management = {chttpp::cookie_management::enable},
                    .follow_redirect = {chttpp::follow_redirects::enable},
                    .auto_decomp = {chttpp::automatic_decompression::enable},
                    .request_url{underlying::to_string(m_base_url)}
                  }
      , m_config_ec{ m_resource.state.init(base_url, convert_buffer, m_resource.config) }
    {
      if (bool(m_config_ec) == false and m_resource.request_url.is_valid() == false) {
        // 指定されたURLが変であるか、変換エラー
        m_config_ec = detail::error_code{underlying::lib_error_code_tratis::url_error_value};
      }
    }

    agent(const agent&) = delete;
    agent& operator=(const agent&) = delete;

    agent(agent&&) = default;
    agent& operator=(agent&&) & = default;

    template<auto Method>
    auto request(string_view url_path, detail::agent_request_config req_cfg = {}) & noexcept -> detail::http_result {
      using tag = decltype(Method)::tag_t;

      if (m_config_ec) {
        return detail::http_result{m_config_ec};
      }

      return underlying::agent_impl::request_impl(url_path, convert_buffer, m_resource, std::move(req_cfg), std::span<const char>{}, tag{});
    }

    template<auto Method, byte_serializable Body>
      requires detail::tag::has_reqbody_method<typename decltype(Method)::tag_t>
    auto request(string_view url_path, Body&& request_body, detail::agent_request_config req_cfg = {}) & noexcept -> detail::http_result {
      using tag = decltype(Method)::tag_t;

      if (m_config_ec) {
        return detail::http_result{m_config_ec};
      }

      // なければデフォ値をセット（実行時の状態に基づいて決められた方が良い・・・？
      // ヘッダで指定してた時はこれどうなるの？？
      if (req_cfg.content_type.empty()) {
        req_cfg.content_type = query_content_type<std::remove_cvref_t<Body>>;
      }

      return underlying::agent_impl::request_impl(url_path, convert_buffer, m_resource, std::move(req_cfg), cpo::as_byte_seq(request_body), tag{});
    }

    auto get(string_view url_path, detail::agent_request_config req_cfg = {}) & noexcept -> detail::http_result {
      return this->request<::chttpp::get>(url_path, std::move(req_cfg));
    }

    auto post(string_view url_path, byte_serializable auto&& request_body, detail::agent_request_config req_cfg = {}) & noexcept -> detail::http_result {
      return this->request<::chttpp::post>(url_path, request_body, std::move(req_cfg));
    }

  private:

    void merge_header(umap_t<string_t, string_t>&& add_headers) {
      auto &header_map = m_resource.headers;

      // 重複ヘッダは上書きする
      // 一部、リスト化して良いヘッダというものも存在するので、要検討
      for (auto&& [key, value] : std::move(add_headers)) {
        header_map.insert_or_assign(std::move(key), std::move(value));
      }
    }

    void merge_cookie(detail::cookie_store&& cookies) {
      auto &cookie_vault = m_resource.cookie_vault;

      // 以降の処理は、アロケータの一致を前提とする（agent初期化後にset_default_resource()を変更されるとしぬ）
      assert(cookie_vault.get_allocator() == cookies.get_allocator());

      if (not m_resource.request_url.secure()) {
        // secure属性付き（secure=true）のクッキーは無視する
        // デフォルトはsecure=falseなので、気にしなければ全てのクッキーが保存される
        erase_if(cookies, [](const auto& c) { return c.secure; });
      }

      // 既に存在するものについては上書き
      // あらかじめ重複を削除してからマージする
      erase_if(cookie_vault, [&](const auto& c) {
        return cookies.contains(c);
      });

      // 指定クッキーをマージ
      cookie_vault.merge(cookies);

      // 空になるはず
      assert(cookies.empty());
    }

    void config_impl(chttpp::cookie_management cfg) {
      this->m_resource.cookie_management = cfg;
    }

    void config_impl(chttpp::follow_redirects cfg) {
      this->m_resource.follow_redirect = cfg;
    }

    void config_impl(chttpp::automatic_decompression cfg) {
      this->m_resource.auto_decomp = cfg;
    }

    // 未対応or知らない設定項目
    void config_impl(...) = delete;

  // 各種設定変更
  public:

    void set_headers(umap_t<string_t, string_t> headers) & 
      requires true
    {
      merge_header(std::move(headers));
    }

    void set_headers(std::initializer_list<std::pair<string_view, string_view>> headers) & {
      umap_t<string_t, string_t> tmp{};

      for (auto [name, value] : headers) {
        tmp.emplace(string_t{name}, string_t{value});
      }

      merge_header(std::move(tmp));
    }

    [[nodiscard]]
    auto headers(umap_t<string_t, string_t> headers) && -> agent&& 
      requires true
    {
      merge_header(std::move(headers));
      return std::move(*this);
    }

    [[nodiscard]]
    auto headers(std::initializer_list<std::pair<string_view, string_view>> headers) && -> agent && {
      this->set_headers(headers);
      return std::move(*this);
    }

    void set_cookies(detail::cookie_store cookies) & {
      merge_cookie(std::move(cookies));
    }

    [[nodiscard]]
    auto cookies(detail::cookie_store cookies) && -> agent&& {
      merge_cookie(std::move(cookies));
      return std::move(*this);
    }

    void set_configs(auto... cfgs) & requires (0 < sizeof...(cfgs)) {
      (this->config_impl(cfgs), ...);
    }

    [[nodiscard]]
    auto configs(auto... cfgs) && -> agent&&
      requires (0 < sizeof...(cfgs))
    {
      (this->config_impl(cfgs), ...);
      return std::move(*this);
    }

  // 状態の覗き見
  public:

    [[nodiscard]]
    std::ranges::input_range auto inspect_header() const & {
      const auto& headers = this->m_resource.headers;
      return std::ranges::subrange{headers.cbegin(), headers.cend()};
    }

    [[nodiscard]]
    std::ranges::input_range auto inspect_cookie() const & {
      const auto& cookies = this->m_resource.cookie_vault;
      return std::ranges::subrange{cookies.cbegin(), cookies.cend()};
    }

    [[nodiscard]]
    auto inspect_config() const & {
      return std::make_tuple(this->m_resource.config, this->m_resource.cookie_management, this->m_resource.follow_redirect, this->m_resource.auto_decomp);
    }
  };

  template<typename... Args>
  agent(std::string_view, detail::request_config = {}, Args&&...) -> agent<char>;

  template<typename... Args>
  agent(std::wstring_view, detail::request_config = {}, Args&&...) -> agent<wchar_t>;
}