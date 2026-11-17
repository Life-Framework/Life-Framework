# Service calls (#248)

- ✅ `LF_ServiceCalls` routes a call to any service in `Configs/Comms/LF_ServiceCalls.conf` (police, medic, taxi; receiver is on-duty police or a current job). An unknown service is rejected with an error, nobody on duty logs "no answer", otherwise `LF_OnServiceCall` is published on the event bus. No caller yet; the payphone is #247.

