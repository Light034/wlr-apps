project('wayland-toplevel-info', 'cpp', 'c',
  version : '0.1.0',
  meson_version : '>=0.51.0',
  default_options : ['warning_level=3', 'cpp_std=c++20', 'c_std=c11'])

# Dependencies
wayland_dep = dependency('wayland-client')
gtkmm_dep = dependency('gtkmm-3.0')
jsoncpp_dep = dependency('jsoncpp')
wayland_scanner_dep = find_program('wayland-scanner')
wlr_protocols_dep = dependency('wlr-protocols')

# --- wlr-foreign-toplevel-list (NEW) ---
# We don't have a wrap file for this, as it's not a standard
# WLR protocol.  We assume it's installed system-wide or
# provided via wlr-protocols
ext_toplevel_protocol_xml = files('/usr/share/wlr-protocols/unstable/wlr-foreign-toplevel-management-unstable-v1.xml')
# Generate the client header
ext_toplevel_client_header = custom_target('ext_toplevel_client_header',
  input : ext_toplevel_protocol_xml,
  output : 'wlr-foreign-toplevel-management-unstable-v1-client-protocol.h',
  command : [wayland_scanner_dep, 'client-header', '@INPUT@', '@OUTPUT@'],
  build_by_default: true
)

ext_toplevel_public_code = custom_target(
  'ext_toplevel_public_code',
  output : 'wlr-foreign-toplevel-management-unstable-v1-client-protocol.c',
  command : [wayland_scanner_dep, 'public-code', '@INPUT@', '@OUTPUT@'],
  input : ext_toplevel_protocol_xml,
  build_by_default: true
)

add_project_arguments(['-DWLR_USE_UNSTABLE'], language: ['cpp', 'c'])

# Executable
executable('wf-getapps',
  ['src/wf-getapps.cpp', ext_toplevel_public_code, ext_toplevel_client_header],
  dependencies : [wayland_dep, gtkmm_dep, jsoncpp_dep, wlr_protocols_dep],
  install : true,
  include_directories : [include_directories('.','src')],
  build_by_default: true
)
