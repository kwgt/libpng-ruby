# libpng-ruby
libpng interface for ruby.

## Installation

```ruby
gem 'libpng-ruby'
```

And then execute:

    $ bundle

Or install it yourself as:

    $ gem install libpng-ruby

## Usage

### decode sample

```ruby
require 'png'

dec = PNG::Decoder.new(:color_type => :BGR)

p dec.read_header(IO.binread("test.png"))

raw = dec << IO.binread("test.png")
p raw.meta

IO.binwrite("test.bgr", raw)
```

#### decode options
| option | value type | description |
|---|---|---|
| :api_type     | "simplified" or "classic" | |
| :pixel_format | String or Symbol | output format<br>(ignored when to use classic API) |
| :without_meta | Boolean | T.B.D |
| :display_gamma | Numeric | T.B.D<br>(ignored when to use simplified API) |

#### supported output color type
GRAY GRAYSCALE GA AG RGB BGR RGBA ARGB BGRA ABGR

### encode sample

```ruby
require 'png'

enc = PNG::Encoder.new(640, 480, :color_type => :YCbCr)

IO.binwrite("test.png", enc << IO.binread("test.raw"))
```
#### encode options
| option | value type | description |
|---|---|---|
| :pixel_fromat | String or Symbol | input pixel format |
| :interlace    | Boolean          | use interlace mode |
| :compression  | Integer or String or Symbol | compression level |
| :text         | Hash             | text information |
| :time         | Boolean          | with tIME chunk |
| :gamma        | Numeric          | file gamma value |

#### supported input color type
GRAY GRASCALE GA RGB RGBA

#### available compression level 
##### Integer
0 to 9(0:no compression, 9:best compression).

#### String
NO_COMPRESSION BEST_SPEED BEST_COMPRESSION DEFAULT

