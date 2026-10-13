# Prior art — iiSU (Android launcher, `iiSU-Alpha-7.4.apk`, release `0.0.7.4`, SHA-256 `0e9008b2b66c48f98edb7cfa42b3dbde2185ea439179ac673ed2e360769de6e8`)

iiSU is closed source; nobody else publishes a decompile. One APK patcher names classes and methods that exist in our jadx output.

| project | URL | kind | license | target build | build match (addresses checked) | what we may take | notes |
|---|---|---|---|---|---|---|---|
| iiSU | https://github.com/iisu-network/iiSU | upstream release repository (README, update notes, APK releases; no source) | none (closed source) | all releases | identity source of our APK (`gh release download 0.0.7.4`) | nothing | last push 2026-07-31 |
| Community-iiSU-PC | https://github.com/MAGOOSKEE/Community-iiSU-PC | APK patcher (apktool + smali) that redirects iiSU launches to PC emulators | PolyForm Noncommercial 1.0.0 | iiSU APK, version read at patch time | matched (2026-10-13) against our 0.0.7.4 jadx output: `MainActivity.v(MainActivity)` (`MainActivity.java:1466`), `defpackage.je6.a()` returning `MainActivity`, `defpackage.wy6.m(File, String)` returning `List` | evidence only (launch hand-off anchors) | active (2026-10-07); anchors on R8 names, which change per release |
| iiSU Interpreted | https://github.com/VictorUnlocked/iisu-interpreted-es-de | ES-DE theme imitating iiSU | none | screenshots of iiSU | not applicable | nothing | built from looks, not from the APK |
| iiSU Asset Tool | https://github.com/viik4/iisu-asset-tool | icon/cover generator for iiSU | MIT | iiSU asset conventions | not checked | evidence only (asset sizes and borders) | last push 2026-02-27 |

Not found (2026-10-13): an iiSU source release, decompile, design spec or layout documentation (GitHub, iisu.network, AlternativeTo, Patreon posts).
