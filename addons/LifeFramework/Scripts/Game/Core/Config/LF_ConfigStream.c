//------------------------------------------------------------------------------------------------
//! What a client did with one config stream from the server (#285).
enum LF_EConfigStreamStatus
{
	APPLIED,		//!< New values; the features re-read them.
	UNCHANGED,		//!< Same hash as the values already applied; nothing to do.
	BAD_FORMAT,		//!< Another stream format: server and client run different builds.
	BAD_HASH,		//!< The payload does not match its hash.
	BAD_JSON,		//!< The payload does not parse.
	BAD_SCHEMA		//!< Another config schema: server and client run different builds.
};

//------------------------------------------------------------------------------------------------
//! The server-to-client config stream (#285). The owner's config.json is read on the server only,
//! so a client builds its shop catalog from the shipped `.conf` files. The server serialises the
//! effective, merged values of the client-relevant sections (core, then pack, then owner) and sends
//! them to every client: on join through the game mode's RplSave/RplLoad (JIP), and again through
//! a broadcast RPC when they change (`/configreload`). The client's feature resolvers then read the
//! server's values instead of the shipped ones (LF_ShopPricesConfig.ApplyLayers, last layer).
//!
//! Payload: one JSON object, `{"stream":1,"schemaVersion":1,"owner":true,"shops":{...}}`. A
//! section is written by its feature (LF_ShopPricesConfig.WriteStream) and read back through an
//! LF_ConfigReader (LF_ShopPricesConfig.ApplyStream), so a malformed value can never crash the
//! client. Only sections a client reads are written; the owner file itself never leaves the server.
//! The wire carries the format version and the payload's hash beside the text; a client that gets
//! another format, another schema or a payload that does not match its hash logs LF_Debug.Error and
//! keeps the values it has.
//!
//! A new client-relevant section: write it in BuildPayload, read it with OpenReceivedSection in the
//! feature's apply step, and re-apply it in ApplyToFeatures.
//------------------------------------------------------------------------------------------------
class LF_ConfigStream
{
	//! The wire format. Bump it when the payload shape changes in a way an older reader misreads.
	static const int FORMAT_VERSION = 1;

	//! The shipped shops section is about 3.8 KB. The engine documents no limit for one replicated
	//! string, so a payload far above what was proven on a dedicated server logs a warning.
	static const int WARN_BYTES = 32768;	// lf-fixed: a log threshold, not tuning

	static const string KEY_FORMAT = "stream";
	static const string KEY_SCHEMA = "schemaVersion";
	static const string KEY_OWNER = "owner";

	//! Server: the payload clients get, and its hash.
	protected static string s_sPublished;
	protected static int s_iPublishedHash;
	protected static bool s_bPublished;

	//! Client: the last payload received and accepted.
	protected static string s_sReceived;
	protected static int s_iReceivedHash;
	protected static bool s_bReceived;

	//------------------------------------------------------------------------------------------------
	//! Serialises the client-relevant sections. Pure: tests call it with a config built in script.
	//! \param[in] shops The effective shop prices, or null to leave the section out.
	//! \param[in] ownerLayer Whether an owner config.json is behind these values (logged by the client).
	//! \param[in] vehicles The effective vehicle prices, or null to leave the section out.
	//! \param[in] police The effective police tunables and ranks, or null to leave the section out.
	//! \param[in] organizations The effective organization tunables, or null to leave the section out.
	//! \return The payload text.
	static string BuildPayload(LF_ShopPricesConfig shops, bool ownerLayer, LF_VehiclePricesConfig vehicles = null, LF_PoliceConfig police = null, LF_OrgConfig organizations = null)
	{
		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext(false);
		ctx.WriteValue(KEY_FORMAT, FORMAT_VERSION);
		ctx.WriteValue(KEY_SCHEMA, LF_ConfigService.SCHEMA_VERSION);
		ctx.WriteValue(KEY_OWNER, ownerLayer);
		if (shops)
		{
			ctx.StartObject(LF_ShopPricesConfig.SECTION);
			shops.WriteStream(ctx);
			ctx.EndObject();
		}
		if (vehicles)
		{
			ctx.StartObject(LF_VehiclePricesConfig.SECTION);
			vehicles.WriteStream(ctx);
			ctx.EndObject();
		}
		if (police)
		{
			ctx.StartObject(LF_PoliceConfig.SECTION);
			police.WriteStream(ctx);
			ctx.EndObject();
		}
		if (organizations)
		{
			ctx.StartObject(LF_OrgConfig.SECTION);
			organizations.WriteStream(ctx);
			ctx.EndObject();
		}
		return ctx.ExportToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rebuilds the payload from the effective values the server prices with.
	//! \return True when the payload differs from the one published before (or none was).
	static bool Publish()
	{
		LF_ShopPricesConfig shops;
		LF_ShopManagerComponent shopManager = LF_ShopManagerComponent.GetInstance();
		if (shopManager)
			shops = shopManager.GetConfig();
		if (!shops)
			LF_Debug.Warn(LF_ConfigService.FEATURE, "stream: no shop config on the server; clients keep their shipped shop prices");

		LF_VehiclePricesConfig vehicles = LF_VehiclePricesConfig.Get();
		if (!vehicles)
			LF_Debug.Warn(LF_ConfigService.FEATURE, "stream: no vehicle price config on the server; clients keep their shipped vehicle prices");

		LF_PoliceConfig police = LF_PoliceConfig.Get();
		if (!police)
			LF_Debug.Warn(LF_ConfigService.FEATURE, "stream: no police config on the server; clients keep their shipped police values");

		LF_OrgConfig organizations = LF_OrgConfig.Get();
		if (!organizations)
			LF_Debug.Warn(LF_ConfigService.FEATURE, "stream: no organization config on the server; clients keep their shipped organization values");

		string payload = BuildPayload(shops, LF_ConfigService.HasOwnerLayer(), vehicles, police, organizations);
		int hash = payload.Hash();
		bool changed = !s_bPublished || hash != s_iPublishedHash;
		s_sPublished = payload;
		s_iPublishedHash = hash;
		s_bPublished = true;

		if (changed)
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("stream: published %1 bytes, hash %2, owner layer %3", payload.Length(), hash, LF_ConfigService.HasOwnerLayer()));
		if (changed && payload.Length() > WARN_BYTES)
			LF_Debug.Warn(LF_ConfigService.FEATURE, string.Format("stream: %1 bytes is above the %2 this stream was proven with; if clients show shipped prices, trim the owner's priceRules", payload.Length(), WARN_BYTES));
		else
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("stream: values unchanged (hash %1); nothing to send", hash));
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: publishes once when nothing is published yet (a client joins before the game start hook).
	static void EnsurePublished()
	{
		if (!s_bPublished)
			Publish();
	}

	//------------------------------------------------------------------------------------------------
	static bool IsPublished()
	{
		return s_bPublished;
	}

	//------------------------------------------------------------------------------------------------
	static string GetPublishedPayload()
	{
		return s_sPublished;
	}

	//------------------------------------------------------------------------------------------------
	static int GetPublishedHash()
	{
		return s_iPublishedHash;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: checks one stream and keeps it when it is good. Does not touch the features; the
	//! replication hooks call OnArrived, which also re-applies them.
	//! \param[in] format The sender's FORMAT_VERSION.
	//! \param[in] hash The sender's hash of payload.
	//! \param[in] payload The payload text.
	static LF_EConfigStreamStatus Receive(int format, int hash, string payload)
	{
		if (format != FORMAT_VERSION)
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("stream: the server sends format %1, this build reads %2; server and client run different builds. Keeping the current config values", format, FORMAT_VERSION));
			return LF_EConfigStreamStatus.BAD_FORMAT;
		}

		if (payload.Hash() != hash)
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("stream: %1 bytes arrived but their hash is %2, not the %3 the server sent; keeping the current config values", payload.Length(), payload.Hash(), hash));
			return LF_EConfigStreamStatus.BAD_HASH;
		}

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext(false);
		if (!ctx.ImportFromString(payload))
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("stream: %1 bytes from the server are not valid JSON; keeping the current config values", payload.Length()));
			return LF_EConfigStreamStatus.BAD_JSON;
		}

		int schema;
		if (!ctx.ReadValue(KEY_SCHEMA, schema) || schema != LF_ConfigService.SCHEMA_VERSION)
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("stream: the server's config schema is %1, this build reads %2; keeping the current config values", schema, LF_ConfigService.SCHEMA_VERSION));
			return LF_EConfigStreamStatus.BAD_SCHEMA;
		}

		if (s_bReceived && hash == s_iReceivedHash)
		{
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("stream: values unchanged (hash %1)", hash));
			return LF_EConfigStreamStatus.UNCHANGED;
		}

		bool ownerLayer;
		ctx.ReadValue(KEY_OWNER, ownerLayer);
		s_sReceived = payload;
		s_iReceivedHash = hash;
		s_bReceived = true;
		LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("stream: received the server's config values, %1 bytes, hash %2, owner layer %3", payload.Length(), hash, ownerLayer));
		return LF_EConfigStreamStatus.APPLIED;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one stream arrived (RplLoad on join, or the change RPC). Accepts it and re-applies
	//! the features that already loaded their config; a feature that loads later reads it then.
	static LF_EConfigStreamStatus OnArrived(int format, int hash, string payload)
	{
		LF_EConfigStreamStatus status = Receive(format, hash, payload);
		if (status == LF_EConfigStreamStatus.APPLIED)
			ApplyToFeatures();
		return status;
	}

	//------------------------------------------------------------------------------------------------
	//! Re-applies every feature that has loaded its config (client after a stream, server after a reload).
	static void ApplyToFeatures()
	{
		LF_ShopManagerComponent shopManager = LF_ShopManagerComponent.GetInstance();
		if (shopManager)
			shopManager.ReloadConfig();

		// The vehicle prices are one cached instance; drop it so the next price read layers the stream.
		LF_VehiclePricesConfig.Reload();

		// The crime catalog is server only and cached the same way; /configreload layers it again.
		LF_CrimeCatalogConfig.Reload();

		// Job XP, levels and licence gates are server-side; the next read layers the owner file again.
		LF_JobsConfig.Reload();

		// Police tunables and ranks, organization tunables: one cached instance each, layered with the
		// owner file (server) or the stream (client) again on the next read.
		LF_PoliceConfig.Reload();
		LF_OrgConfig.Reload();

		// Zone rule sets are server-only and cached the same way.
		LF_ZoneConfig.Reload();

		// The wanted board's poster count and draw radius (server); the boards send the new list.
		LF_WantedPostersConfig.Reload();
		LF_WantedPosters.MarkDirty("config reload");
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a client holds values from the server.
	static bool HasReceived()
	{
		return s_bReceived;
	}

	//------------------------------------------------------------------------------------------------
	static int GetReceivedHash()
	{
		return s_iReceivedHash;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens one section of the received stream, like LF_ConfigService.OpenOwnerSection.
	//! \return A reader on the section, or null on the server, before a stream arrived, or when
	//! the stream has no such section.
	static LF_ConfigReader OpenReceivedSection(string section)
	{
		if (!s_bReceived)
			return null;

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext(false);
		if (!ctx.ImportFromString(s_sReceived))
			return null;

		LF_EConfigJsonKind kind = LF_ConfigReader.KindOf(ctx, section);
		if (kind == LF_EConfigJsonKind.ABSENT)
			return null;

		if (kind != LF_EConfigJsonKind.OBJECT)
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("stream: section %1 is not an object; the section keeps the shipped values", section));
			return null;
		}

		return new LF_ConfigReader(ctx, section);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops what was published and received. World end (LF_StaticReset) and tests.
	static void Reset()
	{
		s_sPublished = string.Empty;
		s_iPublishedHash = 0;
		s_bPublished = false;
		s_sReceived = string.Empty;
		s_iReceivedHash = 0;
		s_bReceived = false;
	}
};
