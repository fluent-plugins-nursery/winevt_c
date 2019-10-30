module Winevt
  class EventLog
    class Subscribe
      alias_method :subscribe_raw, :subscribe
      alias_method :initialize_raw, :initialize
      attr_reader :query_error

      def initialize
        @query_error = nil
        initialize_raw
      end

      def subscribe(path, query, bookmark = nil)
        if bookmark.is_a?(Winevt::EventLog::Bookmark)
          begin
            subscribe_raw(path, query, bookmark.render)
          rescue Winevt::EventLog::Query::Error => e
            @query_error = e
            # fallback to without bookmark version.
            subscribe_raw(path, query)
          end
        else
          subscribe_raw(path, query)
        end
      end
    end
  end
end
