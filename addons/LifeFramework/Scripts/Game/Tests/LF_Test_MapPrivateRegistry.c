// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapPrivateEntitled,LF_Test_MapPrivateRegistry: RED-OK (RedProof() registers the armoury as the old ungated code did, so "a client does not register a private place" and the three "holds no" checks fail). Green with the RegisterOn gate.
// tier: WORLD
//! map/private-registry (#2492, map review 2026-11-17 finding 5): a client's LF_Places never holds a private
//! POI kind, so a modified client has neither its id nor its position. The server holds it and sends a player
//! only what that player may see: a stranger gets neither the role place nor an unheard discover place; once the
//! character has heard of the discover place it is in their set, and the role place still is not.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MapPrivateRegistry : LF_Test
{
	//! Core rows: PoliceArmoury is ROLE (police), Workplace is DISCOVER (Configs/Map/LF_MapPois.conf).
	protected static const string KIND_ROLE = "PoliceArmoury";
	protected static const string KIND_DISCOVER = "Workplace";
	//! A kind with no POI row: not private, so it still registers on a client.
	protected static const string KIND_OTHER = "MapPrivateTestOther";
	protected static const string ID_ROLE = "mpr-armoury";
	protected static const string ID_DISCOVER = "mpr-workplace";
	protected static const string ID_OTHER = "mpr-other";
	protected static const string CHARACTER = "mpr-character";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/private-registry";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasId(notnull array<ref LF_MapPoiPlace> places, string id)
	{
		foreach (LF_MapPoiPlace place : places)
		{
			if (place && place.m_sId == id)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapPoiConfig config = LF_MapPoiConfig.Get();
		ctx.NotNull(config, "the POI table loads");
		if (!config)
			return;

		ctx.True(LF_Places.IsServerOnlyKind(KIND_ROLE), "a ROLE kind is server-only");
		ctx.True(LF_Places.IsServerOnlyKind(KIND_DISCOVER), "a DISCOVER kind is server-only");
		ctx.False(LF_Places.IsServerOnlyKind("Bank"), "a PUBLIC kind is not server-only");
		ctx.False(LF_Places.IsServerOnlyKind(KIND_OTHER), "a kind with no POI row is not server-only");

		IEntity anchor = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BREACH_DOOR);
		ctx.NotNull(anchor, "the annex breach door anchors the fixtures");
		if (!anchor)
			return;

		vector at = anchor.GetOrigin() + "0 -40 0";
		IEntity armoury = LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at, "0 0 0", false);
		IEntity workplace = LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at + "3 0 0", "0 0 0", false);
		IEntity other = LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at + "6 0 0", "0 0 0", false);
		ctx.True(armoury && workplace && other, "three fixture places spawn");
		if (armoury && workplace && other)
			RunWith(ctx, config, armoury, workplace, other);

		SCR_EntityHelper.DeleteEntityAndChildren(armoury);
		SCR_EntityHelper.DeleteEntityAndChildren(workplace);
		SCR_EntityHelper.DeleteEntityAndChildren(other);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWith(LF_TestContext ctx, LF_MapPoiConfig config, IEntity armoury, IEntity workplace, IEntity other)
	{
		// As a client: the private place is refused and nothing of it is held. The red proof registers as
		// the old code did, with no gate.
		bool asServer = false;
		if (RedProof())
			asServer = true;

		ctx.False(LF_Places.RegisterOn(KIND_ROLE, ID_ROLE, armoury, asServer), "a client does not register a private place");
		ctx.True(LF_Places.Find(KIND_ROLE, ID_ROLE) == null, "a client holds no entity for the private id");
		vector origin = "1 2 3";
		ctx.False(LF_Places.FindOrigin(KIND_ROLE, ID_ROLE, origin), "a client has no position for the private place");
		array<string> ids = {};
		array<IEntity> entities = {};
		LF_Places.FindAllWithIds(KIND_ROLE, ids, entities);
		ctx.True(ids.Find(ID_ROLE) == -1, "a client lists no private id");
		ctx.True(LF_Places.RegisterOn(KIND_OTHER, ID_OTHER, other, false), "a client still registers a kind with no private row");

		// As the server: it holds both private places and decides per player.
		ctx.True(LF_Places.RegisterOn(KIND_ROLE, ID_ROLE, armoury, true), "the server registers the role place");
		ctx.True(LF_Places.RegisterOn(KIND_DISCOVER, ID_DISCOVER, workplace, true), "the server registers the discover place");

		array<ref LF_MapPoiPlace> places = {};
		LF_MapPrivatePlaces.GatherPrivate(config, places);
		ctx.True(HasId(places, ID_ROLE) && HasId(places, ID_DISCOVER), "the server gathers both private places");
		ctx.False(HasId(places, ID_OTHER), "a kind with no row is not a private place");

		LF_MapPoiAudience stranger = LF_MapPrivatePlaces.ServerAudience(CHARACTER, null, places);
		array<ref LF_MapPoiPlace> sent = {};
		LF_MapPrivatePlaces.Entitled(config, stranger, places, sent);
		ctx.False(HasId(sent, ID_ROLE), "a stranger is never sent the role place");
		ctx.False(HasId(sent, ID_DISCOVER), "a stranger is never sent an unheard discover place");

		LF_MapKnown.HearCharacter(CHARACTER, ID_DISCOVER);
		LF_MapPoiAudience heard = LF_MapPrivatePlaces.ServerAudience(CHARACTER, null, places);
		sent.Clear();
		LF_MapPrivatePlaces.Entitled(config, heard, places, sent);
		ctx.True(HasId(sent, ID_DISCOVER), "once heard of, the discover place is in the character's set");
		ctx.False(HasId(sent, ID_ROLE), "hearing of one place does not reveal the role place");

		array<string> kinds = {};
		array<string> wireIds = {};
		array<int> positions = {};
		LF_MapPrivatePlaces.Encode(sent, kinds, wireIds, positions);
		array<ref LF_MapPoiPlace> back = {};
		LF_MapPrivatePlaces.Decode(kinds, wireIds, positions, back);
		ctx.True(HasId(back, ID_DISCOVER), "the wire form carries the discover place");
		foreach (LF_MapPoiPlace place : back)
		{
			if (place.m_sId == ID_DISCOVER)
				ctx.True(vector.Distance(place.m_vPos, workplace.GetOrigin()) < 0.1, "the wire form keeps the position within 0.1 m");
		}
	}
}
