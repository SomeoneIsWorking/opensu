# Agents in this repo

- Drive a running openSU through the control channel's probes (`/game/launch`, `/game/close`,
  `/details/open`, `/search`, `/settings/open`, `GET`/`POST /settings`; the list is in
  [docs/guide/environment.md](docs/guide/environment.md#control-channel)), not by walking its
  screens with `/input`. A probe that is missing gets added to `src/app/shell_probes.cpp`.
  `/input`, `/key`, `/text` and `/pointer` are for testing input itself.
- [docs/codemap.md](docs/codemap.md) says where everything lives.
