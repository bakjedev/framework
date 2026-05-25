vma_lib = static_library('vma', 'src/VmaUsage.cpp', include_directories : include_directories('include'), dependencies : dependency('vulkan'), cpp_args : ['-w'])

vma_dep = declare_dependency(link_with : vma_lib, include_directories : include_directories('include'))