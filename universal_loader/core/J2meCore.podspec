# Builds the J2ME core as a framework for the iOS Flutter runner (see ui_app/ios/Podfile).
# Sources and preprocessor defines are read from CMakeLists.txt so the iOS and CMake builds stay in sync.
cmake = File.read(File.join(__dir__, 'CMakeLists.txt'))
sources = cmake.scan(%r{^\s*(src/\S+\.(?:c|cpp))\s*$}).flatten
missing_sources = sources.reject { |path| File.file?(File.join(__dir__, path)) }
raise "Missing J2meCore sources: #{missing_sources.join(', ')}" unless missing_sources.empty?
defines = cmake[/add_compile_definitions\((.*?)\)/m, 1].split + ['J2ME_CORE_EXPORTS=1']

Pod::Spec.new do |s|
  s.name             = 'J2meCore'
  s.version          = '1.0.0'
  s.summary          = 'J2ME (CLDC/MIDP) virtual machine core for Universal Loader'
  s.homepage         = 'https://github.com/PhamTriHien/j2meloader'
  s.license          = { :type => 'Proprietary' }
  s.author           = 'PhamTriHien'
  s.source           = { :path => '.' }
  s.platform         = :ios, '15.0'
  s.source_files     = sources + ['include/j2me_core.h']
  s.public_header_files = 'include/j2me_core.h'
  # Private headers are reached through relative includes and the search paths below
  s.preserve_paths   = ['include/**/*.h', 'src/**/*.h', 'CMakeLists.txt']
  s.library          = 'c++'
  s.frameworks       = 'AudioToolbox'   # audio/audio_output.cpp (AudioQueue)
  s.pod_target_xcconfig = {
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++20',
    'CLANG_CXX_LIBRARY' => 'libc++',
    'GCC_PREPROCESSOR_DEFINITIONS' => '$(inherited) ' + defines.join(' '),
    'HEADER_SEARCH_PATHS' => '$(inherited) "${PODS_TARGET_SRCROOT}/include" "${PODS_TARGET_SRCROOT}/src" "${PODS_TARGET_SRCROOT}/src/audio/sonivox"',
    # The bytecode interpreter is the hot path; Xcode's default -Os costs noticeable speed
    'GCC_OPTIMIZATION_LEVEL[config=Release]' => '3',
    'GCC_OPTIMIZATION_LEVEL[config=Profile]' => '3',
    'GCC_OPTIMIZATION_LEVEL[config=Debug]' => '2',
    'OTHER_CFLAGS' => '$(inherited) -Wno-unused-parameter -Wno-shorten-64-to-32 -Wno-comma',
    'GCC_WARN_INHIBIT_ALL_WARNINGS' => 'YES',
  }
end
