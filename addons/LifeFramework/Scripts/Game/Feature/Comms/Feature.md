# Comms

Calls to services (epic #147). `LF_ServiceCalls` routes a call to a service listed in
`Configs/Comms/LF_ServiceCalls.conf` (key, name key, receiver, channels). Receiver is
`duty:police` or `job:<LF_EJobType number>`. `Place` rejects an unknown service with an Error,
logs "no answer" when nobody is on duty, and otherwise publishes `LF_OnServiceCall` on the event
bus. It never calls dispatch directly. Payphone entry point: #247.

## Payphone (#247)

`LF_Payphone.UseAt` is the server rule: cooldowns (player, phone) and "nobody on duty" refuse before
the coin moves, then the coin is taken once and `LF_ServiceCalls.Place` publishes. `LF_PayphoneAction`
is the in-world entry (one per preset on `Prefabs/Comms/LF_Payphone.et`). `LF_ServiceCallDispatch`
subscribes to `LF_OnServiceCall` and logs and radios police calls through `LF_Dispatch.Receive`.

## Dispatch board (#250)

`LF_ServiceBoard` subscribes to `LF_OnServiceCall` and pins every call whose channels include `board` to
its service's list; a call leaves after `m_iBoardExpirySeconds` (900). A world board is the wanted-board
prefab with `LF_WantedBoardReadAction.m_sService` set (`Prefabs/Comms/LF_DispatchBoard_Medic.et`, placed at
the DebugWorld hospital). Reading asks the server (`RpcAsk_LF_ServiceBoard`); it answers the calls only to a
reader the service reaches now (on-duty police, a current medic), a closed board to anyone else. Nothing is
replicated continuously, so a late joiner reads the current calls. `LF_Dispatch.Report` publishes each crime
report as a police service call (clothing, vehicle and on-duty count ride on the event), so crimes and
payphone calls take one path to the radio, the dispatch log, and the board.

## Taking a call (#251)

`LF_ServiceTakeAction` on a board calls `LF_ServiceTake.ServerTake`: on duty for the service, oldest open call
with room (`m_iMaxTakers`, police 2). It publishes `LF_OnServiceCallTaken`; the board marks the call taken and
`LF_ServiceCallTakenNotifier` notifies the caller (owner RPC by player id, no entity id).

## Pager (#249)

`LF_PagerSubscriber` (installed with the board) writes a call whose channels include `pager` to the
`LF_PagerComponent` of every receiver who carries one (`LF_Pager.Page`), then the pager beeps. The last
`m_iPagerMessages` calls ride one replicated string; `LF_ReadPagerAction` shows them. Police get one in
the duty kit (`GameMode_Roleplay_Debug.et`).

## Service radios (#252)

Duty kits carry a vanilla ANPRC68; `LF_ServiceRadio` tunes its first transceiver to the service's
`m_iFrequency` (kHz) on clock-in and on a reconnect while on duty.

## Police scanner (#253)

`Prefabs/Items/Fence/LF_Scanner.et` (`LF_ScannerComponent`, `LF_SerialComponent`) is sold by the fence
(`m_iMinCrimes` 2) and stamped with a serial like the other tools. `LF_ScannerSubscriber` (installed with the
board) offers every `LF_OnServiceCall` to the scanner carriers who are not its receivers; a service with a
radio frequency is heard (police, medic). The scanner chirps, the carrier gets a toast, and
`LF_OnScannerHeard` goes out on the bus. It never transmits. Legality is `m_bScannerIllegal` in
`LF_ServiceCalls.conf`: `LF_ConfiscateAction` (the officer's search of a wanted suspect) takes every scanner and
files `#LF-Offence_Scanner` once through `LF_WantedReports.Report` (no save change; the crime record counts it).

## Telephone calls (#147)

Design: `docs/architecture/phone-calls-spike.md`. Four pieces:

- `LF_PhoneExchange` (`LF_PhoneExchange.c`) is the call table with no world in it: calls are
  RINGING, CONNECTED, ENDED or MISSED, phones are keys (`LF_Payphone.PhoneKey`), players are server
  ids. `LF_Test_PhoneExchange` drives it.
- `LF_PhoneCalls` (`LF_PhoneCalls.c`) feeds it from the world on the server: dial, pick up, hang
  up, a one-second tick for the ring timeout, the maximum length and the cord
  (`LF_PhonePresence`: disconnected, down or dead, too far, phone gone). After every change it
  syncs the lines, publishes `LF_OnPhoneCall` and toasts the people involved. Dialling the station
  runs `LF_Payphone.UseAt` first, so coin, cooldowns, the duty check and `LF_OnServiceCall` (radio
  dispatch, board, pagers) are unchanged.
- `LF_PhoneComponent` marks a prop as a telephone; `m_sDispatchService` makes it a station
  telephone that rings for that service and is picked up only by its people on duty.
  `LF_PhoneLineComponent` lives on the replicated line entity the server spawns at a phone on
  first use (`Prefabs/Comms/LF_PhoneLine.et`): state and talker id as `RplProp`s. Clients ring a
  RINGING line from the phone prop on their own timer, so JIP needs nothing extra.
- `LF_PhoneDialAction`, `LF_PhoneAnswerAction`, `LF_PhoneHangUpAction` (`LF_PhoneActions.c`) read
  the line to show only what applies; the server decides again.

Call-back requests (the voicemail replacement): `LF_PhoneCallBackBook` is the pure book (offers
per caller and phone, requests, the pad entries); `LF_PhoneCallBacks` feeds it. A MISSED call
(rang out or nobody on duty) gives its caller an offer at that payphone, shown on the line
(`m_iOfferId`) to that player only. `LF_PhoneLeaveRequestAction` opens `LF_PhoneSlipSheet`;
`RpcAsk_LF_PhoneLeave` is checked for rate, offer, distance and length. `LF_PhoneReadPadAction`
opens `LF_PhonePadSheet` for the service's people on duty; `RpcAsk_LF_PhoneCallBack` rings the
payphone (`LF_PhoneCalls.CallBackAt`, a call with `m_bCallBack`). A picked-up ring-back closes the
request; one nobody answers reopens it. Requests expire with the dispatch board; nothing is saved.

Settings: `LF_PhoneConfig` (`Configs/Comms/LF_Phone.conf`). Never call
`RadioManagerEntity.GetTransceiversInRange`: it crashed the dedicated server in the spike.
