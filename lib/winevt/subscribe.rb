module Winevt
  class EventLog
    class Subscribe
      alias_method :each_raw, :each
      def each
        each_raw do |xml, message, string_inserts|
          message = message.gsub(/(%\d+)/, '\1$s')
          message = sprintf(message, *string_inserts) rescue message.gsub(/(%\d+)/, "?")
          yield(xml, message, string_inserts)
        end
      end
    end
  end
end
