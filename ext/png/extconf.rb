require 'mkmf'

have_library( "png16")
have_header( "png.h")
have_header( "zlib.h")

create_makefile( "png/png")
