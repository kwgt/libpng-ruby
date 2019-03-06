require 'test/unit'
require 'pathname'
require 'png'

class TestDecodeClassicAPI < Test::Unit::TestCase
  DATA_DIR = Pathname($0).expand_path.dirname + "data" 

  #
  # :pixel_format
  #
  data(":RGBA", [:RGBA, 4])
  data("GRAY", ["GRAY", 1])
  data("GA", ["GA", 2])
  data("RGB", ["RGB", 3])
  data("RGBA", ["RGBA", 4])

  test "simple decode" do |arg|
    dec = assert_nothing_raised {
      PNG::Decoder.new(:api_type => :classic, :gamma => 1.0)
    }

    raw = assert_nothing_raised {
      dec << (DATA_DIR + "sample_#{arg[0]}.png").binread
    }

    assert_equal(128 * 133 * arg[1], raw.bytesize)

    if arg[0] == "RGB"
      File.open("sample_#{arg[0]}.ppm", "wb") { |f|
        f.printf("P6\n128 133\n255\n")
        f.write(raw)
      }
    end
    
  end
end
