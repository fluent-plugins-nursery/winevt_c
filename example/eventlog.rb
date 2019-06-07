require 'winevt'

@query = Winevt::EventLog::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")

while @query.next do
  eventlog = @query.render
  puts eventlog
end
