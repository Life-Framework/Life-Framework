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
