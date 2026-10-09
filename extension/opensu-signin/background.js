// Watches two sign-in endings and hands the code to opensu's control channel on loopback.
//   GOG:  https://embed.gog.com/on_login_success?origin=client&code=<code>
//   Epic: https://www.epicgames.com/id/api/redirect?clientId=...&responseType=code answers JSON
//         holding "authorizationCode" (Legendary's login, https://legendary.gl/epiclogin)
// The code is never logged or kept; the tab closes only once opensu accepted it.

// opensu's default control port (OPENSU_CONTROL_PORT); change it here if opensu runs on another.
const OPENSU_PORT = 7311;

const handed = new Set();

async function hand(store, code, tabId) {
  const key = `${tabId}:${store}:${code}`;
  if (!code || handed.has(key)) {
    return;
  }
  handed.add(key);
  try {
    const answer = await fetch(`http://127.0.0.1:${OPENSU_PORT}/signin/${store}`, {
      method: "POST",
      body: code,
    });
    if (answer.ok) {
      await browser.tabs.remove(tabId);
    } else {
      console.error(`opensu refused the ${store} sign-in: HTTP ${answer.status}`);
    }
  } catch (error) {
    console.error(`opensu is not reachable on port ${OPENSU_PORT}: ${error.name}`);
  }
}

function onGogPage(details) {
  if (details.frameId !== 0) {
    return;
  }
  hand("gog", new URL(details.url).searchParams.get("code"), details.tabId);
}

const gogPage = { url: [{ hostEquals: "embed.gog.com", pathEquals: "/on_login_success" }] };
browser.webNavigation.onBeforeNavigate.addListener(onGogPage, gogPage);
browser.webNavigation.onCommitted.addListener(onGogPage, gogPage);

// Reads the redirect page's response as it passes by, without changing it.
function onEpicRedirect(details) {
  const filter = browser.webRequest.filterResponseData(details.requestId);
  const decoder = new TextDecoder("utf-8");
  let text = "";
  filter.ondata = (event) => {
    text += decoder.decode(event.data, { stream: true });
    filter.write(event.data);
  };
  filter.onstop = () => {
    filter.close();
    try {
      hand("epic", JSON.parse(text).authorizationCode, details.tabId);
    } catch (error) {
      console.error("the Epic redirect page held no JSON");
    }
  };
}

browser.webRequest.onBeforeRequest.addListener(
  onEpicRedirect,
  { urls: ["https://www.epicgames.com/id/api/redirect*"], types: ["main_frame"] },
  ["blocking"]
);
