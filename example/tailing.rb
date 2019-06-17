require 'winevt'
require 'rexml/document'

@subscribe = Winevt::EventLog::Subscribe.new
@subscribe.tail = true
@subscribe.subscribe("Security", "*[System[(Level <= 4) and TimeCreated[timediff(@SystemTime) <= 86400000]]]")
while (1) do
  if @subscribe.next
    eventlog = @subscribe.render
    message = @subscribe.message
    doc = REXML::Document.new(eventlog)
    nodes = []
    REXML::XPath.each(doc, "/Event/EventData/Data") do |node|
      nodes << node.text
    end
    message = message.gsub(/(%\d+)/, '\1$s')
    message = sprintf(message, *nodes)

    puts ({eventlog: eventlog, data: message})
  else
    printf(".")
    sleep(1)
  end
end
puts @subscribe.bookmark
