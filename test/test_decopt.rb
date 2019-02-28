require 'test/unit'
require 'pathname'
require 'png'

class TestDecodeOption < Test::Unit::TestCase
  DATA_DIR = Pathname($0).expand_path.dirname + "data" 

  #
  # :color_type
  #

  data(":GRAY", [:GRAY, 1])
  data(":GA", [:GA, 2])
  data(":RGB", [:RGB, 3])
  data(":RGBA", [:RGBA, 4])
  data("GRAY", ["GRAY", 1])
  data("GA", ["GA", 2])
  data("RGB", ["RGB", 3])
  data("RGBA", ["RGBA", 4])

  test ":color_type" do |arg|
    %w{GRAY GA RGB RGBA}.each { |type|
      dec = assert_nothing_raised {
        PNG::Decoder.new(:color_type => arg[0])
      }

      raw = assert_nothing_raised {
        dec << (DATA_DIR + "sample_#{type}.png").binread
      }

      assert_equal(128 * 133 * arg[1], raw.bytesize)
    }
  end

  data("gray", ["gray", ArgumentError])
  data("number", [1, TypeError])
  data("nil", [nil, TypeError])

  test "bad :color_type" do |arg|
    assert_raise_kind_of(arg[1]) {
      PNG::Decoder.new(:color_type => arg[0])
    }
  end

  #
  # :without_meta
  #
 
  data("true", true)
  data("string", "true")
  data("number", 0)
  data("hash", {})
  data("array", [])

  test "true :time" do |val|
    enc = assert_nothing_raised {
      PNG::Decoder.new(:without_meta => val)
    }

    raw = assert_nothing_raised {
      enc << (DATA_DIR + "sample_RGBA.png").binread
    }

    assert_not_respond_to(:meta, raw)
  end

  data("false", false)
  data("nil", nil)

  test "false :time" do |val|
    enc = assert_nothing_raised {
      PNG::Encoder.new(128, 133, :color_type => :RGBA, :time => val)
    }

    png = assert_nothing_raised {
      enc << (DATA_DIR + "sample_RGBA.bin").binread
    }

    assert_nil(PNG::Decoder.new.read_header(png).time)
  end
end
