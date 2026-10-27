# Giveaways: free-to-keep games from the stores

Status: open, planned.

The player: "add a section to show giveaways from Epic, GoG, Steam and maybe also if you have
Amazon Prime all though we don't have prime integration right now".

Decided:
- A Giveaways section: each free-to-keep offer with its art, store, and when it ends, the stores
  the player has turned on first; an offer already in the player's library shows "Owned".
- Sources, read-only and public:
  - Epic: the store's free games promotions listing (current and upcoming).
  - GOG: the current giveaway, and catalogue games discounted to free.
  - Steam: games discounted 100% for a limited time (free to keep), not free-to-play titles.
  - Prime Gaming: needs an Amazon sign-in and openSU has no Prime integration; listed only if its
    public offer listing works without one, else left for a Prime integration.
- Claiming: GOG through the signed-in GOG account where its API allows; Epic and Steam open the
  offer's store page (Epic in the browser, Steam in Steam's own browser like the SteamGridDB
  sign-in); nothing is bought or claimed without the player pressing Claim.
- Refreshed in the background a few times a day; a new offer can show a toast.
- Adult-rated offers hidden unless the player turns them on.
