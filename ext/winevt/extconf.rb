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
have_library("advapi32")
have_library("ole32")
# $defs reaches both the C and the C++ compilations; $CFLAGS only reaches C,
# which left the guard in winevt_c.h permanently open in winevt_utils.cpp.
if have_macro("RB_ALLOCV")
  $defs << "-DHAVE_RB_ALLOCV"
end

$LDFLAGS << " -lwevtapi -ladvapi32 -lole32"
$CFLAGS << " -Wall -std=c99 -fPIC -fms-extensions "
$CXXFLAGS << " -Wall -std=c++11 -fPIC -fms-extensions "
# $CFLAGS << " -g -O0 -ggdb"
# $CXXFLAGS << " -g -O0 -ggdb"

create_makefile("winevt/winevt")
