require "helper"

class WinevtTest < Test::Unit::TestCase
  class QueryTest < self
    def setup
      @query = Win32::Winevt::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")
    end

    def test_next
      assert_true(@query.next)
    end

    def test_render
      @query.next
      assert(@query.render)
    end
  end

  class BookmarkTest < self
    def setup
      @bookmark = Win32::Winevt::Bookmark.new
      @query = Win32::Winevt::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")
    end

    def test_update
      @query.next
      assert_true(@bookmark.update(@query))
    end

    def test_render
      @query.next
      assert_true(@bookmark.update(@query))
      assert(@bookmark.render)
    end
  end
end
