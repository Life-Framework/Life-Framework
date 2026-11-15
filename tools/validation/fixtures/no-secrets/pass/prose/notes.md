# Fixture: prose that names the webhook shape without a real one

Webhooks look like https://discord.com/api/webhooks/<id>/<token>; keep the real one in the host env.
The code builds `https://discord.com/api/webhooks/${w.id}/${w.token}` at runtime.
