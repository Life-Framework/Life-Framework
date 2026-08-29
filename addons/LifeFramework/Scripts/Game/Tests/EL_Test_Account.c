// red-proof: change an expected value (e.g. wanted clamp 5 -> 6) or break the
// clamp in EL_PlayerAccount.SetWantedLevel, then run the fast tier.

class EL_Test_AccountWantedClamp : EL_Test
{
	override string GetName()
	{
		return "account/wanted-clamp";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_PlayerAccount account = EL_PlayerAccount.Create("test-account");

		ctx.Equal(0, account.GetWantedLevel(), "fresh account starts at wanted level 0");
		ctx.Equal(EL_Faction.CIVILIAN, account.GetFaction(), "fresh account defaults to civilian");

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
class EL_Test_AccountCharacterRoster : EL_Test
{
	override string GetName()
	{
		return "account/character-roster";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_PlayerAccount account = EL_PlayerAccount.Create("test-account-characters");

		EL_PlayerCharacter a = EL_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "John", "Doe", 30);
		EL_PlayerCharacter b = EL_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "Jane", "Roe", 28);

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
//! Save-data round trip through the public DTO.
class EL_Test_AccountSaveRoundtrip : EL_Test
{
	override string GetName()
	{
		return "account/save-roundtrip";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_PlayerAccount account = EL_PlayerAccount.Create("test-account-save");
		account.SetFaction(EL_Faction.POLICE);
		account.SetOnDuty(true);
		account.SetWantedLevel(2);
		account.AddCharacter(EL_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "John", "Doe", 30), true);

		EL_PlayerAccountSaveData data = new EL_PlayerAccountSaveData();
		data.ReadFrom(account);

		EL_PlayerAccount restored = EL_PlayerAccount.Create("other-id");
		data.ApplyTo(restored);

		ctx.Equal(EL_Faction.POLICE, restored.GetFaction(), "faction survives the round trip");
		ctx.True(restored.IsOnDuty(), "on-duty flag survives the round trip");
		ctx.Equal(2, restored.GetWantedLevel(), "wanted level survives the round trip");
		ctx.Equal(1, restored.GetCharacters().Count(), "character roster survives the round trip");
		ctx.EqualStr("John", restored.GetActiveCharacter().GetFirstName(), "active character survives the round trip");

		EL_PlayerAccountSaveData same = new EL_PlayerAccountSaveData();
		same.ReadFrom(account);
		EL_PlayerAccountSaveData clone = new EL_PlayerAccountSaveData();
		clone.ReadFrom(account);
		ctx.True(same.Equals(clone), "equal accounts compare equal in save data");
	}
};

//------------------------------------------------------------------------------------------------
class EL_Test_AccountManagerCache : EL_Test
{
	override string GetName()
	{
		return "account/manager-cache";
	}

	override void Run(EL_TestContext ctx)
	{
		EL_PlayerAccountManager.Reset();
		EL_PlayerAccountManager manager = EL_PlayerAccountManager.GetInstance();

		EL_PlayerAccount account = EL_PlayerAccount.Create("test-uid");
		manager.AddToCache(account);

		EL_PlayerAccount cached = manager.GetFromCache("test-uid");
		ctx.NotNull(cached, "cached account is returned");
		ctx.True(cached == account, "cached account is the same instance");
		ctx.True(manager.GetFromCache("missing-uid") == null, "unknown uid returns null");

		EL_PlayerAccountManager.Reset();
	}
};