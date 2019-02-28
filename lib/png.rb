#
# libpng for ruby
#
#   Copyright (C) 2019 Hiroshi Kuwagata <kgt9221@gmail.com>
#

require "png/version"
require "png/png"

module PNG
  class << self
    def read_header(path)
      return PNG::Decoder.new.read_header(IO.binread(path))
    end

    def decode(path)
      return PNG::Decoder.new << IO.binread(path)
    end
  end
end
