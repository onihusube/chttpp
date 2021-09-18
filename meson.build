project('chttpp', 'cpp', default_options : ['warning_level=3', 'cpp_debugstl=true'], meson_version : '>=0.55.0')

#インクルードディレクトリ
include_dir = include_directories('include')

#コンパイルオプション調整
cppcompiler = meson.get_compiler('cpp').get_argument_syntax()

if cppcompiler == 'msvc'
    options = ['/std:c++latest', '/source-charset:utf-8', '/Zc:__cplusplus']
    dep_libs = []
elif cppcompiler == 'gcc'
    options = ['-std=c++2a']
    curl_dep = dependency('libcurl')
    dep_libs = [curl_dep]
endif

#VSプロジェクトに編集しうるファイルを追加する
vs_files = ['include/chttpp.hpp', 'include/underlying/winhttp.hpp', 'include/underlying/common.hpp']


if not meson.is_subproject()

#依存プロジェクト
boostut_dep = dependency('boost.ut')
dep_libs += [boostut_dep]

exe = executable('chttpp_test', 'test/chttpp_test.cpp', include_directories : include_dir, extra_files : vs_files, cpp_args : options, dependencies : dep_libs)
test('chttpp test', exe)

else

# subprojectとして構築時は依存オブジェクトの宣言だけしとく
chttpp_dep = declare_dependency(include_directories : include_directories('include'), cpp_args : options, dependencies : dep_libs)

endif