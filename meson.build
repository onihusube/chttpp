project('chttpp', 'cpp', default_options : ['warning_level=3'], meson_version : '>=0.58.0')

#インクルードディレクトリ
include_dir = include_directories('include')

#コンパイルオプション調整
cppcompiler = meson.get_compiler('cpp').get_argument_syntax()

if cppcompiler == 'msvc'
    dep_libs = []
    # VSプロジェクトに編集しうるファイルを追加する
    vs_files = ['include/chttpp.hpp', 'include/mime_types.hpp', 'include/underlying/winhttp.hpp',
                'include/underlying/common.hpp', 'include/null_terminated_string_view.hpp','test/winhttp_test.hpp',
                'test/http_result_test.hpp', 'include/underlying/http_result.hpp', 'include/underlying/status_code.hpp',
                'test/cookie_test.hpp']
elif cppcompiler == 'gcc'
    curl_dep = dependency('libcurl')
    dep_libs = [curl_dep]
    vs_files = []
endif

if not meson.is_subproject()
# デバッグ時は meson setup build -Dcpp_debugstl=true -Db_sanitize='address,undefined' のようにすることを推奨

# テストプロジェクト用の設定追加
if cppcompiler == 'msvc'
    options = ['/std:c++latest', '/source-charset:utf-8', '/Zc:__cplusplus']
elif cppcompiler == 'gcc'
    options = ['-std=c++2b', '-fvisibility=hidden']
endif

# 依存プロジェクト
boostut_dep = dependency('boost.ut')
picojson_dep = dependency('picojson')

dep_libs += [boostut_dep, picojson_dep]

test1 = executable('chttpp_test', ['test/chttpp_test.cpp', 'test/locally_completed_test.cpp'], include_directories : include_dir, extra_files : vs_files, cpp_args : options, dependencies : dep_libs)
test('chttpp test', test1)

locally_completed_test_obj = test1.extract_objects('test/locally_completed_test.cpp')

test2 = executable('chttpp_non_communication_test', 'test/chttpp_non_communication_test.cpp', objects : locally_completed_test_obj, include_directories : include_dir, cpp_args : options, dependencies : dep_libs)
test('chttpp non communication test', test2)

else

# ライブラリ利用時の設定追加
if cppcompiler == 'msvc'
    options = ['/std:c++latest', '/source-charset:utf-8']
elif cppcompiler == 'gcc'
    options = ['-std=c++2b']
endif

# subprojectとして構築時は依存オブジェクトの宣言だけしとく
chttpp_dep = declare_dependency(include_directories : include_directories('include'), compile_args : options, dependencies : dep_libs)

endif
