Pod::Spec.new do |spec|
  spec.name = "libgit2"
  spec.version = "0.19.0"
  spec.summary = "The libgit2 Library."
  spec.homepage = "http://libgit2.github.com"
  spec.license = {
    :type => "GPL v2 (with linking exception)",
    :file => "COPYING"
  }
  spec.authors = "See AUTHORS file",
  spec.source = {
    :git => "https://github.com/libgit2/libgit2.git",
    :tag => "v#{spec.version}"
  }
  spec.source_files =
    "deps/http-parser/*.{h,c}",
    "src/**/*.{h,c}",
    "include/**/*.h"
  spec.exclude_files =
    "**/include/git2/inttypes.h",
    "**/include/git2/stdint.h",
    "**/src/win32/**",
    "**/hash_win32.*",
    "**/src/amiga/**"
  spec.public_header_files = "include/**/*.h"
  # Preserve the layout of headers in the include directory
  spec.header_mappings_dir = "include"
  spec.preserve_paths = "Authors",
  spec.libraries = "z"
  spec.xcconfig = {
    "OTHER_CFLAGS" => "-v", # For debugging #include
    "USE_HEADERMAP" => "NO",
    "HEADER_SEARCH_PATHS" => "\"$(PODS_ROOT)/BuildHeaders/src\"",
    "CLANG_ENABLE_MODULES" => "NO"
  }
  spec.requires_arc = false
end
