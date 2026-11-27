# "Use this PC's Steam login" does not sign fumarole in


Reported by the player (2026-11-27, Ally, e040b6d1): "Use this pc steam login not working here,
please fix".

Cause: the Ally's Steam remembers two accounts, neither marked `MostRecent`, and only one has a
saved token. fumarole's `steam_login.cpp:most_recent_account` refused any choice among several
accounts. Fixed in fumarole 5241e3e: it picks among the accounts with a token. Shown on the Ally
2026-11-27: openSU's "Use this PC's Steam login" signed in, Steam's tile came back, and
`fumarole list` returns the account's 406 games.

Status: fixed.
