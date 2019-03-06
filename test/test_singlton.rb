require 'test/unit'
require 'base64'
require 'pathname'
require 'png'

class TestSingleton < Test::Unit::TestCase
  DATA_DIR = Pathname($0).expand_path.dirname + "data"

  #
  # decode
  #

  test "decode #1" do
    img = assert_nothing_raised {
      PNG.decode((DATA_DIR + "sample_RGBA.png").binread)
    }

    met = img.meta

    assert_equal(128, met.width)
    assert_equal(133, met.height)
    assert_equal("RGB", met.pixel_format)
    assert_equal(384, met.stride)
    assert_equal(met.stride * met.height, img.bytesize)
  end

  test "decode #2" do
    img = assert_nothing_raised {
      PNG.decode_file(DATA_DIR + "sample_RGBA.png", :pixel_format => :RGBA)
    }

    met = img.meta

    assert_equal(128, met.width)
    assert_equal(133, met.height)
    assert_equal("RGBA", met.pixel_format)
    assert_equal(512, met.stride)
    assert_equal(met.stride * met.height, img.bytesize)
  end

  #
  # encode
  #

  test "encode #1" do
    raw = (DATA_DIR + "sample_RGBA.bin").binread
    png = assert_nothing_raised {
      PNG.encode(128, 133, raw, :pixel_format => :RGBA)
    }

    assert_true(png.bytesize < raw.bytesize)
    assert_nothing_raised {PNG.decode(png)}
  end

  test "encode #2" do
    path = DATA_DIR + "sample_RGBA.bin"
    png  = assert_nothing_raised {
      PNG.encode_file(128, 133, path, :pixel_format => :RGBA)
    }

    assert_true(png.bytesize < path.binread.bytesize)
    assert_nothing_raised {PNG.decode(png)}
  end

end
