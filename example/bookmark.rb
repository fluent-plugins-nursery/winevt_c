require 'winevt'

@query = Winevt::EventLog::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")
@bookmark = Winevt::EventLog::Bookmark.new
@query.each do |xml|
  @bookmark.update(@query)
end

puts @bookmark.render
