require 'winevt'

@query = Winevt::EventLog::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")

@query.each do |eventlog|
  puts eventlog
end
