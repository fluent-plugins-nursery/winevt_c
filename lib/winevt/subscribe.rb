module Winevt
  class EventLog
    class Subscribe
      alias_method :subscribe_raw, :subscribe

      def subscribe(path, query, bookmark = nil, session = nil)
        if bookmark.is_a?(Winevt::EventLog::Bookmark) &&
           session.is_a?(Winevt::EventLog::Session)
          subscribe_raw(path, query, bookmark.render, session)
        elsif bookmark.is_a?(Winevt::EventLog::Bookmark)
          subscribe_raw(path, query, bookmark.render)
        else
          subscribe_raw(path, query)
        end
      end
    end
  end
end
