// red-proof: 2026-11-17, cli test --no-build --red LF_Test_MapPrivateEntitled,LF_Test_MapPrivateRegistry: RED-OK (RedProof() sends every private place, as every client used to hold them, so "a player who is not entitled is sent no private place" fails). Green with LF_MapPrivatePlaces.Entitled.
//! map/private-entitled (#2492): the private places the server sends one player are exactly what the
//! visibility rule gives that player. A player who knows nothing and holds no role is sent no private place:
//! not the hidden fence, not the police armoury, not an unheard discover place. A heard-of discover place, a
//! fence the character met and, on duty, the armoury join the set; a public place is never part of it. Also
//! the wire form, the duty mask that triggers a resend, and the server scan's grid.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MapPrivateEntitled : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/private-entitled";
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
	//! What the server sends: the rule. The red proof sends every private place, as every client used to hold.
	protected void Send(LF_MapPoiConfig config, LF_MapPoiAudience audience, array<ref LF_MapPoiPlace> places, array<ref LF_MapPoiPlace> into)
	{
		if (!RedProof())
		{
			LF_MapPrivatePlaces.Entitled(config, audience, places, into);
			return;
		}

		foreach (LF_MapPoiPlace place : places)
		{
			if (LF_MapPrivatePlaces.IsPrivate(config, place.m_sKind))
				into.Insert(place);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapPoiConfig config = new LF_MapPoiConfig();
		config.m_aKinds = new array<ref LF_MapPoiKind>();
		config.m_aKinds.Insert(LF_MapPoiKind.Make("MpPub", LF_EMapPoiVisibility.PUBLIC));
		config.m_aKinds.Insert(LF_MapPoiKind.Make("MpDisc", LF_EMapPoiVisibility.DISCOVER));
		config.m_aKinds.Insert(LF_MapPoiKind.Make("MpRole", LF_EMapPoiVisibility.ROLE, LF_MapShares.ROLE_POLICE));
		config.m_aKinds.Insert(LF_MapPoiKind.Make("MpFence", LF_EMapPoiVisibility.HIDDEN));

		array<ref LF_MapPoiPlace> places = {};
		places.Insert(LF_MapPoiPlace.Make("MpPub", "mp-pub", "-9100 0 -9100", true));
		places.Insert(LF_MapPoiPlace.Make("MpDisc", "mp-disc-a", "-9200 0 -9200", true));
		places.Insert(LF_MapPoiPlace.Make("MpDisc", "mp-disc-b", "-9210 0 -9200", true));
		places.Insert(LF_MapPoiPlace.Make("MpRole", "mp-armoury", "-9300 0 -9300", true));
		places.Insert(LF_MapPoiPlace.Make("MpFence", "mp-fence", "-9400 0 -9400", true));

		ctx.False(LF_MapPrivatePlaces.IsPrivate(config, "MpPub"), "a PUBLIC row is not private");
		ctx.True(LF_MapPrivatePlaces.IsPrivate(config, "MpFence"), "a HIDDEN row is private");
		ctx.False(LF_MapPrivatePlaces.IsPrivate(config, "MpNoRow"), "a kind with no row is not private");

		// The stranger: no role, knows nothing.
		array<ref LF_MapPoiPlace> sent = {};
		Send(config, new LF_MapPoiAudience(), places, sent);
		ctx.Equal(0, sent.Count(), "a player who is not entitled is sent no private place");
		ctx.False(HasId(sent, "mp-fence"), "the fence never reaches a stranger");

		// Heard of one discover place, met the fence.
		LF_MapPoiAudience known = new LF_MapPoiAudience();
		known.SetKnown("mp-disc-a", LF_EMapPoiKnown.HEARD);
		known.SetKnown("mp-fence", LF_EMapPoiKnown.VISITED);
		sent.Clear();
		Send(config, known, places, sent);
		ctx.True(HasId(sent, "mp-disc-a"), "a heard-of discover place is sent");
		ctx.False(HasId(sent, "mp-disc-b"), "a discover place next to it is not");
		ctx.True(HasId(sent, "mp-fence"), "a fence the character met is sent");
		ctx.False(HasId(sent, "mp-armoury"), "the armoury is not sent off duty");
		ctx.False(HasId(sent, "mp-pub"), "a public place is never part of the private set");

		// On duty as police.
		LF_MapPoiAudience police = new LF_MapPoiAudience();
		int onDuty = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, 1);
		int offDuty = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, false, 0, 1);
		police.AddRolesFromState(onDuty);
		sent.Clear();
		Send(config, police, places, sent);
		ctx.Equal(1, sent.Count(), "an officer on duty is sent the armoury and nothing else");
		ctx.True(HasId(sent, "mp-armoury"), "the officer's set holds the armoury");

		ctx.True(LF_MapPrivatePlaces.RoleMask(onDuty) != LF_MapPrivatePlaces.RoleMask(offDuty), "going off duty changes the role mask (a resend)");
		ctx.Equal(LF_MapPrivatePlaces.RoleMask(offDuty), LF_MapPrivatePlaces.RoleMask(0), "off duty holds no role");

		// The wire form.
		array<string> kinds = {};
		array<string> ids = {};
		array<int> positions = {};
		array<ref LF_MapPoiPlace> pair = {};
		pair.Insert(LF_MapPoiPlace.Make("MpDisc", "mp-wire", "1234.56 78.9 -12.34", true));
		LF_MapPrivatePlaces.Encode(pair, kinds, ids, positions);
		ctx.Equal(3, positions.Count(), "one place is three position ints");
		array<ref LF_MapPoiPlace> back = {};
		ctx.Equal(1, LF_MapPrivatePlaces.Decode(kinds, ids, positions, back), "one place decodes");
		ctx.True(back.Count() == 1 && back[0].m_sId == "mp-wire" && back[0].m_sKind == "MpDisc", "the id and kind survive");
		if (back.Count() == 1)
			ctx.True(vector.Distance(back[0].m_vPos, "1234.56 78.9 -12.34") < 0.1, "the position survives within 0.1 m");

		positions.Remove(2);
		back.Clear();
		ctx.Equal(0, LF_MapPrivatePlaces.Decode(kinds, ids, positions, back), "a short position array decodes nothing");

		// The server scan's grid: within the discovery radius, across a cell edge, and not beyond.
		LF_MapDiscoverIndex index = new LF_MapDiscoverIndex();
		index.Add("mp-grid-near", Vector(LF_MapDiscoverIndex.CELL - 1, 0, 10));
		index.Add("mp-grid-far", Vector(LF_MapDiscoverIndex.CELL + 30, 0, 10));
		array<string> near = {};
		index.Near(Vector(LF_MapDiscoverIndex.CELL + 5, 0, 10), LF_MapKnownRules.DEFAULT_RADIUS, near);
		ctx.True(near.Find("mp-grid-near") != -1, "a place 6 m away across a cell edge is in reach");
		ctx.True(near.Find("mp-grid-far") == -1, "a place 25 m away is not");
	}
}
