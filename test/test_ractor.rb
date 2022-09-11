require 'test/unit'
require 'base64'
require 'png'

Warning[:experimental] = false

class TestRactor < Test::Unit::TestCase
  TEST_DATA = Base64.decode64(<<~EOD)
    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
  EOD

  test "using Ractor" do
    r = Ractor.new(TEST_DATA) { |data|
      PNG::Decoder.new << data
    }

    img = assert_nothing_raised {r.take}
    met = img.meta

    assert_equal(256, met.width)
    assert_equal(224, met.height)
    assert_equal("RGB", met.pixel_format)
    assert_equal(768, met.stride)
    assert_equal(met.stride * met.height, img.bytesize)
  end

  test "using decode result in Ractor" do
    assert_nothing_raised {
      r = Ractor.new(PNG::Decoder.new << TEST_DATA) { |raw|
        raw.meta
      }

      r.take
    }
  end

  test "using encode result in Ractor" do
    raw = PNG::Decoder.new << TEST_DATA
    met = raw.meta
    enc = PNG::Encoder.new(met.width,
                           met.height,
                           :pixel_format => met.pixel_format)

    assert_nothing_raised {
     r = Ractor.new(enc << raw) {|png| png.bytesize}
     r.take
    }
  end

  test "unshareble" do
    assert_false(Ractor.shareable?(PNG::Decoder.new))
    assert_false(Ractor.shareable?(PNG::Encoder.new(200, 200)))
  end

end
