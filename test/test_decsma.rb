require 'test/unit'
require 'pathname'
require 'png'

class TestDecodeSimplifiedAPI < Test::Unit::TestCase
  DATA_DIR = Pathname($0).expand_path.dirname + "data" 

  #
  # :pixel_format
  #

  data(":GRAY", [:GRAY, 1])
  data(":GA", [:GA, 2])
  data(":RGB", [:RGB, 3])
  data(":RGBA", [:RGBA, 4])
  data("GRAY", ["GRAY", 1])
  data("GA", ["GA", 2])
  data("RGB", ["RGB", 3])
  data("RGBA", ["RGBA", 4])

  test ":pixel_format" do |arg|
    %w{GRAY GA RGB RGBA}.each { |type|
      dec = assert_nothing_raised {
        PNG::Decoder.new(:api_type => :simplified, :pixel_format => arg[0])
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

  test "bad :pixel_format" do |arg|
    assert_raise_kind_of(arg[1]) {
      PNG::Decoder.new(:api_type => :simplified, :pixel_format => arg[0])
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

  test "true :without_meta" do |val|
    dec = assert_nothing_raised {
      PNG::Decoder.new(:api_type => :simplified, :without_meta => val)
    }

    raw = assert_nothing_raised {
      dec << (DATA_DIR + "sample_RGBA.png").binread
    }

    assert_not_respond_to(raw, :meta)
  end

  data("false", false)
  data("nil", nil)

  test "false :without_meta" do |val|
    dec = assert_nothing_raised {
      PNG::Decoder.new(:api_type => :simplified,
                       :pixel_format => :RGBA,
                       :without_meta => val)
    }

    raw = assert_nothing_raised {
      dec << (DATA_DIR + "sample_RGBA.png").binread
    }

    assert_respond_to(raw, :meta)
  end

  #
  # read_header
  #
  test "read_header()" do
    dec = assert_nothing_raised {
      PNG::Decoder.new(:api_type => :simplified)
    }

    met = assert_nothing_raised {
      dec.read_header((DATA_DIR + "sample_RGBA.png").binread)
    }

    assert_kind_of(PNG::Meta, met)
    assert_equal(128, met.width)
    assert_equal(133, met.height)
    assert_equal(8, met.bit_depth)
    assert_equal("RGBA", met.color_type)
    assert_equal("NONE", met.interlace_method)
    assert_equal("BASE", met.compression_method)
    assert_equal("BASE", met.filter_method)
    assert_kind_of(Time, met.time)
  end
end
