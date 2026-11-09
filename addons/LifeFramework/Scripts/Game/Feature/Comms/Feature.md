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
subscribes to `LF_OnServiceCall` and relays police calls through `LF_Dispatch.RelayCall`.
