require 'test/unit'
require 'pathname'
require 'png'

class TestEncodeOption < Test::Unit::TestCase
  DATA_DIR = Pathname($0).expand_path.dirname + "data" 

  #
  # :pixel_format
  #

  data(":GRAY", :GRAY)
  data(":GA", :GA)
  data(":RGB", :RGB)
  data(":RGBA", :RGBA)
  data("GRAY", "GRAY")
  data("GA", "GA")
  data("RGB", "RGB")
  data("RGBA", "RGBA")

  test ":pixel_format" do |type|
    enc = assert_nothing_raised {
      PNG::Encoder.new(128, 133, :pixel_format => type)
    }

    dat = (DATA_DIR + "sample_#{type}.bin").binread

    png = assert_nothing_raised {
      enc << dat
    }

    assert_true(dat.bytesize > png.bytesize)
    #IO.binwrite("afo_#{data_label}.png", png)
  end

  data("gray", ["gray", ArgumentError])
  data("number", [1, TypeError])
  data("nil", [nil, TypeError])

  test "bad :pixel_format" do |arg|
    assert_raise_kind_of(arg[1]) {
      PNG::Encoder.new(128, 133, :pixel_format => arg[0])
    }
  end

  #
  # :interlace
  #

  data("true", true)
  data("string", "true")
  data("number", 0)
  data("hash", {})
  data("array", [])

  test "true :interlace" do |val|
    enc = assert_nothing_raised {
      PNG::Encoder.new(128, 133, :pixel_format => :RGBA, :interlace => val)
    }

    png = assert_nothing_raised {
      enc << (DATA_DIR + "sample_RGBA.bin").binread
    }

    met =  PNG::Decoder.new.read_header(png)
    assert_equal("ADAM7", met.interlace_method)
  end

  data("false", false)
  data("nil", nil)

  test "false :interlace" do |val|
    enc = assert_nothing_raised {
      PNG::Encoder.new(128, 133, :pixel_format => :RGBA, :interlace => val)
    }

    png = assert_nothing_raised {
      enc << (DATA_DIR + "sample_RGBA.bin").binread
    }

    met =  PNG::Decoder.new.read_header(png)
    assert_equal("NONE", met.interlace_method)
  end

  #
  # :compression
  #

  data("0", 0)
  data("1", 1)
  data("2", 2)
  data("3", 3)
  data("4", 4)
  data("5", 5)
  data("6", 6)
  data("7", 7)
  data("8", 8)
  data("9", 9)
  data("NO_COMPRESSION", "NO_COMPRESSION")
  data("BEST_SPEED", "BEST_SPEED")
  data("BEST_COMPRESSION", "BEST_COMPRESSION")
  data("DEFAULT", "DEFAULT")

  test ":compression" do |val|
    enc = assert_nothing_raised {
      PNG::Encoder.new(128, 133, :pixel_format => :RGBA, :compression => val)
    }

    dat = (DATA_DIR + "sample_RGBA.bin").binread

    png = assert_nothing_raised {
      enc << dat
    }

    if val != 0 and val != "NO_COMPRESSION"
      assert_true(dat.bytesize > png.bytesize)
    end
  end

  data("-1", [-1, RangeError])
  data("10", [10, RangeError])
  data("true", [true, TypeError])
  data("false", [false, TypeError])
  data("string", ["true", ArgumentError])
  data("hash", [{}, TypeError])
  data("array", [[], TypeError])

  test "bad :compression" do |arg|
    assert_raise_kind_of(arg[1]) {
      PNG::Encoder.new(128, 133, :compression => arg[0])
    }
  end

  #
  # :text
  #

  data("#1", :title => "Test image")
  data("#2", :title => "Test image", :description => "Test description")
  data("#3", :title => "Test image", :creation_time => Time.now.to_s)

  test ":text" do |val|
    enc = assert_nothing_raised {
      PNG::Encoder.new(128, 133, :pixel_format => :RGBA, :text => val)
    }

    dat = (DATA_DIR + "sample_RGBA.bin").binread

    png = assert_nothing_raised {
      enc << dat
    }

    assert_equal(val, PNG::Decoder.new.read_header(png).text)
  end

  data("-1",     [-1,     TypeError])
  data("10",     [10,     TypeError])
  data("true",   [true,   TypeError])
  data("false",  [false,  TypeError])
  data("string", ["true", TypeError])
  data("array",  [[],     TypeError])

  test "bad :text" do |arg|
    assert_raise_kind_of(arg[1]) {
      PNG::Encoder.new(128, 133, :text => arg[0])
    }
  end

  #
  # :time
  #

  data("true", true)
  data("string", "true")
  data("number", 0)
  data("hash", {})
  data("array", [])

  test "true :time" do |val|
    enc = assert_nothing_raised {
      PNG::Encoder.new(128, 133, :pixel_format => :RGBA, :time => val)
    }

    dat = (DATA_DIR + "sample_RGBA.bin").binread

    png = assert_nothing_raised {
      enc << dat
    }

    assert_kind_of(Time, PNG::Decoder.new.read_header(png).time)
  end

  data("false", false)
  data("nil", nil)

  test "false :time" do |val|
    enc = assert_nothing_raised {
      PNG::Encoder.new(128, 133, :pixel_format => :RGBA, :time => val)
    }

    png = assert_nothing_raised {
      enc << (DATA_DIR + "sample_RGBA.bin").binread
    }

    assert_nil(PNG::Decoder.new.read_header(png).time)
  end

  #
  # :gamma
  #
  data("integer", 1)
  data("float", 4.4)
  data("rational", 9/20r)

  test ":gamma" do |val|
    enc = assert_nothing_raised {
      PNG::Encoder.new(128, 133, :pixel_format => :RGBA, :gamma => val)
    }

    dat = (DATA_DIR + "sample_RGBA.bin").binread

    png = assert_nothing_raised {
      enc << dat
    }

    assert_kind_of(Float, PNG::Decoder.new.read_header(png).file_gamma)
  end

  data("true", [true, TypeError])
  data("false", [false, TypeError])
  data("string", ["true", TypeError])
  data("hash", [{}, TypeError])
  data("array", [[], TypeError])

  test "bad :gamma" do |arg|
    assert_raise_kind_of(arg[1]) {
      PNG::Encoder.new(128, 133, :gamma => arg[0])
    }
  end
end
