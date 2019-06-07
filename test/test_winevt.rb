require "helper"

class WinevtTest < Test::Unit::TestCase
  class QueryTest < self
    def setup
      @query = Winevt::EventLog::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")
    end

    def test_next
      assert_true(@query.next)
    end

    def test_render
      @query.next
      assert(@query.render)
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
      @query.each do |xml|
        assert(xml)
      end
    end

    def test_seek
      assert_true(@query.seek(:first))
      assert_true(@query.seek("first"))
      assert_true(@query.seek(:last))
      assert_true(@query.seek("last"))
    end
  end

  class BookmarkTest < self
    def setup
      @bookmark = Winevt::EventLog::Bookmark.new
      @query = Winevt::EventLog::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")
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
  end
end
