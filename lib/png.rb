#
# libpng for ruby
#
#   Copyright (C) 2019 Hiroshi Kuwagata <kgt9221@gmail.com>
#

require "png/version"
require "png/png"

module PNG
  class << self
    def read_header(data)
      return PNG::Decoder.new.read_header(data)
    end

    def decode(png, **opt)
      return PNG::Decoder.new(**opt) << png
    end

    def decode_file(path, **opt)
      return PNG.decode(IO.binread(path), **opt)
    end

    def encode(w, h, raw, **opt)
      return PNG::Encoder.new(w, h, **opt) << raw
    end

    def encode_file(w, h, path, **opt)
      return PNG.encode(w, h, IO.binread(path), **opt)
    end
  end
end
