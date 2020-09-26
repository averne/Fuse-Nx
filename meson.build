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

add_global_arguments('-ffunction-sections', language: ['c', 'cpp'])
add_global_arguments('-fdata-sections', language: ['c', 'cpp'])
add_project_arguments('-fno-rtti', language: ['c', 'cpp'])
add_project_arguments('-DFUSENX_VERSION="@0@"'.format(meson.project_version()), language: ['c', 'cpp'])

exe_deps = []

if get_option('cryptobackend') == 'mbedtls'
    exe_deps += dependency('mbedtls', fallback: ['mbedtls', 'mbedtls_dep'])
else
    crypto_dep = dependency('libgcrypt', required: false)
    if crypto_dep.found()
        add_project_arguments('-DUSE_GCRYPT', language: ['c', 'cpp'])
    else
        crypto_dep = dependency('mbedtls', fallback: ['mbedtls', 'mbedtls_dep'])
    endif
    exe_deps += crypto_dep
endif

if get_option('fuse').enabled()
    exe_deps += dependency('fuse')
    add_project_arguments('-DFUSE_USE_VERSION=26', language: ['c', 'cpp'])
endif

cli11 = subproject('CLI11')
exe_deps += cli11.get_variable('CLI11_dep')
exe_deps += dependency('re2', fallback: ['re2', 're2_dep'])

lib_inc = include_directories('include')
lib_src = []
subdir('lib')

exe_src = []
subdir('src')

fnx_lib = library('fnx',
    lib_src,
    include_directories: lib_inc
)

executable('fuse-nx',
    exe_src,
    include_directories: lib_inc,
    dependencies: exe_deps,
    link_with: fnx_lib,
    cpp_args: '-std=gnu++20', # re2 pkg-config overrides the standard version to c++11
    link_args: '-Wl,--gc-sections',
    install: true,
)
