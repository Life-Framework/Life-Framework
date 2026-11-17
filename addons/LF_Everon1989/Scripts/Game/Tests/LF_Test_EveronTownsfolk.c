// red-proof: observed 2026-11-16 (#2107): the first run read the name through SCR_ChimeraCharacter and failed `- the old man has the name Gaspard Lenoir (is: )` (the prefab is an LF_NpcBaseEntity); `tools\cli test --no-build --red LF_Test_EveronTownsfolk` (RedProof() skips the load): every `- <who> spawns` fails, RED PROOF OK. Green without the hook (pack leg, worlds=LF_Everon1989_Test).
// tier: WORLD
//! #1960 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the three townsfolk
//! (the old man, the cafe owner, the night-shift worker) each load as a person with a name, the
//! `townsfolk` role (the role component overridden by GUID 7A31C05E94D2B861), a `Townsfolk` place id
//! and the 1989 clothes in their loadout. Found by prefab and place id, never by a town coordinate.
//! The pack owns the prefabs; the core names none of them.
class LF_Test_EveronTownsfolk : LF_Test
{
	protected static const string ROLE = "townsfolk";
	protected static const string PLACE_KIND = "Townsfolk";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/townsfolk";
	}

	//------------------------------------------------------------------------------------------------
	//! How many of the NPC's worn items (children carrying a cloth component) have a prefab path ending in each wanted file.
	protected static int CountWorn(IEntity npc, array<string> wanted, out string wornNames)
	{
		int matched = 0;
		IEntity child = npc.GetChildren();
		while (child)
		{
			if (child.FindComponent(BaseLoadoutClothComponent))
			{
				string name = LF_Utils.GetPrefabName(child);
				wornNames += name + " ";
				foreach (string file : wanted)
				{
					if (name.IndexOf(file) != -1)
					{
						matched++;
						break;
					}
				}
			}

			child = child.GetSibling();
		}

		return matched;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPerson(LF_TestContext ctx, ResourceName prefab, string who, string fullName, string placeId, array<string> clothes)
	{
		// The act: load the townsperson's prefab.
		IEntity npc;
		if (!RedProof())
			npc = LF_Utils.SpawnEntityPrefab(prefab, "0 -50 0");

		ctx.True(npc != null, who + " spawns");
		if (!npc)
			return;

		// Name.
		string actual;
		CharacterIdentityComponent identityComp = CharacterIdentityComponent.Cast(npc.FindComponent(CharacterIdentityComponent));
		if (identityComp && identityComp.GetIdentity())
			actual = identityComp.GetIdentity().GetName() + " " + identityComp.GetIdentity().GetSurname();

		ctx.True(actual.IndexOf(fullName) != -1, who + " has the name " + fullName + " (is: " + actual + ")");

		// Role, from the role component the prefab overrides by GUID.
		ctx.EqualStr(ROLE, LF_Roles.RoleOf(npc), who + " has the townsfolk role");

		// Place: found by kind and id.
		LF_PlaceComponent place = LF_PlaceComponent.Cast(npc.FindComponent(LF_PlaceComponent));
		ctx.NotNull(place, who + " has a place component");
		if (place)
			ctx.EqualStr(placeId, place.GetPlaceId(), who + " has the place id " + placeId);

		// Clothes: every authored slot is worn, jacket, pants and boots at least.
		string wornNames;
		int matched = CountWorn(npc, clothes, wornNames);
		ctx.Equal(clothes.Count(), matched, who + " wears all " + clothes.Count() + " period clothes (wears: " + wornNames + ")");

		SCR_EntityHelper.DeleteEntityAndChildren(npc);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		CheckPerson(ctx, "{ED4DD018891346D0}Configs/Npcs/LF_Npc_Townsfolk_OldMan.et", "the old man", "Gaspard Lenoir", "old_man",
			{"Jacket_Denim_01_brownShirt.et", "Pants_Trousers_01_DarkBrown.et", "CombatBoots_US_01.et", "Hat_FlatCap_01_lightgrey.et"});

		CheckPerson(ctx, "{83D04234954B8914}Configs/Npcs/LF_Npc_Townsfolk_CafeOwner.et", "the cafe owner", "Odile Marchand", "cafe_owner",
			{"Shirt_CottonShirt_01_V2.et", "Pants_Trousers_01_DarkGrey.et", "CombatBoots_US_01.et"});

		CheckPerson(ctx, "{FB61502DB411C69E}Configs/Npcs/LF_Npc_Townsfolk_NightShift.et", "the night-shift worker", "Duval", "night_shift",
			{"Shirt_Turtleneck_01_grey.et", "Pants_Trousers_01_DarkGrey_Dirty.et", "CombatBoots_US_01.et", "Hat_Knitted_01_black.et"});
	}
}
