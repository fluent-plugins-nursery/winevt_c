require "helper"

class WinevtTest < Test::Unit::TestCase
  class QueryTest < self
    def setup
      @query = Winevt::EventLog::Query.new("Application", "*")
    end

    def test_query
      query = "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]"

      assert do
        Winevt::EventLog::Query.new("Application", query)
      end
    end

    def test_next
      assert_true(@query.next)
    end

    def test_timeout
      @query.timeout = 1
      assert_equal(1, @query.timeout)
    end

    def test_offset
      @query.offset = 1
      assert_equal(1, @query.offset)
    end

    def test_each
      @query.offset = 0
      @query.seek(:last)
      @query.each do |xml, message|
        assert(xml)
      end
    end

    data("first symbol" => [true, :first],
         "first string" => [true, "first"],
         "last symbol" => [true, :last],
         "last string" => [true, "last"],
         "Flag::RelativeToLast" => [true, Winevt::EventLog::Query::Flag::RelativeToLast],
         "Flag::RelativeToFirst" => [true, Winevt::EventLog::Query::Flag::RelativeToFirst],
         "valid complex Flag" => [true, Winevt::EventLog::Query::Flag::RelativeToFirst |
                                        Winevt::EventLog::Query::Flag::Strict],
         "invalid flag" => [false, 123]
        )
    def test_seek(data)
      expected, value = data
      assert_equal(expected, @query.seek(value))
    end

    def test_seek_with_invalid_flag
      error = assert_raises ArgumentError do
        @query.seek("hoge");
      end
      assert_equal("Unknown seek flag: hoge", error.message)
    end

    def test_preserve_qualifiers
      assert_false(@query.preserve_qualifiers?)
      @query.preserve_qualifiers = true
      assert_true(@query.preserve_qualifiers?)
    end
  end

  class BookmarkTest < self
    def setup
      @bookmark = Winevt::EventLog::Bookmark.new
      @query = Winevt::EventLog::Query.new("Application", "*")
    end

    def test_query
      query = "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]"
      @bookmark = Winevt::EventLog::Bookmark.new
      assert do
        Winevt::EventLog::Query.new("Application", query)
      end
    end

    def test_update
      @query.next
      assert_true(@bookmark.update(@query))
    end

    def test_update_with_seek_bookmark
      @query.next
      assert_true(@bookmark.update(@query))
      assert_true(@query.seek(@bookmark))
    end

    def test_render
      @query.next
      assert_true(@bookmark.update(@query))
      assert(@bookmark.render)
    end

    def test_render_as_xml
      assert_true(@query.render_as_xml?)
      @query.render_as_xml = false
      assert_false(@query.render_as_xml?)
    end
  end

  class SubscribeTest < self
    def setup
      @bookmark = Winevt::EventLog::Bookmark.new
      @subscribe = Winevt::EventLog::Subscribe.new
      @subscribe.subscribe("Application", "*")
    end

    def test_query
      query = "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]"
      assert do
        @subscribe.subscribe("Application", query)
      end
    end

    def test_subscribe_twice
      subscribe = Winevt::EventLog::Subscribe.new
      subscribe.subscribe("Application", "*")
      assert_true(subscribe.next)
      subscribe.subscribe("Security", "*")
      assert_true(subscribe.next)
    end

    def test_subscribe_with_bookmark
      subscribe = Winevt::EventLog::Subscribe.new
      subscribe.subscribe("Application", "*", @bookmark)
      assert_true(subscribe.next)
    end

    def test_subscribe_without_bookmark
      subscribe = Winevt::EventLog::Subscribe.new
      subscribe.subscribe("Application", "*")
      assert_true(subscribe.next)
    end

    def test_subscribe_invalid_query
      subscribe = Winevt::EventLog::Subscribe.new
      error = assert_raises Winevt::EventLog::Query::Error do
        subscribe.subscribe("", "");
      end
      assert_match(/ErrorCode: 15001\nError: .*\n/, error.message)
    end

    def test_next
      assert_true(@subscribe.next)
    end

    def test_read_existing_events
      assert_true(@subscribe.read_existing_events?)
      @subscribe.read_existing_events = false
      assert_false(@subscribe.read_existing_events?)
    end

    def test_bookmark
      @subscribe.next
      assert(@subscribe.bookmark)
    end

    def test_each
      assert do
        @subscribe.respond_to?(:each)
      end
    end

    def test_rate_limit
      assert_equal(Winevt::EventLog::Subscribe::RATE_INFINITE,
                   @subscribe.rate_limit)
      rate_limit = 50
      @subscribe.rate_limit = rate_limit
      assert_equal(rate_limit, @subscribe.rate_limit)
      @subscribe.rate_limit = Winevt::EventLog::Subscribe::RATE_INFINITE
      assert_equal(Winevt::EventLog::Subscribe::RATE_INFINITE,
                   @subscribe.rate_limit)
      assert_raise(ArgumentError) do
        @subscribe.rate_limit = 3
      end
      assert_raise(ArgumentError) do
        @subscribe.rate_limit = 33
      end
    end

    def test_render_as_xml
      assert_true(@subscribe.render_as_xml?)
      @subscribe.render_as_xml = false
      assert_false(@subscribe.render_as_xml?)
    end

    def test_preserve_qualifiers
      assert_false(@subscribe.preserve_qualifiers?)
      @subscribe.preserve_qualifiers = true
      assert_true(@subscribe.preserve_qualifiers?)
    end
  end

  class ChannelTest < self
    def setup
      @channel = Winevt::EventLog::Channel.new
    end

    def test_each
      assert do
        @channel.each
      end
    end

    def test_force_enumerate
      assert_false(@channel.force_enumerate)
      @channel.force_enumerate = true
      assert_true(@channel.force_enumerate)
    end
  end
end
