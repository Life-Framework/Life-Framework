//------------------------------------------------------------------------------------------------
//! LF_SiteStrongboxComponent.c: the strongbox at a contested site (#2684, gang war #1022). The
//! component names the hideout whose rent bank (LF_SiteRent) the box holds. A member of the
//! organisation that leases the hideout, whose rank has WITHDRAW, takes the bank as MoneyStack in
//! hand through LF_SiteStrongboxAction. Money stays physical: it is never paid into an account.
[ComponentEditorProps(category: "LifeFramework/Feature/Sites", description: "Strongbox of a contested site: the holder takes the banked rent here.")]
class LF_SiteStrongboxComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_SiteStrongboxComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, desc: "Hideout id of the contested site this box banks rent for (the id of its LF_ContestedSiteComponent).")]
	protected string m_sHideoutId;

	//------------------------------------------------------------------------------------------------
	string GetHideoutId()
	{
		return m_sHideoutId;
	}

	//------------------------------------------------------------------------------------------------
	//! A box spawned by script (a test, a site builder) names its hideout here; one placed in a
	//! world carries it as an attribute.
	void BindHideout(string hideoutId)
	{
		m_sHideoutId = hideoutId;
	}
}

//------------------------------------------------------------------------------------------------
enum LF_ESiteStrongboxResult
{
	//! The bank went to the player's hands.
	TAKEN,
	//! The player's organisation does not hold the site (or has none, or the box names no site).
	NOT_HOLDER,
	//! A member of the holder whose rank lacks WITHDRAW.
	NO_RIGHT,
	//! The bank is empty.
	EMPTY,
	//! The cash could not be put in hand; the bank is unchanged.
	NO_ROOM,
	//! The player stands too far from the box.
	OUT_OF_REACH
}

//------------------------------------------------------------------------------------------------
class LF_SiteStrongbox
{
	static const string FEATURE = "Sites";
	//! The reason on the MoneyStack line.
	static const string REASON = "site-rent";
	//! How far from the box a take is accepted on the server, in metres.
	static const float REACH = 4.0;	// lf-fixed: a sanity bound on the action's own 2 m radius

	//------------------------------------------------------------------------------------------------
	//! Server: the action's entry point. Finds the hideout from the box, the character from the player
	//! and takes the bank.
	//! \param amount Out: the cash taken, 0 unless the result is TAKEN.
	static LF_ESiteStrongboxResult TakeFromBox(IEntity box, IEntity user, out int amount)
	{
		amount = 0;
		if (!box || !user)
			return LF_ESiteStrongboxResult.NOT_HOLDER;

		LF_SiteStrongboxComponent component = LF_Component<LF_SiteStrongboxComponent>.Find(box);
		if (!component || component.GetHideoutId().IsEmpty())
		{
			LF_Debug.Error(FEATURE, "strongbox take refused: the box names no hideout");
			return LF_ESiteStrongboxResult.NOT_HOLDER;
		}

		if (vector.Distance(user.GetOrigin(), box.GetOrigin()) > REACH)
		{
			LF_Debug.Warn(FEATURE, string.Format("strongbox take refused: %1 is out of reach of %2", LF_Utils.GetPlayerUid(user), component.GetHideoutId()));
			return LF_ESiteStrongboxResult.OUT_OF_REACH;
		}

		int banked = LF_SiteRent.BankedOf(component.GetHideoutId());
		LF_ESiteStrongboxResult result = Take(user, LF_OrganizationPlayer.CharacterId(user), component.GetHideoutId());
		if (result == LF_ESiteStrongboxResult.TAKEN)
			amount = banked;

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a character takes a site's banked rent as cash. The holder is the organisation that
	//! leases the hideout; the character must be its member with a WITHDRAW rank. The cash is put in
	//! hand first and the bank emptied after, so a failed payout leaves the bank as it was.
	static LF_ESiteStrongboxResult Take(IEntity user, string characterId, string hideoutId)
	{
		if (!Replication.IsServer())
			return LF_ESiteStrongboxResult.NOT_HOLDER;

		if (!user || characterId.IsEmpty() || hideoutId.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "strongbox take refused: no player, character or hideout");
			return LF_ESiteStrongboxResult.NOT_HOLDER;
		}

		string holderId = LF_Hideout.GetOwner(hideoutId);
		LF_Organization org = LF_OrganizationManager.GetByCharacter(characterId);
		if (holderId.IsEmpty() || !org || org.GetId() != holderId)
		{
			LF_Debug.Warn(FEATURE, string.Format("strongbox take refused: %1 does not hold %2", characterId, hideoutId));
			return LF_ESiteStrongboxResult.NOT_HOLDER;
		}

		if (!org.Allows(characterId, LF_EOrganizationPermission.WITHDRAW))
		{
			LF_Debug.Warn(FEATURE, string.Format("strongbox take refused: %1 lacks the withdraw rank at %2", characterId, hideoutId));
			return LF_ESiteStrongboxResult.NO_RIGHT;
		}

		int banked = LF_SiteRent.BankedOf(hideoutId);
		if (banked <= 0)
		{
			LF_Debug.Info(FEATURE, string.Format("strongbox take: %1 has nothing banked", hideoutId));
			return LF_ESiteStrongboxResult.EMPTY;
		}

		int given = LF_MoneyUtils.AddCash(user, banked, REASON);
		if (given != banked)
		{
			if (given > 0)
				LF_MoneyUtils.RemoveCash(user, given, REASON + "-rollback");

			LF_Debug.Error(FEATURE, string.Format("strongbox take restored: paid %1 of %2 at %3", given, banked, hideoutId));
			return LF_ESiteStrongboxResult.NO_ROOM;
		}

		LF_SiteRent.Take(hideoutId, holderId);
		LF_Debug.Info(FEATURE, string.Format("strongbox take: %1 of %2 took %3 from %4", characterId, holderId, banked, hideoutId));
		return LF_ESiteStrongboxResult.TAKEN;
	}

	//------------------------------------------------------------------------------------------------
	//! The toast key of a result.
	static string ResultKey(LF_ESiteStrongboxResult result)
	{
		switch (result)
		{
			case LF_ESiteStrongboxResult.TAKEN:
				return "#LF-SiteStrongbox_Taken";
			case LF_ESiteStrongboxResult.NO_RIGHT:
				return "#LF-SiteStrongbox_NoRight";
			case LF_ESiteStrongboxResult.EMPTY:
				return "#LF-SiteStrongbox_Empty";
			case LF_ESiteStrongboxResult.NO_ROOM:
				return "#LF-SiteStrongbox_NoRoom";
		}

		return "#LF-SiteStrongbox_NotHolder";
	}
}
