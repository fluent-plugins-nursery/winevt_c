require "helper"

class WinevtTest < Test::Unit::TestCase
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
