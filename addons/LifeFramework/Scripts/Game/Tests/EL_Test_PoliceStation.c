// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_PoliceDutySwap,EL_Test_PoliceDeathOnDuty,
// EL_Test_PoliceWantedBoard,EL_Test_PoliceStationWorld,EL_Test_PoliceItemState,EL_Test_CharacterCreationRules`
// with one break per test, all restored afterwards (SUMMARY tier=only passed=0 failed=6 total=6):
// police/duty-swap: ClockOut kept the civilian stash and SetAmmo was skipped ->
//   `- no civilian inventory is stored while it is worn`, `- the magazine keeps the rounds left at clock-out (expected 11, got 15)`
// police/death-on-duty: OnDied did not hand the civilian stash to the carried slot -> `- the respawn has a carried inventory`
// police/wanted-board: BuildPosters gave the place to every reader -> `- the public poster does not say where (expected '', got 'Fuel station')`
// police/station: Roadmap_Police_Desk back on the plain vanilla desk -> `- the desk carries the Clock in / Clock out action`
//   (the action now sits on the desk's punch clock; the assertion reads "the desk's punch clock carries ...")
// police/item-state: [NonSerialized()] removed from m_iAmmo -> `- the ammo field stays out of the carried record format`

//------------------------------------------------------------------------------------------------
//! Shared setup for the duty desk tests: a registered account with a character, on the police
//! roster for the test's duration.
class EL_TestPoliceKit
{
	static const ResourceName M9_PREFAB = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	//------------------------------------------------------------------------------------------------
	static EL_PlayerAccount Account(string uid, bool roster)
	{
		EL_PlayerAccount account = EL_PlayerAccountManager.GetOrCreate(uid);
		if (!account.GetActiveCharacter())
			account.AddCharacter(EL_PlayerCharacter.Create(EL_TestCharacterFixture.CHARACTER_PREFAB, "Test", "Officer", 30), true);

		account.SetOnDuty(false);
		account.SetFaction(EL_Faction.CIVILIAN);
		if (roster)
			EL_WhitelistManager.AddFactionWhitelist(uid, EL_EFactionType.POLICE);
		else
			EL_WhitelistManager.RemoveFactionWhitelist(uid, EL_EFactionType.POLICE);

		return account;
	}

	//------------------------------------------------------------------------------------------------
	static void Release(string uid)
	{
		EL_WhitelistManager.RemoveFactionWhitelist(uid, EL_EFactionType.POLICE);
		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetFromCache(uid);
		if (account && account.GetActiveCharacter())
		{
			string id = account.GetActiveCharacter().GetId();
			LF_StorageSnapshot.Clear(id, LF_StorageSnapshot.SLOT_CARRIED);
			LF_StorageSnapshot.Clear(id, LF_DutyLocker.SLOT_POLICE);
			LF_StorageSnapshot.Clear(id, LF_DutyLocker.SLOT_CIVILIAN);
		}

		if (account)
		{
			account.SetOnDuty(false);
			account.SetFaction(EL_Faction.CIVILIAN);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The first magazine anywhere under root, or null.
	static BaseMagazineComponent FindMagazine(IEntity root)
	{
		if (!root)
			return null;

		array<Managed> storages = {};
		root.FindComponents(BaseInventoryStorageComponent, storages);
		foreach (Managed managed : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(managed);
			if (!storage)
				continue;

			for (int slot = 0; slot < storage.GetSlotsCount(); slot++)
			{
				IEntity item = storage.Get(slot);
				if (!item)
					continue;

				BaseMagazineComponent magazine = BaseMagazineComponent.Cast(item.FindComponent(BaseMagazineComponent));
				if (magazine)
					return magazine;

				magazine = FindMagazine(item);
				if (magazine)
					return magazine;
			}
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Clock in stores the civilian inventory and issues the starter police kit; clock out stores
//! the police inventory (with what the officer picked up, and the rounds left in the magazine)
//! and returns the civilian one; clocking in again brings the police inventory back.
// tier: WORLD
class EL_Test_PoliceDutySwap : EL_Test
{
	protected static const string UID = "police-duty-swap";
	protected static const string OUTSIDER = "police-duty-outsider";

	override string GetName()
	{
		return "police/duty-swap";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_PlayerAccount outsider = EL_TestPoliceKit.Account(OUTSIDER, false);
		ctx.EqualStr("#LF-Duty_NotOnRoster", LF_DutyLocker.RefuseClockIn(outsider), "a player off the roster cannot clock in");
		ctx.EqualStr("#LF-Duty_NotOn", LF_DutyLocker.RefuseClockOut(outsider), "a civilian cannot clock out");

		EL_PlayerAccount account = EL_TestPoliceKit.Account(UID, true);
		string characterId = account.GetActiveCharacter().GetId();
		IEntity body = EL_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!body || ctx.FailureCount() > 0)
		{
			EL_TestPoliceKit.Release(UID);
			EL_TestCharacterFixture.Delete(body);
			return;
		}

		// Clock in: the starter kit, nothing civilian left on the body.
		ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body), "a rostered player clocks in");
		ctx.True(account.IsOnDuty(), "on duty after clock-in");
		ctx.Equal(EL_Faction.POLICE, account.GetFaction(), "police while clocked in");
		ctx.True(EL_TestCharacterFixture.HasItem(body, EL_TestPoliceKit.M9_PREFAB), "the starter police kit carries the M9");
		ctx.False(EL_TestCharacterFixture.HasItem(body, EL_TestCarriedInventoryKit.APPLE_PREFAB), "no civilian apple stays on the officer");
		LF_StorageSnapshotRecord civilian = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_CIVILIAN);
		ctx.NotNull(civilian, "the civilian inventory is stored at the desk");
		if (civilian)
			ctx.Equal(3, civilian.CountPrefab(EL_TestCarriedInventoryKit.APPLE_PREFAB), "the stored civilian inventory holds the 3 apples");
		ctx.False(LF_StorageSnapshot.Has(characterId, LF_DutyLocker.SLOT_POLICE), "no police inventory is stored while it is worn");
		ctx.EqualStr("#LF-Duty_AlreadyOn", LF_DutyLocker.RefuseClockIn(account), "a second clock-in is refused");

		// On shift: the officer takes 2 apples and fires 4 rounds.
		ctx.Equal(2, EL_InventoryUtils.AddAmount(body, EL_TestCarriedInventoryKit.APPLE_PREFAB, 2), "the officer picks up 2 apples");
		BaseMagazineComponent magazine = EL_TestPoliceKit.FindMagazine(body);
		ctx.NotNull(magazine, "the police kit carries a magazine");
		int rounds = -1;
		if (magazine)
		{
			rounds = Math.Max(0, magazine.GetMaxAmmoCount() - 4);
			magazine.SetAmmoCount(rounds);
		}

		// Clock out: civilian inventory back, police items gone.
		ctx.EqualStr("", LF_DutyLocker.ClockOut(account, body), "the officer clocks out");
		ctx.False(account.IsOnDuty(), "off duty after clock-out");
		ctx.Equal(EL_Faction.CIVILIAN, account.GetFaction(), "civilian after clock-out");
		ctx.False(EL_TestCharacterFixture.HasItem(body, EL_TestPoliceKit.M9_PREFAB), "the M9 does not leave with the civilian");
		ctx.Equal(3, EL_InventoryUtils.GetAmount(body, EL_TestCarriedInventoryKit.APPLE_PREFAB), "the civilian's 3 apples are back, the police 2 are not");
		ctx.Equal(1, EL_TestCarriedInventoryKit.CountInside(body, EL_TestCarriedInventoryKit.BACKPACK_PREFAB, EL_TestCarriedInventoryKit.WATER_PREFAB), "the water bottle is back inside the backpack");
		ctx.False(LF_StorageSnapshot.Has(characterId, LF_DutyLocker.SLOT_CIVILIAN), "no civilian inventory is stored while it is worn");
		LF_StorageSnapshotRecord police = LF_StorageSnapshot.Get(characterId, LF_DutyLocker.SLOT_POLICE);
		ctx.NotNull(police, "the police inventory is stored at the desk");
		if (police)
			ctx.Equal(2, police.CountPrefab(EL_TestCarriedInventoryKit.APPLE_PREFAB), "the stored police inventory holds the 2 apples");

		LF_StorageSnapshotRecord carried = LF_StorageSnapshot.Get(characterId, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(carried, "the carried snapshot follows the swap");
		if (carried)
			ctx.Equal(3, carried.CountPrefab(EL_TestCarriedInventoryKit.APPLE_PREFAB), "the carried snapshot is the civilian inventory");

		// Clock in again: the same police inventory, magazine rounds included.
		ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body), "the officer clocks in again");
		ctx.True(EL_TestCharacterFixture.HasItem(body, EL_TestPoliceKit.M9_PREFAB), "the M9 is back");
		ctx.Equal(2, EL_InventoryUtils.GetAmount(body, EL_TestCarriedInventoryKit.APPLE_PREFAB), "the police inventory's 2 apples are back");
		BaseMagazineComponent restored = EL_TestPoliceKit.FindMagazine(body);
		ctx.NotNull(restored, "the magazine is back");
		if (restored)
			ctx.Equal(rounds, restored.GetAmmoCount(), "the magazine keeps the rounds left at clock-out");

		LF_DutyLocker.ClockOut(account, body);
		EL_TestPoliceKit.Release(UID);
		EL_TestPoliceKit.Release(OUTSIDER);
		EL_TestCharacterFixture.Delete(body);
	}
}

//------------------------------------------------------------------------------------------------
//! Death on duty: the shift ends, the respawn carries the stored civilian inventory, and no
//! police inventory stays stored next to the lootable body.
// tier: WORLD
class EL_Test_PoliceDeathOnDuty : EL_Test
{
	protected static const string UID = "police-death-on-duty";

	override string GetName()
	{
		return "police/death-on-duty";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_PlayerAccount account = EL_TestPoliceKit.Account(UID, true);
		string characterId = account.GetActiveCharacter().GetId();
		IEntity body = EL_TestCarriedInventoryKit.SpawnLoaded(ctx);
		if (!body || ctx.FailureCount() > 0)
		{
			EL_TestPoliceKit.Release(UID);
			EL_TestCharacterFixture.Delete(body);
			return;
		}

		ctx.EqualStr("", LF_DutyLocker.ClockIn(account, body), "clocked in");

		// The death hook order of EL_SpawnLogic.OnPlayerKilled_S.
		LF_StorageSnapshot.OnCharacterDied(characterId);
		ctx.True(LF_DutyLocker.OnDied(account), "death on duty ends the shift");
		ctx.False(account.IsOnDuty(), "off duty after death");
		ctx.Equal(EL_Faction.CIVILIAN, account.GetFaction(), "civilian after death");
		ctx.False(LF_StorageSnapshot.Has(characterId, LF_DutyLocker.SLOT_POLICE), "no stored police inventory beside the body");
		ctx.False(LF_StorageSnapshot.Has(characterId, LF_DutyLocker.SLOT_CIVILIAN), "the civilian stash moved out of the desk");
		LF_StorageSnapshotRecord carried = LF_StorageSnapshot.Get(characterId, LF_StorageSnapshot.SLOT_CARRIED);
		ctx.NotNull(carried, "the respawn has a carried inventory");
		if (carried)
		{
			ctx.Equal(3, carried.CountPrefab(EL_TestCarriedInventoryKit.APPLE_PREFAB), "the respawn carries the civilian apples");
			ctx.Equal(0, carried.CountPrefab(EL_TestPoliceKit.M9_PREFAB), "the respawn carries no police M9");
		}

		ctx.False(LF_DutyLocker.OnDied(account), "a civilian death changes nothing");

		EL_TestPoliceKit.Release(UID);
		EL_TestCharacterFixture.Delete(body);
	}
}

//------------------------------------------------------------------------------------------------
//! Posters: a description and no name until identified; the place only for on-duty police; the
//! poster comes down when wanted returns to 0.
// tier: LOGIC
class EL_Test_PoliceWantedBoard : EL_Test
{
	protected static const string ROBBER = "police-board-robber";

	override string GetName()
	{
		return "police/wanted-board";
	}

	override void Run(EL_TestContext ctx)
	{
		LF_WantedReports.ResetStatics();
		EL_PlayerAccount robber = EL_PlayerAccountManager.GetOrCreate(ROBBER);
		if (!robber.GetActiveCharacter())
			robber.AddCharacter(EL_PlayerCharacter.Create(EL_TestCharacterFixture.CHARACTER_PREFAB, "Rob", "Tilly", 25), true);

		robber.SetWantedLevel(1);
		LF_WantedReports.Add(ROBBER, LF_WantedReports.OFFENCE_ROBBERY, "#AR-Jacket_M88|#AR-Pants_M88", "", "Fuel station", false);

		array<string> names = {};
		array<string> offences = {};
		array<string> clothing = {};
		array<string> vehicles = {};
		array<string> places = {};
		array<int> levels = {};

		ctx.Equal(1, LF_WantedBoard.BuildPosters(true, 8, names, offences, clothing, vehicles, places, levels), "one poster for the officer");
		ctx.EqualStr("", names[0], "the officer's poster has no name before the suspect is identified");
		ctx.EqualStr(LF_WantedReports.OFFENCE_ROBBERY, offences[0], "the poster names the offence");
		ctx.EqualStr("#AR-Jacket_M88|#AR-Pants_M88", clothing[0], "the poster describes the clothing");
		ctx.EqualStr("Fuel station", places[0], "the officer sees where it happened");
		ctx.Equal(1, levels[0], "the poster shows wanted level 1");

		LF_WantedBoard.BuildPosters(false, 8, names, offences, clothing, vehicles, places, levels);
		ctx.EqualStr("", places[0], "the public poster does not say where");
		ctx.EqualStr("", names[0], "the public poster has no name before identification");

		ctx.Equal(1, LF_WantedReports.Identify(ROBBER, "test ID check"), "an ID check identifies the open report");
		LF_WantedBoard.BuildPosters(true, 8, names, offences, clothing, vehicles, places, levels);
		ctx.EqualStr("Rob Tilly", names[0], "after the ID check the same poster names the robber");
		LF_WantedBoard.BuildPosters(false, 8, names, offences, clothing, vehicles, places, levels);
		ctx.EqualStr("Rob Tilly", names[0], "the public poster names them too");

		LF_WantedReports.Add(ROBBER, LF_WantedReports.OFFENCE_BREAK_IN, "", "#AR-Vehicle_S105", "", true);
		LF_WantedBoard.BuildPosters(true, 1, names, offences, clothing, vehicles, places, levels);
		ctx.Equal(1, offences.Count(), "the board shows at most the configured number of posters");
		ctx.EqualStr(LF_WantedReports.OFFENCE_BREAK_IN, offences[0], "the newest poster comes first");

		array<ref LF_WantedReportRecord> saved = LF_WantedReports.ExportAll();
		LF_WantedReports.ResetStatics();
		LF_WantedReports.ApplyAll(saved);
		ctx.Equal(2, LF_WantedReports.CountFor(ROBBER), "the reports load back");
		ctx.True(LF_WantedReports.IsIdentified(ROBBER), "the identified flag loads back");

		robber.SetWantedLevel(0);
		ctx.Equal(0, LF_WantedReports.CountFor(ROBBER), "the posters come down when no longer wanted");
		ctx.Equal(0, LF_WantedBoard.BuildPosters(true, 8, names, offences, clothing, vehicles, places, levels), "an empty board");

		LF_WantedReports.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The placed station: the desk clocks in with a punch-clock sound, the board reads posters and
//! prints its title, and the old debug duty point no longer offers duty.
// tier: WORLD
class EL_Test_PoliceStationWorld : EL_Test
{
	override string GetName()
	{
		return "police/station";
	}

	override void Run(EL_TestContext ctx)
	{
		IEntity desk = GetGame().GetWorld().FindEntityByName("Roadmap_Police_Desk");
		ctx.NotNull(desk, "Roadmap_Police_Desk is placed");
		if (desk)
		{
			// The action sits on the punch clock itself: the interaction trace hits the clock, so
			// the prompt shows when the player looks at it.
			IEntity clock = desk.GetChildren();
			while (clock && !HasAction(clock, EL_DutyAction))
				clock = clock.GetSibling();

			ctx.NotNull(clock, "the desk's punch clock carries the Clock in / Clock out action");
			if (clock)
				ctx.NotNull(clock.FindComponent(SCR_SoundDataComponent), "the punch clock carries its click sound");
		}

		IEntity board = GetGame().GetWorld().FindEntityByName("Roadmap_Police_WantedBoard");
		ctx.NotNull(board, "Roadmap_Police_WantedBoard is placed");
		if (board)
		{
			ctx.True(HasAction(board, LF_WantedBoardReadAction), "the board carries the read action");
			ctx.NotNull(board.FindComponent(LF_SignComponent), "the board prints its title");
			if (desk)
				ctx.True(vector.Distance(desk.GetOrigin(), board.GetOrigin()) < 12, "the board stands at the station beside the desk");
		}

		IEntity debugDesk = GetGame().GetWorld().FindEntityByName("PoliceStation_Debug");
		if (debugDesk)
			ctx.False(HasAction(debugDesk, EL_DutyAction), "the old debug duty point no longer offers duty");
	}

	protected bool HasAction(IEntity entity, typename actionType)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(entity.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return false;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (action && action.Type().IsInherited(actionType))
				return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Magazine ammo rides beside a snapshot (itemstate payload) and never inside the carried
//! record's positional format.
// tier: LOGIC
class EL_Test_PoliceItemState : EL_Test
{
	override string GetName()
	{
		return "police/item-state";
	}

	override void Run(EL_TestContext ctx)
	{
		LF_StorageItemRecord item = LF_StorageItemRecord.Create("{9C05543A503DB80E}Prefabs/Weapons/Magazines/Magazine_9x19_M9_15rnd_Ball.et", 1, -1, 0, 0);
		item.m_iAmmo = 11;
		JsonSaveContext json = new JsonSaveContext(false);
		json.WriteValue("item", item);
		string text = json.SaveToString();
		ctx.True(text.Contains("m_sPrefab"), "the record serializes its fields");
		ctx.False(text.Contains("m_iAmmo"), "the ammo field stays out of the carried record format");

		LF_StorageSnapshotRecord record = LF_StorageSnapshotRecord.Create("item-state-owner", LF_DutyLocker.SLOT_POLICE);
		record.m_aItems.Insert(LF_StorageItemRecord.Create("{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et", 1, -1, 0, 0));
		record.m_aItems.Insert(item);
		LF_StorageSnapshot.Set(record);

		array<ref LF_StorageItemStateRecord> states = LF_StorageSnapshot.ExportItemState();
		LF_StorageItemStateRecord mine;
		foreach (LF_StorageItemStateRecord state : states)
		{
			if (state.m_sOwnerKey == "item-state-owner")
				mine = state;
		}

		ctx.NotNull(mine, "a snapshot with a magazine exports item state");
		item.m_iAmmo = -1;
		ctx.True(LF_StorageSnapshot.ApplyItemState(states) >= 1, "the item state applies to its snapshot");
		ctx.Equal(11, LF_StorageSnapshot.Get("item-state-owner", LF_DutyLocker.SLOT_POLICE).m_aItems[1].m_iAmmo, "the magazine gets its 11 rounds back");

		LF_StorageSnapshot.Clear("item-state-owner", LF_DutyLocker.SLOT_POLICE);
	}
}
