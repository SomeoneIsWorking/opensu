# Giveaways: free-to-keep games from the stores

Status: partial. Built; Claim not run against real accounts (see Unproven).

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
- Every offer shows: no content filter (keeping explicit games out applies to the README
  screenshots only).

## Placement

A Library tile, Giveaways, after the store launchers and before All games, shown when any store
launcher is; A opens a `gpu2d::ui` screen. It sits with the stores whose offers it lists, and a
dock section would need a third section with its own grid presentation for a handful of cards.

## Sources

All are public, need no key or sign-in, and are read by `src/giveaways/offer_feed.cpp`.

- Epic: `GET https://store-site-backend-static-ipv4.ak.epicgames.com/freeGamesPromotions?locale=en-US&country=US&allowCountries=US`.
  `data.Catalog.searchStore.elements[]`: `title`, `id`, `offerType` (`BASE_GAME`, `DLC`,
  `ADD_ON`), `catalogNs.mappings[]` and `offerMappings[]` (`pageSlug` of `pageType`
  `productHome`), `productSlug`, `keyImages[]` (`OfferImageWide`, `OfferImageTall`, `Thumbnail`),
  `promotions.promotionalOffers[].promotionalOffers[]` and `upcomingPromotionalOffers` with
  `startDate`, `endDate`, `discountSetting.discountPercentage` (the percent left to pay: 0 is
  free). Store page `https://store.epicgames.com/en-US/p/<pageSlug>`.
- GOG catalogue: `GET https://catalog.gog.com/v1/catalog?limit=48&price=between:0,0&discounted=eq:true&productType=in:game,pack,dlc&order=desc:trending&page=1&countryCode=US&locale=en-US&currencyCode=USD`.
  `products[]`: `id`, `slug`, `title`, `productType`, `coverHorizontal`, `storeLink`,
  `price.finalMoney.amount`, `price.baseMoney.amount`.
  Free means final 0 and base above 0. No discount end date. `query=like:<title>` searches it.
- GOG giveaway: the homepage's sections, `GET https://sections.gog.com/v1/pages/2f?countryCode=US&currencyCode=USD&locale=en-US`
  (`2f` is the path `/` in hex), lists `sections[]` (`sectionType`, `sectionId`); the giveaway is
  `GIVEAWAY_SECTION`, read with `GET https://sections.gog.com/v1/pages/2f/sections/<id>`:
  `properties.endDate`, `properties.product.title`, `slug`, `coverHorizontal`. The product's id
  comes from the catalogue search by title. Claiming on GOG's site is
  `POST https://www.gog.com/giveaway/claim` with the web store's own bearer token, which is not
  the Galaxy token openSU holds, so Claim opens `https://www.gog.com/en/#giveaway`.
- Steam: `GET https://store.steampowered.com/search/results/?query=&start=0&count=100&maxprice=free&specials=1&infinite=1&cc=us&l=english&ignore_preferences=1`
  with `Cookie: wants_mature_content=1; birthtime=0` (without it adult-only apps are left out),
  JSON `results_html`; rows with `data-discount="100"` give `data-ds-appid`. Then
  `GET https://api.steampowered.com/IStoreBrowseService/GetItems/v1/?input_json=` with
  `{"ids":[{"appid":N}],"context":{"language":"english","country_code":"US"},"data_request":{"include_assets":true}}`:
  `store_items[]`: `appid`, `name`, `type` (0 game, 4 DLC), `store_url_path`,
  `assets.asset_url_format` and `assets.header` (on `https://shared.akamai.steamstatic.com/store_item_assets/`),
  `best_purchase_option.is_free_to_keep` and `free_to_keep_ends`. A free-to-keep game or DLC is
  listed; free-to-play titles have no such option.
- Prime Gaming: left out. `gaming.amazon.com` now sends to `luna.amazon.com/claims/home`, which
  draws its offers from a private GraphQL that refuses requests without the page's session (403,
  then 418 with a CSRF token). It needs a Prime integration.

## How it runs

`giveaways::GiveawayBoard` reads every store at start when the cache is older than 6 hours, every
6 hours after, 30 minutes after a failure, and on X; a store that fails keeps its last offers. The
offers are cached in `<cache>/giveaways/offers.json` and their art in `<cache>/giveaways/art/`.
`giveaways::shownOffers` drops ended offers; turned-on stores come first. Owned is the same store's
game in the library by id or title. After the first read a later read that makes an offer free shows
one toast. Claim: Steam opens the store page in Big Picture when openSU's own Steam is Ready, else
the browser (`xdg-open`); Epic and GOG open the page in the browser.

Check it: in a hidden run open Library › Giveaways; each read logs `[giveaways] <Store>: N free
to keep` or the store's failure.

## Unproven

- Claim was never pressed: the browser path and the Big Picture store page are unrun.
- No GOG giveaway or discounted-to-free GOG game was live on 2026-10-27; the giveaway section and
  discount parsers ran only on fixtures, and a live giveaway's section shape is from GOG's page
  bundle.
- The new-offer toast was not seen live.
