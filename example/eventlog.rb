require 'winevt'
require 'rexml/document'

@query = Winevt::EventLog::Query.new("Application", "*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")

@query.each do |eventlog, message|
  doc = REXML::Document.new(eventlog)
  nodes = []
  REXML::XPath.each(doc, "/Event/EventData/Data") do |node|
    nodes << node.text
  end
  message = message.gsub(/(%\d+)/, '\1$s')
  message = sprintf(message, *nodes)

  puts ({eventlog: eventlog, data: message})
end
