project('passgraph', 'cpp', version : '1.0', meson_version : '>=1.1.0',
        default_options : [
            'cpp_std=c++20',
            'warning_level=3',
            'werror=true',
            'buildtype=debugoptimized'
        ]
)

compiler = meson.get_compiler('cpp')

extra_warnings = compiler.get_supported_arguments(
    '-Wshadow', # warn the user if a variable declaration shadows one from a parent context
    '-Wnon-virtual-dtor', # warn the user if a class with virtual functions has a non-virtual destructor. This helps catch hard to track down memory errors
    '-Wold-style-cast', # warn for c-style casts
    '-Wcast-align', # warn for potential performance problem casts
    '-Wunused', # warn on anything being unused
    '-Woverloaded-virtual', # warn if you overload (not override) a virtual function
    '-Wconversion', # warn on type conversions that may lose data
    '-Wsign-conversion', # warn on sign conversions
    '-Wnull-dereference', # warn if a null dereference is detected
    '-Wdouble-promotion', # warn if float is implicit promoted to double
    '-Wformat=2', # warn on security issues around functions that format output (ie printf)
    '-Wimplicit-fallthrough', # warn on statements that fallthrough without an explicit annotation
    '-Wmisleading-indentation', # warn if indentation implies blocks where blocks do not exist
    '-Wduplicated-cond', # warn if if / else chain has duplicated conditions
    '-Wduplicated-branches', # warn if if / else branches have duplicated code
    '-Wlogical-op', # warn about logical operations being used where bitwise were probably wanted
    '-Wuseless-cast', # warn if you perform a cast to the same type
    '-Wsuggest-override' # warn if an overridden member function is not marked 'override' or 'final'
)

add_project_arguments(extra_warnings, language : 'cpp')

sources = files(
    'src/pass_graph.cpp',
    'src/pass_builder.cpp'
)

vulkan_dep = dependency('vulkan')

passgraph_lib = library('passgraph-lib', sources, include_directories : include_directories('src'), dependencies : [vulkan_dep])
passgraph_dep = declare_dependency(
    link_with : passgraph_lib,
    include_directories : include_directories('src'),
    dependencies : [vulkan_dep]
)

gtest_dep = dependency('gtest', main : true, required : false)

if gtest_dep.found()
    tests_exe = executable('passgraph-tests', 'tests/test_passgraph.cpp', dependencies : [gtest_dep, passgraph_dep])
    test('unit', tests_exe, protocol : 'gtest')
endif

subdir('vendor/vma')

# examples
subdir('examples/basic')