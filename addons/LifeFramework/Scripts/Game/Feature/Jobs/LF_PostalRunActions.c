//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Jobs", description: "Post office of the postal van run: registers in LF_Places (kind PostalPoint) so a run finds its offices by place, never by name. Names the van this office lends.")]
class LF_PostalPointComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class LF_PostalPointComponent : ScriptComponent
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The van a run taken here lends (a pack's livery); empty: the postal run config's van", "et")]
	protected ResourceName m_Van;

	//------------------------------------------------------------------------------------------------
	ResourceName GetVan()
	{
		return m_Van;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode())
			return;

		LF_Places.Register(LF_PostalRun.PLACE_KIND, string.Format("postal_%1", owner.GetID()), owner);
	}
}

//------------------------------------------------------------------------------------------------
//! What a postal office action does.
enum LF_EPostalStep
{
	START = 0,	// take the run and the van
	SACK = 1,	// load a sack at home, unload one at the far office
	HAND_IN = 2,	// hand the run in at home and get paid
	GIVE_UP = 3	// give the run up, no pay
}

//------------------------------------------------------------------------------------------------
//! One action class for the four steps at a post office; the server decides and toasts the outcome.
class LF_PostalAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "Step", "", ParamEnumArray.FromEnum(LF_EPostalStep))]
	protected LF_EPostalStep m_eStep;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_CivilianJob.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_CivilianJob.SYSTEM, "postal run"))
			return;

		string uid = LF_Utils.GetPlayerUid(pUserEntity);
		if (m_eStep == LF_EPostalStep.START)
		{
			if (!LF_RateLimit.AllowCharacter(pUserEntity, LF_ERequestKind.SPAWN, "postal van"))
				return;

			Toast(pUserEntity, LF_PostalRun.Start(pUserEntity, uid, pOwnerEntity), "#LF-Postal_Started", LF_PostalRun.MinMinutes().ToString());
			return;
		}

		if (m_eStep == LF_EPostalStep.SACK)
		{
			LF_EPostalResult result = LF_PostalRun.Sack(pUserEntity, uid, pOwnerEntity);
			LF_PostalRunState run = LF_PostalRun.GetRun(uid);
			string arg;
			string done = "#LF-Postal_Loaded";
			if (run)
			{
				arg = string.Format("%1/%2", run.m_iLoaded, LF_PostalRunConfig.Get().m_iSacks);
				if (run.m_iUnloaded > 0)
				{
					done = "#LF-Postal_Unloaded";
					arg = string.Format("%1/%2", run.m_iUnloaded, LF_PostalRunConfig.Get().m_iSacks);
				}
			}

			Toast(pUserEntity, result, done, arg);
			return;
		}

		if (m_eStep == LF_EPostalStep.HAND_IN)
		{
			int paid;
			LF_EPostalResult handIn = LF_PostalRun.HandIn(pUserEntity, uid, pOwnerEntity, paid);
			Toast(pUserEntity, handIn, "#LF-Postal_Paid", paid.ToString());
			return;
		}

		if (LF_PostalRun.GiveUp(uid))
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Postal_GivenUp", LF_PostalRun.TITLE_KEY, 5.0);
		else
			LF_Utils.NotifyEntity(pUserEntity, "#LF-Postal_NoRun", LF_PostalRun.TITLE_KEY, 4.0);
	}

	//------------------------------------------------------------------------------------------------
	protected void Toast(IEntity user, LF_EPostalResult result, string okKey, string arg)
	{
		if (result == LF_EPostalResult.OK)
		{
			LF_Utils.NotifyEntity(user, okKey, LF_PostalRun.TITLE_KEY, 6.0, arg);
			return;
		}

		string key = LF_PostalRun.RefusalKey(result);
		if (key.IsEmpty())
			return;

		// The hand-in refusal names the least time (minutes) so a $0 never looks like a bug.
		string minutes;
		if (result == LF_EPostalResult.TOO_FAST)
			minutes = LF_PostalRun.MinMinutes().ToString();

		LF_Utils.NotifyEntity(user, key, LF_PostalRun.TITLE_KEY, 8.0, minutes);
	}
}
