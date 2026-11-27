# `/devices/open Tab/Row` leaves the focus on the tabs

Status: open.

Seen 2026-11-27 on the Ally (b949272a): with Devices closed, or open on another tab,
`POST /devices/open` with `Controllers/check:/dev/input/event17` answered
`{"devices":"controllers","row":"check:/dev/input/event17"}` but `/state` showed
`screenFocus: tabs/bluetooth`, so the next `a` started the calibration card instead of the check.
Opening `Controllers` first and then the row a second later focuses the row. Suspect
`DevicesController::openAt`: `navigator_.focus(row)` runs before the chosen tab's rows are built.
