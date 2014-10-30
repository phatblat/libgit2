Pod::Spec.new do |s|
  s.name          = "libgit2"
  s.version       = "0.21.0-rc1"
  s.summary       = "The libgit2 Library."
  s.description   = <<-DESC
    libgit2 is a portable, pure C implementation of the Git core methods
    provided as a re-entrant linkable library with a solid API, allowing you to
    write native speed custom Git applications in any language which supports C
    bindings.

    _This version of the Podspec is unstable. Experimenting with various options to get it to build correctly._

    Podspec maintained by Ben Chatelain <benchatelain@gmail.com>.
  DESC
  s.homepage      = "http://libgit2.github.com"
  s.license       = {
    :type => "GPL v2 (with linking exception)",
    :file => "COPYING"
  }
  s.authors = "See AUTHORS file"
  s.preserve_paths = "AUTHORS"

  s.source = {
    :git => "https://github.com/libgit2/libgit2.git",
    :tag => "v#{s.version}"
  }
  s.source_files =
    "deps/http-parser/*.{h,c}",
    "src/**/*.{h,c}"

  # Ignore incompatible platforms
  s.exclude_files =
    "include/git2/inttypes.h",
    "include/git2/stdint.h",
    "src/amiga/**",
    "src/hash/hash_win32.*",
    "src/win32/**"

  # Include headers in the correct places
  s.public_header_files = "include/**/*.h"
  s.private_header_files = "src/**/*.h"
#   s.header_dir = "zzz_public"
  s.header_mappings_dir = "include" # Preserve include/git2
  s.preserve_paths = "AUTHORS",

  # Preserve include/git2 folder structure
  s.header_mappings_dir = "include"

  s.ios.deployment_target = "5.0"
  s.osx.deployment_target = "10.7"

  s.requires_arc = false

  s.xcconfig = {
    # -v        -> Debug #include
    # -isysroot -> WIP prevent "redefinition of 'entry'" errors from iPhoneOS.sdk/usr/include/search.h
    "OTHER_CFLAGS" => "-v", # -isysroot /dev/null",

    # WIP include tmp dir with symlinks to necessary includes from iPhoneOS.sdk/usr/include
    # "HEADER_SEARCH_PATHS" => "/tmp/libgit2-Pod",

    # Headermap info
    # http://stackoverflow.com/questions/2596695/controlling-which-project-header-file-xcode-will-include
    # "USE_HEADERMAP" => "NO",
    # "HEADERMAP_INCLUDES_FLAT_ENTRIES_FOR_TARGET_BEING_BUILT" => "NO",

    # From CocoaPods 0.33 or less
    # "HEADER_SEARCH_PATHS" => "\"$(SRCROOT)/Pods/Headers/Build/libgit2\"",

    # CocoaPods 0.34+
    # DOES NOT FIX: Pods-Octopad-ObjectiveGit-prefix.pch:5:9: fatal error: 'Pods-Octopad-environment.h' file not found
    # "HEADER_SEARCH_PATHS" => "\"$(SRCROOT)/Target Support Files/Pods-Octopad\"",

    # This prevents "redefinition of 'entry'" errors in src/indexer.c
    # However, it introduces: Pods-Octopad-ObjectiveGit-prefix.pch:5:9: fatal error: 'Pods-Octopad-environment.h' file not found
    # "CLANG_ENABLE_MODULES" => "NO"
  }

  # Enable the SSL and SSH features
  s.compiler_flags = '-DGIT_SSL', '-DGIT_SSH'

  s.libraries = "z"

  s.dependency 'OpenSSL'
  s.dependency 'libssh2'
end
