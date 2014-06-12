Pod::Spec.new do |s|
  s.name          = "libgit2"
  s.version       = "0.19.0"
  s.summary       = "The libgit2 Library."
  s.description   = <<-DESC
    libgit2 is a portable, pure C implementation of the Git core methods
    provided as a re-entrant linkable library with a solid API, allowing you to
    write native speed custom Git applications in any language which supports C
    bindings.

    # Setup

    Note that at present, after CocoaPods is done installing libgit2 the target
    build will fail due to a header path ordering issue. The current workaround
    is to add the following post_install hook to your `Podfile`.

    ```
    post_install do |installer|
      # Reorder HEADER_SEARCH_PATHS for libgit2
      target = installer.project.targets.find { |t| t.to_s == "Pods-libgit2" }
      target.build_configurations.each do |config|
        # Value will be empty initially since all settings come from xcconfig files
        # Prime it with $(inherited)
        s = config.build_settings['HEADER_SEARCH_PATHS'] ||= ['$(inherited)']

        # Insert new value at the start of HEADER_SEARCH_PATHS
        s.unshift('${PODS_LIBGIT__HEADER_SEARCH_PATHS}')

        config.build_settings['HEADER_SEARCH_PATHS'] = s
      end
    end
    ```

    Better solutions welcome via [pull request](https://github.com/phatblat/Podspecs) :)
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
