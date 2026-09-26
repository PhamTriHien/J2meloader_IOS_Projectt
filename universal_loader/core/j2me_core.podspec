Pod::Spec.new do |s|
  s.name             = 'j2me_core'
  s.version          = '1.0.0'
  s.summary          = 'Unified C++20 Core Engine for J2ME Loader'
  s.description      = <<-DESC
    High-performance C++20 J2ME core engine supporting CLDC 1.1 VM, MIDP 2.0 LCDUI,
    M3G 3D, Mascot Capsule Micro3D, Sonivox EAS Synthesizer, JSR-75, and JSR-120.
  DESC
  s.homepage         = 'https://github.com/PhamTriHien/J2meloader_IOS_Projectt'
  s.license          = { :type => 'GPL-3.0' }
  s.author           = { 'Pham Tri Hien' => 'trihien@local' }
  s.source           = { :path => '.' }

  s.ios.deployment_target = '13.0'
  s.osx.deployment_target = '10.15'

  s.source_files = 'src/**/*.{h,hpp,c,cpp}', 'include/**/*.{h,hpp}'
  s.public_header_files = 'include/**/*.h'

  s.pod_target_xcconfig = {
    'CLANG_CXX_LANGUAGE_STANDARD' => 'c++20',
    'CLANG_CXX_LIBRARY' => 'libc++',
    'GCC_PREPROCESSOR_DEFINITIONS' => [
      'UNIFIED_DEBUG_MESSAGES=1',
      'EAS_WT_SYNTH=1',
      'NUM_OUTPUT_CHANNELS=2',
      '_SAMPLE_RATE_22050=1',
      'MAX_SYNTH_VOICES=64',
      '_16_BIT_SAMPLES=1',
      '_FILTER_ENABLED=1',
      'DLS_SYNTHESIZER=1',
      '_REVERB_ENABLED=1',
      '_IMELODY_PARSER=1',
      '_RTTTL_PARSER=1',
      '_OTA_PARSER=1'
    ],
    'HEADER_SEARCH_PATHS' => [
      '"$(PODS_TARGET_SRCROOT)/include"',
      '"$(PODS_TARGET_SRCROOT)/src"',
      '"$(PODS_TARGET_SRCROOT)/src/audio/sonivox"'
    ].join(' ')
  }

  s.libraries = 'c++'
  s.ios.frameworks = 'AudioToolbox', 'CoreGraphics', 'UIKit'
  s.osx.frameworks = 'AudioToolbox', 'CoreGraphics', 'AppKit'
end
