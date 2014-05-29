Pod::Spec.new do |s|
  s.name          = "libgit2"
  s.version       = "0.19.0"
  s.summary       = "The libgit2 Library."
  s.description   = <<-DESC
    libgit2 is a portable, pure C implementation of the Git core methods
    provided as a re-entrant linkable library with a solid API, allowing you to
    write native speed custom Git applications in any language which supports C
    bindings.
  DESC
  s.homepage      = "http://libgit2.github.com"
  s.license       = {
    :type => "GPL v2 (with linking exception)",
    :file => "COPYING"
  }
  s.authors = "See AUTHORS file",

  s.source = {
    :git => "https://github.com/libgit2/libgit2.git",
    :tag => "v#{s.version}"
  }
  s.source_files =
    "deps/http-parser/*.{h,c}",
    "src/**/*.{h,c}",
    "include/**/*.h"
  s.exclude_files =
    "**/include/git2/inttypes.h",
    "**/include/git2/stdint.h",
    "**/src/win32/**",
    "**/hash_win32.*",
    "**/src/amiga/**"
  s.public_header_files = "include/**/*.h"
  s.header_mappings_dir = "include" # Preserve include/git2
  s.preserve_paths = "AUTHORS",

  s.libraries = "z"
  s.requires_arc = false
  s.xcconfig = {
    "OTHER_CFLAGS" => "-v", # For debugging #include
    "USE_HEADERMAP" => "NO",
    "HEADER_SEARCH_PATHS" => "\"$(PODS_ROOT)/BuildHeaders/src\"",
    "CLANG_ENABLE_MODULES" => "NO"
  }
end
