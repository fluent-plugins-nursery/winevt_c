module Winevt
  class EventLog
    class Query
      alias_method :each_raw, :each
      def each
        each_raw do |xml, message, string_inserts|
          placeholdered_message = message.gsub(/(%\d+)/, '\1$s')
          replaced_message = sprintf(placeholdered_message, *string_inserts) rescue message.gsub(/(%\d+)/, "?")
          yield(xml, replaced_message, string_inserts)
        end
      end
    end
  end
end
