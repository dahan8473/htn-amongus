# Among Us IRL

🏆 **Winner — Solana: Best Use of Solana and Badge Hack ($2,500), [Hack the North 2026](https://hackthenorth.com)**

A physical, badge-based version of Among Us. Each player wears an ESP32-C3
badge with buttons, an LED strip, and a small screen. There's no laptop, no
router, and no game server: badges talk to each other directly over ESP-NOW,
and whichever badge starts a round becomes the host for that round, running
the game's state machine and broadcasting results to everyone else.

**[Devpost](https://devpost.com/software/among-us-lbrwjk)** ·
**Demos:** [full playthrough](https://www.youtube.com/shorts/oBDq92WUub4) ·
[proximity killing](https://youtube.com/shorts/8PUTfQ83m04) ·
[emergency meeting range test](https://youtube.com/shorts/_d-grZlzlpw)

Built in 36 hours by [Lily Song](https://github.com/s-illly), [Yanzi Guo](https://github.com/yanziguoo), and [David Liu](https://github.com/dahan8473).

## Hardware

| Component | Interface | Pins |
|---|---|---|
| Buttons (A, B, HOME, D-pad, AUX1) | 74HC165 shift register | DATA=7, LOAD=20, CLK=21 |
| START button | Dedicated GPIO, active-low, internal pull-up | GPIO 9 |
| LED strip (6x WS2812/NeoPixel) | Single-wire | GPIO 3 |
| Display (ST7789 TFT) | SPI | MOSI=10, SCLK=1, CS=2, DC=0, RST=4 |
| IMU (SC7A20 accelerometer) | I2C | SDA=5, SCL=6, addr 0x19 |
| NFC reader (MFRC522) | I2C | addr 0x26 (drives the task minigames — see the `tasks` branch upstream) |

Board: `esp32-c3-devkitm-1`, Arduino framework, built with PlatformIO.

## Physical controls

There are **8 momentary buttons behind the shift register** (A, B, HOME,
UP/DOWN/LEFT/RIGHT, and AUX1), **one dedicated momentary button** (START, its
own GPIO since it's also the boot-mode strapping pin), and **one maintained
toggle switch** (also read as AUX1's bit, but physically a switch that stays
where you leave it rather than springing back).

That distinction matters for how the firmware reads each one:
- The 8 shift-register buttons and START are all edge-detected in software
  (`isButtonPressed()` fires once on press, `isButtonHeld()` is true for as
  long as it's down) -- debounced over a ~10ms poll in `buttons.cpp`.
- AUX1 is read as a live level, not an edge -- the firmware just mirrors
  whatever position the switch is currently in.

### AUX1 -- Immortal / Mortal switch (demo mode)

This is the one physical **toggle**, and it's separate from the rest of
normal gameplay -- it exists for demos and testing, not for players.

- **Switch ON** ("immortal"): this badge cannot be killed, and impostors'
  proximity scan won't even list it as a valid target -- it's skipped
  exactly like an already-dead player or a fellow impostor would be.
- **Switch OFF** ("mortal"): normal behavior, killable like anyone else.

Flipping it immediately broadcasts your new status to every other badge (an
`IMM:<id>:<0|1>` message over the mesh), so the host -- which validates every
kill attempt -- always has an up-to-date answer regardless of which badge is
hosting. The state also **persists across rounds**, since it reflects a
physical switch position rather than per-round game state; flip it back to
mortal manually when you're done demoing.

### Buttons, by game phase

| Phase | Button | Action |
|---|---|---|
| **Lobby** | UP / DOWN | Move the settings cursor (impostor count, discuss/vote timers, meeting limit) |
| | LEFT / RIGHT | Change the selected setting's value (synced live to every badge) |
| | START | Start the game. **Whoever presses this becomes the host for the round, and is guaranteed to be an impostor.** |
| **Playing** | HOME (tap) | Call an emergency meeting (only while alive) |
| | HOME (hold) | Show a debug tilt-sensor screen for as long as it's held |
| | START (hold) | Reveal your role card: your role, your fellow impostors (if any), and -- for impostors -- a live kill-cooldown countdown |
| | B (hold) | Context-sensitive: if you're an impostor, off cooldown, and a killable crewmate is in proximity range, this kills them. Otherwise, if a dead player's badge is in proximity range, this reports the body and calls a meeting. If neither applies, holding B does nothing. |
| **Gather** (meeting called) | A | Move to discussion |
| | B | Cancel, return to playing |
| **Discuss** | B | End discussion early, move to voting |
| **Voting** | LEFT / RIGHT | Cycle your vote target (players, or SKIP) |
| | A | Cast your vote |
| **Game over** | START | Return to the lobby (host only) |

Kills and body reports aren't manually aimed -- there's no target-cycling
button. The nearest qualifying badge in ESP-NOW proximity range (an RSSI
threshold, roughly "a couple meters," tuned via `KILL_RSSI`/`REPORT_RSSI` in
`game.cpp`) is picked automatically when you hold B.

## Networking

No WiFi router, no MQTT broker, no laptop -- badges connect directly over
ESP-NOW on a fixed channel (`ESPNOW_CHANNEL` in `espnow_radio.h`), which
every badge must share since there's no access point for it to be inherited
from. Two independent message types ride the same radio, tagged so a shared
receive callback can tell them apart:

- **Proximity pings** -- each badge broadcasts its short ID every 200ms;
  RSSI on each received ping (read straight off the ESP-NOW receive
  callback) drives the "who's nearby" checks used for kills and body
  reports.
- **Game-state mesh** -- a small TTL-flood protocol: every game message
  (role assignment, phase changes, alive list, votes, results, immortal
  status) is relayed by whichever badges hear it, up to a bounded number of
  hops, so the whole group stays in sync without needing every badge to be
  in direct radio range of every other one. A dedup cache stops this from
  turning into a broadcast storm.

Whichever badge presses START in the lobby becomes the **host** for that
round: it owns the phase timer, deals roles, validates every kill/report/vote
request, and broadcasts the results. Every other badge just follows the
host's messages.

## Building & flashing

```
pio run                          # build
pio run -t upload                # flash (add --upload-port /dev/cu.XXXX if it can't autodetect)
pio device monitor                # serial log (115200 baud)
```

Every badge should be flashed from the same firmware build, since the
ESP-NOW channel is hardcoded and shared -- there's no per-device
configuration needed.

## Project layout

| File | Responsibility |
|---|---|
| `main.cpp` | Boot sequence and the main loop wiring everything together |
| `espnow_radio.*` | Shared ESP-NOW radio bring-up (fixed channel, no AP), demuxes the one physical receive callback by packet type |
| `broadcast.*` | TTL-flood mesh transport for game-state messages |
| `espnow_prox.*` | Proximity/RSSI ranging used for kills and body reports |
| `players.*` | Player identity, roster, and per-player state (alive, role, immortal) |
| `game.*` | The host-authoritative game state machine: phases, roles, votes, kills |
| `buttons.*` | Shift-register + START button reading and debouncing |
| `leds.*` | The 6-LED status strip |
| `display.*` | All TFT screens (lobby, HUD, role card, voting, results, etc.) |
| `imu.*` | Tilt sensor, used for the debug screen and IMU minigames |
| `nfc.*` | NFC reader driver for tag-scanning tasks |

## Task minigames

Crewmates scan NFC tags placed around the venue to open sensor-driven task
minigames: garbage disposal and window-wiping use the IMU's tilt (pitch/roll),
wires and a rhythm game use the button layout. The task work lives on the
[`tasks` branch upstream](https://github.com/s-illly/htn-amongus/tree/tasks).

## Roadmap

- Merge the task minigames into main
- Sabotage mechanics for impostors
- Cooperative tasks that use RSSI to make players physically link up
