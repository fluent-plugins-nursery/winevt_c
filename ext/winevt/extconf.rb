require "mkmf"
require "rbconfig"

if RbConfig::CONFIG['host_os'] =~ /mingw/
  $CFLAGS << ' -fno-omit-frame-pointer'
end

libdir = RbConfig::CONFIG["libdir"]
includedir = RbConfig::CONFIG["includedir"]

dir_config("winevt", includedir, libdir)

have_library("wevtapi")
have_func("EvtQuery", "winevt.h")

$LDFLAGS << " -lwevtapi"
$CFLAGS << " -std=c99 -fPIC -fms-extensions "
# $CFLAGS << " -g -O0"

create_makefile("winevt/winevt")
