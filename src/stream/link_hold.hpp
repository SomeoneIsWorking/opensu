// link_hold — the network kept at its fastest while a stream plays: on Android, Wi-Fi out of its
// power saving.
#pragma once

namespace opensu::stream {

/// Asks the platform for the hold, or lets it go; `link_hold_android.cpp`, else nothing.
void holdLink(bool held);

class LinkHold {
  public:
    LinkHold() {
        holdLink(true);
    }
    ~LinkHold() {
        holdLink(false);
    }
    LinkHold(const LinkHold&) = delete;
    LinkHold& operator=(const LinkHold&) = delete;
    LinkHold(LinkHold&&) = delete;
    LinkHold& operator=(LinkHold&&) = delete;
};

} // namespace opensu::stream
