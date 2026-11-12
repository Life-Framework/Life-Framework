# Phone calls spike: payphone to dispatch phone, by voice

Spike of 2026-11-12 for the Comms epic (#147). Question: can a payphone call to the police be a
real call, where the dispatch phone rings, an officer picks up, the two talk by voice on a private
channel, and the caller leaves a voicemail when nobody answers?

Short answers:

- **Ringing, answering, hanging up, missed calls: yes.** Built and run on a dedicated server with
  two clients.
- **A private voice channel between two phone props: the wiring works, the sound is unverified.**
  Two world props can hold radio transceivers on a frequency no handheld can tune, with a key per
  call. The radio layer delivers between them at 268 m and refuses a wrong key. Each talker's
  machine transmits through its prop. No agent can speak or listen, and the test clients sent no
  voice at all, so one human test with two microphones decides whether the listener hears it.
- **Voicemail as a voice recording: no.** Script cannot read, store or replay voice. The
  replacement is a call-back request, built and run in the proof of concept.

The proof of concept is on the unmerged branch `spike/phone-calls-poc` (commit `eb93217b`). It is
not production code: raw English strings, no persistence, no tests, call state not replicated.

## How calls work today

`LF_PayphoneAction` on `Prefabs/Comms/LF_Payphone.et` calls `LF_Payphone.UseAt` on the server
(cooldowns, coin), which publishes `LF_OnServiceCall` through `LF_ServiceCalls.Place`.
`LF_ServiceCallDispatch` turns a police call into a radio toast for on-duty officers
(`LF_Dispatch.Receive`); the board, the pager and the scanner subscribe to the same event. Nobody
talks to anybody. `LF_ServiceRadio` already tunes a duty officer's handheld to the service
frequency, so the mod already depends on vanilla radio voice.

## What the engine offers

Sources are the vanilla scripts, read with `game_read`, and the API index (`api_search`).

| Fact | Source |
|---|---|
| `VoNComponent` has nine methods. The ones that matter: `SetCapture(bool)`, `SetCommMethod(ECommMethod)`, `SetTransmitRadio(BaseTransceiver)`, `GetTransmitRadio()`. | `scripts/Game/generated/Components/VoNComponent.c` |
| Its events carry no audio. `OnCapture(BaseTransceiver transmitter)` is polled while recording. `OnReceive(int playerId, bool isSenderEditor, BaseTransceiver receiver, int frequency, float quality)` fires on playback. `OnVoNUsed(int senderId)` fires on the server. | same file |
| `ECommMethod` is `DIRECT`, `SQUAD_RADIO`, `DIRECT_AI`, `GAME_MASTER_DIRECT`, `GAME_MASTER_RADIO`. There is no script-defined method. | `scripts/Game/generated/ECommMethod.c` |
| `VoNSystem` is an empty class. Routing is native. `RadioSpatialSystem` "refreshes RadioManagerEntity spatial cache from live radio world positions. VoN routing reads that cache on the server." | `scripts/Game/generated/System/VoNSystem.c`, `RadioSpatialSystem.c` |
| A radio is a `BaseRadioComponent` with a list of transceivers. Per radio: `SetPower`, `SetEncryptionKey(string)`. Per transceiver: `SetFrequency(int kHz)`, `SetRange(metres, 1 to 65535, server only)`, `GetMinFrequency`/`GetMaxFrequency`, `BeginTransmission(BaseRadioMessage)`. | `BaseRadioComponent.c`, `BaseTransceiver.c` |
| The band is prefab data. The ANPRC68 handheld has two transceivers, 32000 to 68000 kHz in 200 kHz steps, range 1300 m. | `Prefabs/Items/Equipment/Radios/Radio_ANPRC68.et` |
| The vanilla push-to-talk does `SetCommMethod(SQUAD_RADIO)`, `SetTransmitRadio(entry transceiver)`, `SetCapture(true)`, and `SetCapture(false)` on release. It refuses a radio whose key is not the faction key unless the server allows cross-faction transmit. | `scripts/Game/VON/SCR_VONController.c` lines 544 to 607, 629 to 648 |
| `SCR_VONController.AddEntry`, `RemoveEntry` and `SetEntryActive` are public, so a mod can add its own entry to the radio wheel. | same file, lines 200 to 277 |
| `ScriptedRadioMessage.OnDelivery(receiver, freq, quality)` lets script send a message over a transceiver and see who the radio layer delivers it to. | `scripts/Game/generated/Radio/ScriptedRadioMessage.c` |

## What was built

Branch `spike/phone-calls-poc`:

- `Prefabs/Comms/LF_PhoneLine.et`: an invisible replicated entity with a `BaseRadioComponent` and
  one `RadioTransceiver`, band 100000 to 165000 kHz in 1 kHz steps, range 60000 m. The server
  spawns one at each phone on first use.
- `Scripts/Game/Feature/Comms/LF_PhoneCalls.c`: `LF_PhoneComponent` (marks a prop as a phone;
  `m_sDispatchService` makes it a dispatch phone), the server call table `LF_PhoneCalls`, the
  client side `LF_PhoneVoice`, two owner RPCs on the player controller, and seven actions.
- `LF_Payphone.et` gains Call the police (voice), Pick up, Hang up, Leave a message.
  `LF_DispatchConsole.et` (the radio on the desk) gains Pick up, Hang up, Phone messages, Call back,
  and a debug switch for the voice mode.
- `harness phone` prints what one client's voice layer reads. `session.mjs phone-proof` drives the
  whole flow.

The call, as built:

1. **Dial.** Server checks the service has someone on duty (`LF_ServiceCalls.Check`). If so the
   call is `RINGING` and every dispatch phone for that service rings.
2. **Ring.** The server sends each player within 40 m of a ringing phone an owner RPC every 4 s;
   the client plays a sound at the phone prop. On-duty officers get a toast with the caller's
   place. A player who joins or walks up mid-ring hears the next pulse.
3. **Answer.** An on-duty officer uses Pick up. The call is `CONNECTED`. The server powers both
   lines, sets one frequency (100000 + call id) and one random key on both, and tells both
   players' machines which line to transmit through.
4. **Talk.** Each client sets `SQUAD_RADIO`, `SetTransmitRadio(its line)`, `SetCapture(true)`, and
   repeats that every 250 ms, because the vanilla voice keys reset it on release. It is an open
   microphone while the call lasts.
5. **Hang up.** Either talker uses Hang up, or walks more than 6 m from the phone, dies or
   disconnects. Both clients stop capturing; the lines get new idle keys.
6. **Missed.** After 20 s with no answer the call is `MISSED`. With nobody on duty it is missed at
   once. The caller may then leave a message at that phone.
7. **Message and call-back.** An officer reads the waiting messages at the dispatch phone and
   calls the oldest back. The payphone it came from rings; anyone there can pick it up; the same
   voice wiring connects the two.

A second mode, `CARRIED`, puts a vanilla handheld in each talker's inventory for the call and
tunes those instead of the props. It exists as the fallback if a prop transceiver turns out not to
be heard.

## Evidence

One lowspec session, dedicated server, two clients, 2026-11-12 22:02 to 22:05. Client 1 at
`Roadmap_Fuel_Payphone`, client 2 (police, on duty) at `Annex_Dispatch_Console`, 268 m apart.

Server log:

```
call 1 RINGING: player 1 dialled police ...; 2 dispatch phone(s) ring, 1 on duty
call 1 ring pulse at 2 phone(s); heard by player(s): 2 1
call 1 CONNECTED after 6 s: player 1 (caller) <-> player 2 (answered at ...), mode PROP
call 1 caller line: rplId -2147478452 powered=1 f=100001 kHz (asked 100001, band 100000..165000) key=LF_call_1_143227 range=60000 m
call 1 answerer line: rplId -2147478450 powered=1 f=100001 kHz (asked 100001, band 100000..165000) key=LF_call_1_143227 range=60000 m
call 1 lines are 268 m apart
call 1 probe 'right key' sent on 100001 kHz
call 1 probe 'right key' DELIVERED to rplId -2147478450 at <62.001,2.114,53.002> key=LF_call_1_143227 on 100001 kHz quality 0.99998
call 1 probe 'wrong key' sent on 100001 kHz
call 1 ENDED: player 1 hung up; talked 13 s; server saw 0 voice use(s)
call 1 probe 'after hang-up' sent on 100001 kHz
call 3 MISSED after 21 s: nobody picked up
message 4 left by player 1 (Kasper) from ...: 'Please call me back.'; 1 waiting
message listed: #4, 0 min ago, ..., rang 21 s: Please call me back.
call 5 RINGING: player 2 calls back message 4; the phone at ... rings
call 5 CONNECTED after 6 s: player 2 (caller) <-> player 1 (answered at ...), mode PROP
player 1 dialled police from ...: MISSED at once, nobody on duty; message offered
```

The wrong-key probe and the after-hang-up probe have no `DELIVERED` line: the radio layer gave
them to nobody.

Client log (both clients read the same):

```
ring 1 heard on this client at <252.15,0.914,229.875>, played at the phone prop=1
voice order: call 1, line rplId -2147478452, 100001 kHz
voice up: call 1, transmit radio is the line=1, line f=100001 kHz (ordered 100001) key=LF_call_1_143227 powered=1, comm=SQUAD_RADIO, capture accepted=1
call 1: first capture frame, transmitter is the line=1 (null transmitter=0)
voice down: call 1; captures=185 (on line 185), receives=0 (on line 0)
```

What that proves:

- A transceiver on a world prop works on a dedicated server on a frequency outside every handheld
  band. Frequency, key and power set by the server reach the clients.
- The radio layer delivers between the two props at 268 m (quality 0.99998 with a 60000 m range;
  0.958 for the 1300 m handhelds in `CARRIED` mode), only with the right key, and not after the
  hang-up.
- A client's `VoNComponent` accepts a prop transceiver it does not carry as its transmit radio, and
  the engine reports capture on that transceiver (185 capture events, all on the line).
- The state machine, the ring, the duty check, the message and the call-back run end to end.

What it does not prove:

- **That anyone hears anything.** The server's `OnVoNUsed` never fired and no client got
  `OnReceive`, in `PROP` mode and equally in `CARRIED` mode, which is the plain vanilla
  radio-in-inventory case. So the test clients sent no voice in either mode (most likely no
  microphone input on the hidden desktop; not confirmed), and the run says nothing about playback.
- **That a listener hears through a prop transceiver they do not carry.** Sending is native and
  position-based. Which player a receiving transceiver plays to is decided in native code that
  script cannot read. This is the one open engine question.
- **A late joiner.** Not run (a third client did not fit in RAM). The ring is re-sent every 4 s
  and the radio state is engine-replicated, so nothing in the design depends on join time, but it
  is unproven.

One crash: `RadioManagerEntity.GetTransceiversInRange(pos, 60000, list)` killed the dedicated
server with an access violation in the first run. The proof of concept no longer calls it.
Production code should not either.

## Answers to the spike questions

**Can two handsets share a private channel?** Yes at the radio layer. A line prefab picks its own
band; 100000 to 165000 kHz in 1 kHz steps gives 65000 channels that no vanilla radio can tune. The
key is a free string per radio, so a per-call key keeps out even a radio that reaches the
frequency. Limits: one key per radio (not per transceiver), range 1 to 65535 m, frequency is an
int in kHz.

**Entity-attached transmit?** Yes: `SetTransmitRadio` takes any `BaseTransceiver`, and the engine
captured on it. The player's voice still leaves from their own character; the prop is the radio.

**Routing voice through script?** No. Script chooses the comm method and the transceiver and
starts or stops capture. It never touches audio, and it cannot name the listeners.

**Who can hear?** By design, the two talkers. Unverified by ear. Whether people standing next to
a talker hear them as direct speech while they transmit is vanilla behaviour and is in the manual
test. Nobody near the far phone hears the earpiece; there is no API to play one player's voice at
another place.

**Dedicated server and range?** The radio layer: yes, at 268 m with range to spare (60000 m covers
Everon).

**Radio voice effect?** A call is expected to sound like a vanilla radio (unheard so far). No method on `VoNComponent`,
`BaseRadioComponent` or `BaseTransceiver` turns the effect off.

**Interplay with the vanilla voice keys.** The proof of concept holds the microphone open by
re-asserting every 250 ms. Pressing the direct-speech key during a call drops the talker to direct
speech until the next tick. Production should add a phone entry with `SCR_VONController.AddEntry`
and make it active, so the normal radio key and HUD work. `SCR_VONEntryRadio` reads its gadget
component for UI, and `ActivateVON` rejects a non-faction key, so the entry needs its own subclass.

## Voicemail verdict

**Recording a caller's voice is not possible from script, on the server or on a client.**

- `VoNComponent` exposes no audio buffer. `OnCapture` and `OnReceive` pass a transceiver, a player
  id, a frequency and a quality number.
- `VoNSystem` is an empty class; `VonRadioMessage` is an empty class.
- `AudioSystem` has 36 static methods (`scripts/Core/generated/Audio/AudioSystem.c`): play, stop,
  volumes, variables, music. None captures or records.
- `api_search` for methods named Record, Capture or Microphone finds only unrelated ones (base
  capture, screenshots, key binding).
- Even with the bytes, the only playback entry points take a resource (`PlaySound(resourceName)`,
  `PlayEvent(acp, event)`), not runtime data.

Alternatives, best first:

1. **Call-back request (recommended, built).** A missed call offers "leave a message". The server
   stores caller, place, phone, time and how long it rang. Officers list the messages at the
   dispatch phone and call one back; that payphone rings and whoever is there picks up. It costs
   nothing new in the engine and keeps the exchange in voice.
2. **Typed message with the request (recommended addition).** An edit box at the payphone, 200
   characters, rate-limited like other Comms requests. The text shows in the list. It is the only
   way to carry content when the caller cannot wait by the phone.
3. **Client-side recording.** No API exists. Not possible.
4. **External voice bridge.** With no tap on voice in the server, a bridge means a second voice
   system: every player runs a separate client, the server pairs identities, and something
   outside the game records and serves audio. It also cannot play audio back inside the game. The
   cost is a product of its own, plus consent and storage duties. Not recommended.

Storage, size, privacy: a message is a few strings, kept in server memory. Nothing is recorded, so
there is no audio to store and no recording consent to collect. Typed messages should be logged
like chat and fall under the same moderation rules. If a later engine version allows recording, it
needs an on-screen notice before the beep, a retention limit, and a place in the server rules.

## Recommended design

1. **Lines.** A phone prop carries `LF_PhoneComponent`. Its line is the replicated radio entity
   (spawned by the server as in the spike, or part of the prefab once the phone props are
   replicated). Line state (`IDLE`, `RINGING`, `BUSY`) is a replicated int on the line, so the
   actions show only when they apply and the ring is a client-side loop driven by state. That
   replaces the 4 s RPC pulse and gives a late joiner the ring for free.
2. **Calls.** `LF_PhoneCalls` on the server owns the table: `RINGING`, `CONNECTED`, `ENDED`,
   `MISSED`. It publishes bus events from `Feature/Comms/LF_CommsEvents.c` so the board, the pager
   and the scanner can react (a missed call is a board entry). Dialling goes through
   `LF_Payphone.UseAt` for the coin and the cooldowns. The current preset "report" actions stay
   for players without a microphone.
3. **Voice.** Per call: one frequency in the line band, one random key, both lines powered only
   for the call. The talker's client gets a phone entry in the vanilla voice controller. The call
   ends at hang-up, at 6 m from the phone, on death, disconnect or a configured maximum length.
4. **Fallback.** If the human test shows prop transceivers are not heard, keep everything and move
   the transceiver into a hidden, undroppable handset item in the talker's inventory for the call
   (`CARRIED` mode proves the server side). The spike used a plain ANPRC68 for this; a production
   handset item would use the private band.
5. **Messages.** Call-back requests with optional typed text, listed at the dispatch phone and on
   the dispatch board, expiring with the board (`m_iBoardExpirySeconds`). No persistence: a
   restart clears them, as it clears the board.
6. **Core or pack.** Phone logic, the line prefab and the actions are core. Which props are
   phones, the ring sound and the numbers are pack data.

## Costs and risks

- **The audio path is unproven.** Everything rests on one manual test. Do it before any issue
  below is started.
- **Open microphone.** A talker transmits everything until they hang up. The HUD must show it, and
  the 6 m cord and a maximum length bound it.
- **Vanilla voice controller.** A game update can change `SCR_VONController`; the phone entry is a
  modded surface to re-check on every update.
- **Radio sound.** Calls are expected to sound like radio, and script cannot change that.
- **Extra transceivers.** One powered radio per phone in use. Lines are powered only during a
  call. A few dozen phones are fewer radios than a full server's handhelds.
- **`GetTransceiversInRange` crash.** Do not call it.
- **Abuse.** Ringing the station on a loop. The payphone cooldowns and coin already apply once
  dialling goes through `LF_Payphone.UseAt`.
- **Ring sound.** The spike plays a radio beep. A real bell needs an audio asset.

## Manual testing

Needs two players with microphones on a dedicated server built from `spike/phone-calls-poc`,
DebugWorld, one of them on the police roster.

1. Officer: clock in at `Roadmap_Police_Desk`, walk to the dispatch console in the annex (the desk
   with the radio).
2. Caller: at the fuel station payphone use **Call the police (voice)**. Expect: the caller sees
   "Ringing..."; the officer hears a beep from the desk radio every 4 s and sees a toast.
3. Officer: **Pick up the dispatch phone**. Both see "Connected".
4. Both speak without pressing any key. Record for each side: is the other heard, and does it
   sound like radio? **This is the open question.**
5. A third player stands beside the caller, then beside the officer: do they hear the nearby
   talker, and do they hear the far one? (Expected: the far one is not heard. Whether the near
   one is heard is what this step finds out.)
6. A third player with a handheld scans its whole band during the call: expected silence.
7. Caller: **Hang up**. Speak again: nothing is heard at the far end.
8. If step 4 failed: officer uses **Spike: switch voice mode** (toast "Voice mode: CARRIED") and
   repeat steps 2 to 7. Each talker now holds a spare handheld for the call.
9. Missed call: caller dials, officer waits 20 s. Caller uses **Leave a message for the police**.
   Officer uses **Phone messages**, then **Call back the oldest message**; the payphone beeps;
   caller uses **Pick up**; both speak.
10. Late joiner: a player joins while a phone rings and stands near it: the beep is heard within
    4 s.

The server log lines to read are under `[LFDebug:Phone]`; `server saw N voice use(s)` at the end
of a call must be above 0 once people speak.

## Proposed issues

In order. The first gates the rest.

1. **Human voice test of the spike branch** (steps above). Outcome picks prop lines or the carried
   handset.
2. **Phone lines and call state**: `LF_PhoneComponent`, the line prefab, replicated line state,
   `LF_PhoneCalls` with `LOGIC` tests for the state machine (dial, answer, hang up, timeout, busy,
   off duty), bus events, localization keys.
3. **Ring**: client loop from line state, a bell asset, the on-duty notification, a late-joiner
   check in a three-client session.
4. **Voice channel**: per-call frequency and key, the phone entry in the voice controller, the
   in-call HUD mark, the cord and the maximum length. Dedicated-server proof plus manual steps.
5. **Dispatch phone prop**: a real phone prefab for the station, placed with `tools\cli place`,
   replacing the desk radio stand-in.
6. **Missed calls and call-back**: the message list at the dispatch phone and on the dispatch
   board, typed text with rate limit, expiry, call-back ringing the payphone.
7. **Payphone integration**: coin and cooldowns through `LF_Payphone.UseAt`, the preset report
   actions kept, `docs/features.md` and `Feature/Comms/Feature.md` updated.
8. **Medic and taxi lines**: the same dispatch phone for other services, config only.
