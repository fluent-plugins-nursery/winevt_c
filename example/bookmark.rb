require 'winevt'

@query = Winevt::EventLog::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")
@bookmark = Winevt::EventLog::Bookmark.new
while @query.next do
  xml = @query.render
  @bookmark.update(@query)
end

puts @bookmark.render
