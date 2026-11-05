# Net

`net::WebClient` (`src/net/web_client.*`) is openSU's one HTTP client, over libcurl: the stores'
APIs, artwork, emulator, core, shader and runtime downloads, plugins' `http.get`, WebDAV save
shares and the calls to other openSUs on the LAN (`lan::PeerClient`).

## Owns

- `get` (a 200 body, redirects followed), `request` (any status, each redirect asked of a
  `Refusal`, a body cap, an optional `Progress`, what it sends: `Sent`), `head` (`BodyFacts`) and
  `download` (a body streamed to a file, its `BodyFacts`).
- The limits: 10 s to connect, 30 s per transfer, or with `Progress` a stall limit of 1 KiB/s for
  30 s instead.
- What a request comes to, `net::Result<T>` (`src/net/result.hpp`): its value, a `Failure` with
  its reason, or `Stopped`.

## The stop rule

Every request takes a `std::stop_token` as its first argument; there is no default and no form
without one, so a caller cannot make a request that ignores its stop. The transfer runs on a curl
multi handle woken by a `std::stop_callback`, so a requested stop ends it at once, whatever it is
waiting for (connect, TLS, the answer); a request on a stop already requested is not made. A
`Progress` that returns false ends it the same way.

A stopped request is `Stopped`, never a `Failure`: whoever asked keeps nothing of it and logs
nothing. The layers above pass it on as their own stopped outcome (`Result::as`, a `Stopped` kind,
`library::RequestStopped` through store code) up to the worker, which ends its item without
recording it.

Each worker passes its own thread's token (`std::jthread`'s): the shell's owners are destroyed one
after another at quit, each joining its worker, so a request on a stop that is never requested
would hold the quit for up to 40 s per call. A caller with no worker of its own passes
`std::stop_token{}` explicitly, with the reason beside it; the one in `src/` is the GOG sign-in's
token exchange (`app/sign_in.cpp`), answered on the control channel's thread.

## Checking it

- `tests/net/web_client_test.cpp`: each kind of request held by a slow loopback server ends
  `Stopped` within 2 s of its stop, and one on a requested stop is not made.
- In the product: run `--hidden` with `https_proxy` pointing at a proxy that holds every CONNECT
  to the worker's host and refuses the rest, wait until the worker's request reaches it, `POST
  /quit` within curl's 10 s connect limit, and time the exit (under 0.1 s). A LAN peer is a
  listener that never answers, announced with `avahi-publish` under a peer in `lan.json`.
