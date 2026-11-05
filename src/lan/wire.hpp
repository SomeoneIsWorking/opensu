// wire — the LAN protocol's messages as the JSON bodies both sides send, the one place each is
// written and read. Keys and nonces travel as lower-case hex; anything malformed reads as nothing.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "secrets/crypto.hpp"

namespace opensu::lan::wire {

/// A string field of a document, empty when it is absent or not a string.
[[nodiscard]] std::string textField(const nlohmann::json& document, const char* key);

/// `GET /lan/hello`: who answers, the one thing an unpaired openSU learns.
struct Hello {
    std::string id;
    std::string name;
};
[[nodiscard]] std::string write(const Hello& hello);
[[nodiscard]] std::optional<Hello> readHello(std::string_view body);

/// `POST /lan/pair`: a guest's request, committed to its key.
struct PairRequest {
    std::string id;
    std::string name;
    secrets::crypto::Bytes commitment;
};
[[nodiscard]] std::string write(const PairRequest& request);
[[nodiscard]] std::optional<PairRequest> readPairRequest(std::string_view body);

/// The host's answer to it: its key and nonce for this pairing.
struct PairOffer {
    secrets::crypto::Bytes key;
    secrets::crypto::Bytes nonce;
};
[[nodiscard]] std::string write(const PairOffer& offer);
[[nodiscard]] std::optional<PairOffer> readPairOffer(std::string_view body);

/// `POST /lan/pair/reveal`: the guest's key and nonce.
struct PairReveal {
    std::string id;
    secrets::crypto::Bytes key;
    secrets::crypto::Bytes nonce;
};
[[nodiscard]] std::string write(const PairReveal& reveal);
[[nodiscard]] std::optional<PairReveal> readPairReveal(std::string_view body);

/// `POST /lan/pair/status`: which guest asks, and the host's word back.
struct PairPoll {
    std::string id;
};
[[nodiscard]] std::string write(const PairPoll& poll);
[[nodiscard]] std::optional<PairPoll> readPairPoll(std::string_view body);

struct PairVerdict {
    std::string verdict;
    std::string signature;
};
[[nodiscard]] std::string write(const PairVerdict& verdict);
[[nodiscard]] std::optional<PairVerdict> readPairVerdict(std::string_view body);

/// One game of `GET /lan/library`.
struct ListedGame {
    std::string id;
    std::string title;
    /// Its cover's token (`isCoverToken`); empty when it has none.
    std::string cover;
    /// While it runs there, the profiles (linked copies keep the id) playing a seat in it.
    std::vector<std::string> playingAs;

    bool operator==(const ListedGame&) const = default;
};

/// A cover token is this many bytes, sent as hex.
inline constexpr std::size_t coverTokenBytes = 16;

/// Whether `text` can be a cover token: `coverTokenBytes` as lower-case hex.
[[nodiscard]] bool isCoverToken(std::string_view text) noexcept;

/// `{"games":[{"id","title","cover"}]}`.
[[nodiscard]] std::string write(const std::vector<ListedGame>& games);
/// A listing read back; nothing when `body` is not one. Games without an id or a title are left
/// out, and covers that are not tokens dropped.
[[nodiscard]] std::optional<std::vector<ListedGame>> readListing(std::string_view body);

/// The routes of profile copies, for paired peers.
inline constexpr std::string_view profilesRoute = "/lan/profiles";
inline constexpr std::string_view profileCopyRoute = "/lan/profiles/copy";
inline constexpr std::string_view profileCopyStatusRoute = "/lan/profiles/copy/status";
inline constexpr std::string_view profileUnlinkRoute = "/lan/profiles/unlink";

/// One profile of `GET /lan/profiles`, and the one a copy takes.
struct ListedProfile {
    std::string id;
    std::string name;
    std::uint8_t picture{0};
    /// It is linked with the openSU that asked.
    bool linked{false};

    bool operator==(const ListedProfile&) const = default;
};
/// `{"profiles":[{"id","name","picture","linked"}]}`.
[[nodiscard]] std::string write(const std::vector<ListedProfile>& profiles);
/// A listing read back; nothing when `body` is not one. Entries without a profile id or a name
/// are left out, a picture past the last one wraps.
[[nodiscard]] std::optional<std::vector<ListedProfile>> readProfileListing(std::string_view body);

/// `POST /lan/profiles/copy`, `/copy/status` and `/unlink`: which profile of the host.
struct ProfileAsk {
    std::string profile;
};
[[nodiscard]] std::string write(const ProfileAsk& ask);
/// Nothing when `body` is not one or its profile is not a profile id.
[[nodiscard]] std::optional<ProfileAsk> readProfileAsk(std::string_view body);

/// The answer to `/copy/status`: `waiting`, `accepted` (with the profile) or `declined`.
struct CopyVerdict {
    std::string verdict;
    ListedProfile profile;
};
[[nodiscard]] std::string write(const CopyVerdict& verdict);
/// Nothing when `body` is not one, or an accepted one carries no profile.
[[nodiscard]] std::optional<CopyVerdict> readCopyVerdict(std::string_view body);

/// The routes a paired peer asks for the saves of a profile linked with it.
inline constexpr std::string_view saveStatusRoute = "/lan/saves/status";
inline constexpr std::string_view saveGetRoute = "/lan/saves/get";
inline constexpr std::string_view saveNewsRoute = "/lan/saves/news";
inline constexpr std::string_view saveSyncedRoute = "/lan/saves/synced";

/// `POST /lan/saves/status`, `/get`, `/news` and `/synced`: which game's saves, by its
/// `library::saves::SaveKey`.
struct SaveAsk {
    std::string system;
    std::string game;
    /// The profile whose saves they are (`profiles::Profile::id`).
    std::string profile;
    /// `/news`: the player picked the sender's saves over the receiver's.
    bool picked{false};
    /// `/synced`: the hash both now hold, and the counter the sender holds it as.
    std::string hash;
    std::uint64_t counter{0};
};
[[nodiscard]] std::string write(const SaveAsk& ask);
/// Nothing when `body` is not one, its system or game could step out of a folder, or its profile
/// is not a profile id.
[[nodiscard]] std::optional<SaveAsk> readSaveAsk(std::string_view body);

// The answer to `/lan/saves/status` is a `save_sync::SaveStatus`.

/// `POST /lan/play`: a paired guest asks to play one of the listed games on the host as its
/// active profile, with its half of the session's salt.
inline constexpr std::string_view playRoute = "/lan/play";
struct PlayRequest {
    std::string game;
    /// The guest's active profile's id; the host plays as it only while it is linked with the
    /// guest.
    std::string profile;
    secrets::crypto::Bytes salt;
};
[[nodiscard]] std::string write(const PlayRequest& request);
[[nodiscard]] std::optional<PlayRequest> readPlayRequest(std::string_view body);

/// The host's answer: its half of the salt, and the UDP port its stream listens on, or `waiting`
/// (no port yet) while its player is asked to let the guest join a game it already runs.
/// `joined` says the game was running there already, so it goes on when the guest leaves.
struct PlayAnswer {
    std::uint16_t port{0};
    secrets::crypto::Bytes salt;
    bool waiting{false};
    bool joined{false};
    /// The caller took over a seat in the game that runs there.
    bool continued{false};
};
[[nodiscard]] std::string write(const PlayAnswer& answer);
/// Nothing when `body` is not one, or one not waiting has no port.
[[nodiscard]] std::optional<PlayAnswer> readPlayAnswer(std::string_view body);

/// `POST /lan/play/status`: how the caller's waiting ask to join `game` stands there.
inline constexpr std::string_view playStatusRoute = "/lan/play/status";
/// A game named by its id on the openSU asked: the body of `/lan/play/status`.
struct GameRef {
    std::string game;
};
[[nodiscard]] std::string write(const GameRef& ref);
[[nodiscard]] std::optional<GameRef> readGameRef(std::string_view body);
/// `POST /lan/library/news`: what the caller lists changed, so it is read again.
inline constexpr std::string_view libraryNewsRoute = "/lan/library/news";
/// `POST /lan/invite`: the host's player invites this openSU to its game running now.
inline constexpr std::string_view inviteRoute = "/lan/invite";
struct Invitation {
    std::string game;
    std::string title;
};
[[nodiscard]] std::string write(const Invitation& invitation);
[[nodiscard]] std::optional<Invitation> readInvitation(std::string_view body);
/// `waiting`, `declined` (with why, when the player allowed it and the host could not) or
/// `accepted`, with the stream's port.
struct PlayVerdict {
    std::string verdict;
    std::uint16_t port{0};
    std::string why;
};
[[nodiscard]] std::string write(const PlayVerdict& verdict);
/// Nothing when `body` is not one, or an accepted one has no port.
[[nodiscard]] std::optional<PlayVerdict> readPlayVerdict(std::string_view body);

/// `POST /lan/netplay`: a paired openSU asks to play a ROM both have, linked, with its half of the
/// session's salt. Its answer is `{}`; the player here is asked.
inline constexpr std::string_view netplayRoute = "/lan/netplay";
/// `POST /lan/netplay/status` with a `NetplayRom`: how the ask stands.
inline constexpr std::string_view netplayStatusRoute = "/lan/netplay/status";
/// `POST /lan/netplay/save` with a `NetplayRom`: the save the asked side handed over, as bytes.
inline constexpr std::string_view netplaySaveRoute = "/lan/netplay/save";
/// What both machines of a session must have the same, or their games part: on each side, the link
/// core's revision, the options it would run with and the BIOS it would boot (its SHA-256, empty
/// for none).
struct NetplayFacts {
    std::string coreRevision;
    std::map<std::string, std::string> options;
    std::string bios;

    bool operator==(const NetplayFacts&) const = default;
};

/// The clock a session's cartridges run from, the asker's: Unix time in milliseconds at the first
/// frame and the seconds east of UTC that their faces show.
struct NetplayClock {
    std::int64_t epochMs{0};
    std::int32_t utcOffset{0};

    bool operator==(const NetplayClock&) const = default;
};

struct NetplayAsk {
    std::string console;
    /// The ROM's file name.
    std::string file;
    std::string title;
    std::uint64_t size{0};
    std::string sha256;
    secrets::crypto::Bytes salt;
    NetplayFacts facts;
    NetplayClock clock;

    bool operator==(const NetplayAsk&) const = default;
};
[[nodiscard]] std::string write(const NetplayAsk& ask);
/// Nothing when `body` is not one.
[[nodiscard]] std::optional<NetplayAsk> readNetplayAsk(std::string_view body);

struct NetplayRom {
    std::string sha256;
};
[[nodiscard]] std::string write(const NetplayRom& rom);
[[nodiscard]] std::optional<NetplayRom> readNetplayRom(std::string_view body);

/// "waiting", "declined" with why when it says, or "allowed" with the UDP port of the session and
/// the asked side's half of the salt.
struct NetplayVerdict {
    std::string verdict;
    std::string refusal;
    std::uint16_t port{0};
    secrets::crypto::Bytes salt;
    /// `NetplayClock::epochMs` as the asked side started with it: the ask's, plus what the player
    /// took to answer.
    std::int64_t clockMs{0};
};
[[nodiscard]] std::string write(const NetplayVerdict& verdict);
[[nodiscard]] std::optional<NetplayVerdict> readNetplayVerdict(std::string_view body);

/// The routes of ROM import, for paired peers.
inline constexpr std::string_view romListRoute = "/lan/roms";
inline constexpr std::string_view romFileRoute = "/lan/roms/file";
inline constexpr std::string_view romChunkRoute = "/lan/roms/chunk";
/// The most one `/lan/roms/chunk` answer carries.
inline constexpr std::uint64_t romChunkBytes = std::uint64_t{2} * 1024 * 1024;

/// Which folder a ROM route's path is in: a ROM folder the host lists, or its BIOS folder.
enum class RomKind : std::uint8_t { Rom, Bios };

/// One file a host offers: its path inside the ROM folder (`<console folder>/...`) or the BIOS
/// folder, `/`-separated, and its size.
struct RomFile {
    std::string path;
    std::uint64_t bytes{0};

    bool operator==(const RomFile&) const = default;
};

/// One game of a console: a file or a game folder of its ROM folder, and every file it holds.
struct RomGame {
    std::string title;
    /// Which of the host's ROM folders it is in, by its place in the host's list.
    std::uint32_t root{0};
    /// The file or folder that is the game, inside that ROM folder.
    std::string entry;
    std::vector<RomFile> files;

    [[nodiscard]] std::uint64_t bytes() const;
    bool operator==(const RomGame&) const = default;
};

/// One console's games on the host, under the folder name the host keeps them in, and the BIOS
/// files the host's runner has for it.
struct RomConsole {
    /// The console's key (`library::roms::RomSystem::key`).
    std::string system;
    std::string folder;
    std::vector<RomGame> games;
    std::vector<RomFile> bios;

    bool operator==(const RomConsole&) const = default;
};

/// `GET /lan/roms`: `{"consoles":[{"system","folder","games":[{"title","root","entry","files"}],
/// "bios"}]}`, `files` and `bios` each `[{"path","bytes"}]`.
[[nodiscard]] std::string write(const std::vector<RomConsole>& consoles);
/// A ROM listing read back; nothing when `body` is not one. A file whose path could step out of
/// its folder, or a game's file outside its entry, drops the game; a console without a key or
/// folder is left out.
[[nodiscard]] std::optional<std::vector<RomConsole>> readRomListing(std::string_view body);

/// `POST /lan/roms/file` and `/chunk`: one listed file, and for a chunk where it starts and how
/// many bytes are asked for (at most `romChunkBytes` come back).
struct RomAsk {
    RomKind kind{RomKind::Rom};
    std::uint32_t root{0};
    std::string path;
    std::uint64_t offset{0};
    std::uint64_t length{0};
};
[[nodiscard]] std::string write(const RomAsk& ask);
/// Nothing when `body` is not one or its path is empty or could step out of its folder.
[[nodiscard]] std::optional<RomAsk> readRomAsk(std::string_view body);

/// The answer to `/lan/roms/file`: the file's size and its SHA-256 as hex, empty while the host
/// is still hashing it.
struct RomFacts {
    std::uint64_t bytes{0};
    std::string sha256;

    bool operator==(const RomFacts&) const = default;
};
[[nodiscard]] std::string write(const RomFacts& facts);
[[nodiscard]] std::optional<RomFacts> readRomFacts(std::string_view body);

} // namespace opensu::lan::wire
