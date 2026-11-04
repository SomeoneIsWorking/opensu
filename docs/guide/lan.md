# LAN

With LAN on, openSU finds the other openSUs on your network, pairs with the ones you accept, and
lists their installed games in your Library under their names. Playing them from here
(remote play) is not built yet. Starting one says so.

LAN is an integration, off until you turn it on in Settings › Integrations
([stores.md](stores.md#integrations)). Finding other openSUs works on Linux with Avahi running
(`avahi-daemon`, on by default on most desktops). On Windows, macOS and Android, Settings › LAN
says it is not built yet.

## Pairing

1. Turn LAN on on both openSUs.
2. On one of them, open Settings › LAN. The other one is listed under *Found here*; press Pair.
3. Both screens show the same six-digit code. On the other openSU, a question asks whether to
   pair. Accept only if it shows the code you see here; Decline is the default.

Once paired, each openSU lists the games installed on the other one. In Library they are a
launcher named after that openSU, with its covers. They do not appear on Home. They are listed
while that openSU is on and on the network, and are checked again every minute.

## Settings › LAN

- **Name**: how other openSUs list this one. It is the computer's name until you change it.
- **Found on the LAN**: whether others can find this openSU, and the port it listens on (7312
  unless `OPENSU_LAN_PORT` says otherwise; [environment.md](environment.md)). Allow that port in
  your firewall for other machines to reach it.
- **Found here**: other openSUs on the network you have not paired with.
- **Paired**: the ones you have. Remove forgets the pairing here and tells the other openSU to
  forget it too. Its games leave Library on both sides. Pairing again shows a new code.

## What is shared, and with whom

- An openSU you have not paired with learns only this one's name. Everything else is refused.
- A paired openSU sees the titles and covers of your installed games, nothing more.
- Pairings are kept in your desktop's keyring when there is one, otherwise in an owner-only file
  in openSU's config folder.
- What is sent is not encrypted: someone else on the same network can read the game list as it
  passes. Each request is signed with the pairing's secret, so no one can ask in a paired
  openSU's name or forge its answers. Use LAN only on networks you trust.
- The two computers' clocks must agree to within two minutes.
