// frame_parity — Reed-Solomon parity over a hardware codec's frame, so a lost datagram costs no
// picture: every block of a frame's packets goes with parity packets, and as many packets as a
// block has parity can be rebuilt from what came. A frame's packets are all one size but its last.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace opensu::stream {

/// What parity adds to a frame's bytes.
inline constexpr double parityShare = 0.2;

/// How many parity packets go with `parts` packets.
[[nodiscard]] std::size_t parityFor(std::size_t parts);

/// The parity packets of a frame's `parts`, block by block.
[[nodiscard]] std::vector<std::string> parityOf(std::span<const std::string> parts);

/// Rebuilds what it can of `parts`, by part, from `parity`, by its index among the frame's parity
/// packets. `expected` is how many parts the frame has and `lastBytes` the length of the last.
void restoreParts(std::map<std::uint16_t, std::string>& parts,
                  const std::map<std::uint16_t, std::string>& parity, std::size_t expected,
                  std::size_t lastBytes);

} // namespace opensu::stream
