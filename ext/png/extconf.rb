require 'mkmf'
require 'optparse'

OptionParser.new { |opt|
  opt.on('--with-png-include=PATH', String) { |path|
    $CFLAGS << " -I#{path}"
  }

  opt.on('--with-png-lib=PATH', String) { |path|
    $LDFLAGS << " -L#{path}"
  }

  opt.parse!(ARGV)
}

have_library( "png16")
have_header( "png.h")
have_header( "zlib.h")

create_makefile( "png/png")
