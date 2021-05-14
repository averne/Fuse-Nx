project('Fuse-NX', ['c', 'cpp'],
    default_options: [
        'buildtype=release',
        'default_library=static',
        'b_ndebug=if-release',
        'b_lto=true',
        'strip=true',
        'warning_level=2',
        'c_std=gnu11',
        'cpp_std=gnu++2a',
        'auto_features=enabled',
    ],
    license: 'GPLv3+',
    version: '1.0.0',
)

cmake = import('cmake')

add_global_arguments('-ffunction-sections', language: ['c', 'cpp'])
add_global_arguments('-fdata-sections', language: ['c', 'cpp'])
add_project_arguments('-fno-rtti', language: ['c', 'cpp'])
add_project_arguments('-DFUSENX_VERSION="@0@"'.format(meson.project_version()), language: ['c', 'cpp'])

exe_deps = []

crypto_dep = dependency('libgcrypt', required: false)
if not crypto_dep.found() or get_option('cryptobackend') == 'mbedtls'
    opts = cmake.subproject_options()
    opts.add_cmake_defines({'ENABLE_TESTING': false, 'ENABLE_PROGRAMS': false})
    crypto_dep = cmake.subproject('mbedtls', options: opts).dependency('mbedcrypto')
else
    add_project_arguments('-DUSE_GCRYPT', language: ['c', 'cpp'])
endif
exe_deps += crypto_dep

if host_machine.system() == 'windows'
    exe_deps += declare_dependency(include_directories: include_directories('C:/Program Files (x86)/WinFsp/inc/fuse'))
    exe_deps += meson.get_compiler('cpp').find_library('winfsp-x64', dirs: 'C:/Program Files (x86)/WinFsp/lib', static: true)
else
    exe_deps += dependency('fuse', required: true)
    add_project_arguments('-DFUSE_USE_VERSION=26', language: ['c', 'cpp'])
endif

cli11_proj = subproject('CLI11')
exe_deps += cli11_proj.get_variable('CLI11_dep')

re2_dep = dependency('re2', required: false)
if not re2_dep.found()
    re2_dep = cmake.subproject('re2').dependency('re2')
endif
exe_deps += re2_dep

if host_machine.system() == 'windows'
    exe_deps += meson.get_compiler('cpp').find_library('Shlwapi', required: true)
endif

lib_inc = include_directories('include')
lib_src = []
subdir('lib')

exe_src = []
subdir('src')

fnx_lib = library('fnx',
    lib_src,
    include_directories: lib_inc,
    dependencies: crypto_dep,
)

exe_ldargs = ['-Wl,--gc-sections']
if host_machine.system() == 'windows'
    exe_ldargs += ['-static', '-static-libgcc']
endif

executable('fuse-nx',
    exe_src,
    include_directories: lib_inc,
    dependencies: exe_deps,
    link_with: fnx_lib,
    cpp_args: '-std=gnu++20', # re2 pkg-config overrides the standard version to c++11
    link_args: exe_ldargs,
    install: true,
)
