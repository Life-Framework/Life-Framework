// red-proof: change an expected value (e.g. wanted clamp 5 -> 6) or break the
// clamp in LF_PlayerAccount.SetWantedLevel, then run the fast tier.

// tier: LOGIC
class LF_Test_AccountWantedClamp : LF_Test
{
	override string GetName()
	{
		return "account/wanted-clamp";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerAccount account = LF_PlayerAccount.Create("test-account");

		ctx.Equal(0, account.GetWantedLevel(), "fresh account starts at wanted level 0");
		ctx.Equal(LF_Faction.CIVILIAN, account.GetFaction(), "fresh account defaults to civilian");

		account.SetWantedLevel(3);
		ctx.Equal(3, account.GetWantedLevel(), "SetWantedLevel in range is kept");

		account.SetWantedLevel(99);
		ctx.Equal(5, account.GetWantedLevel(), "SetWantedLevel clamps at 5");

		account.SetWantedLevel(-3);
		ctx.Equal(0, account.GetWantedLevel(), "SetWantedLevel clamps at 0");

		account.IncreaseWantedLevel();
		ctx.Equal(1, account.GetWantedLevel(), "IncreaseWantedLevel raises by 1");

		account.IncreaseWantedLevel(6);
		ctx.Equal(5, account.GetWantedLevel(), "IncreaseWantedLevel clamps at 5");

		account.IncreaseWantedLevel(-99);
		ctx.Equal(0, account.GetWantedLevel(), "IncreaseWantedLevel clamps at 0");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_AccountCharacterRoster : LF_Test
{
	override string GetName()
	{
		return "account/character-roster";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerAccount account = LF_PlayerAccount.Create("test-account-characters");

		LF_PlayerCharacter a = LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "John", "Doe", 30);
		LF_PlayerCharacter b = LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "Jane", "Roe", 28);

		ctx.EqualStr("John Doe", a.GetFullName(), "full name is first plus last");
		ctx.Equal(30, a.GetAge(), "character age is preserved");

		account.AddCharacter(a, true);
		ctx.True(account.HasCharacters(), "account reports characters after add");
		ctx.True(account.GetActiveCharacter() == a, "first character added with setAsActive is active");

		account.AddCharacter(b);
		ctx.Equal(2, account.GetCharacters().Count(), "roster holds both characters");

		account.SetActiveCharacter(b);
		ctx.True(account.GetActiveCharacter() == b, "SetActiveCharacter selects b");

		account.RemoveCharacter(a);
		ctx.Equal(1, account.GetCharacters().Count(), "RemoveCharacter shrinks the roster");
		ctx.True(account.GetActiveCharacter() == b, "removing a non-active character keeps b active");
	}
};

//------------------------------------------------------------------------------------------------
//! Save-data round trip through the public record + manager seam.
// tier: LOGIC
class LF_Test_AccountSaveRoundtrip : LF_Test
{
	override string GetName()
	{
		return "account/save-roundtrip";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerAccountManager.Reset();
		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();

		LF_PlayerAccount account = LF_PlayerAccount.Create("test-account-save");
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		account.SetWantedLevel(2);
		account.AddCharacter(LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "John", "Doe", 30), true);
		manager.AddToCache(account);

		// Export -> rebuild a fresh manager from the records (the load path)
		ref array<ref LF_PlayerAccountRecord> records = LF_PlayerAccountManager.ExportAll();
		LF_PlayerAccountManager.Reset();
		LF_PlayerAccountManager.ApplyAll(records);

		LF_PlayerAccount restored = LF_PlayerAccountManager.GetInstance().GetFromCache("test-account-save");
		ctx.NotNull(restored, "account is restored from its records");
		if (!restored)
			return;

		ctx.Equal(LF_Faction.POLICE, restored.GetFaction(), "faction survives the round trip");
		ctx.True(restored.IsOnDuty(), "on-duty flag survives the round trip");
		ctx.Equal(2, restored.GetWantedLevel(), "wanted level survives the round trip");
		ctx.Equal(1, restored.GetCharacters().Count(), "character roster survives the round trip");
		ctx.EqualStr("John", restored.GetActiveCharacter().GetFirstName(), "active character survives the round trip");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_AccountManagerCache : LF_Test
{
	override string GetName()
	{
		return "account/manager-cache";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerAccountManager.Reset();
		LF_PlayerAccountManager manager = LF_PlayerAccountManager.GetInstance();

		LF_PlayerAccount account = LF_PlayerAccount.Create("test-uid");
		manager.AddToCache(account);

		LF_PlayerAccount cached = manager.GetFromCache("test-uid");
		ctx.NotNull(cached, "cached account is returned");
		ctx.True(cached == account, "cached account is the same instance");
		ctx.True(manager.GetFromCache("missing-uid") == null, "unknown uid returns null");

		LF_PlayerAccountManager.Reset();
	}
};