//------------------------------------------------------------------------------------------------
//! The start sign's action (#1923): pay the entry fee into the pot. The course comes from the
//! sign's own attributes; the server takes the player's uid from the user entity, never from the
//! client, and LF_RaceRunner decides everything.
class LF_RaceEntryAction : ScriptedUserAction
{
	[Attribute("", UIWidgets.ResourceNamePicker, "The race courses file", "conf")]
	protected ResourceName m_Courses;

	[Attribute("", UIWidgets.EditBox, "Course id this sign starts")]
	protected string m_sCourseId;

	protected ref LF_RaceCourseRecord m_Course;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(LF_RaceRunner.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (LF_Systems.Refuses(LF_RaceRunner.SYSTEM, "LF_RaceEntryAction"))
			return;

		LF_RaceCourseRecord course = ResolveCourse();
		string uid = LF_Utils.GetPlayerUID(pUserEntity);
		LF_ERaceJoin outcome = LF_RaceRunner.Join(course, uid, pUserEntity);

		string arg;
		if (course)
			arg = LF_MoneyFormat.FormatMoney(course.m_iEntryFee);

		LF_ENotificationType type = LF_ENotificationType.WARNING;
		if (outcome == LF_ERaceJoin.JOINED)
		{
			type = LF_ENotificationType.INFO;
			LF_PlayerInteraction.PlayHandoverSound(pUserEntity, LF_PlayerInteraction.SOUND_HANDOVER);
		}

		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(pUserEntity), "#LF-Race_Title", AnswerKey(outcome), 6.0, type, arg);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RaceCourseRecord ResolveCourse()
	{
		if (m_Course)
			return m_Course;

		if (m_Courses.IsEmpty() || m_sCourseId.IsEmpty())
		{
			LF_Debug.Error(LF_RaceRunner.FEATURE, "race sign has no courses file or course id");
			return null;
		}

		Resource holder = BaseContainerTools.LoadContainer(m_Courses);
		if (!holder || !holder.IsValid())
		{
			LF_Debug.Error(LF_RaceRunner.FEATURE, "race sign courses file does not load: " + m_Courses);
			return null;
		}

		LF_RaceCoursesConfig config = LF_RaceCoursesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(holder.GetResource().ToBaseContainer()));
		if (!config || !config.m_aCourses)
		{
			LF_Debug.Error(LF_RaceRunner.FEATURE, "race sign courses file is not an LF_RaceCoursesConfig: " + m_Courses);
			return null;
		}

		foreach (LF_RaceCourseRecord record : config.m_aCourses)
		{
			if (record && record.m_sId == m_sCourseId)
			{
				m_Course = record;
				return m_Course;
			}
		}

		LF_Debug.Error(LF_RaceRunner.FEATURE, string.Format("race sign course '%1' is not in %2", m_sCourseId, m_Courses));
		return null;
	}

	//------------------------------------------------------------------------------------------------
	static string AnswerKey(LF_ERaceJoin outcome)
	{
		if (outcome == LF_ERaceJoin.JOINED)
			return "#LF-Race_Joined";
		if (outcome == LF_ERaceJoin.ALREADY_IN)
			return "#LF-Race_AlreadyIn";
		if (outcome == LF_ERaceJoin.NOT_ENOUGH_CASH)
			return "#LF-Race_NotEnoughCash";
		if (outcome == LF_ERaceJoin.CLOSED)
			return "#LF-Race_Running";
		if (outcome == LF_ERaceJoin.FULL)
			return "#LF-Race_Full";

		return "#LF-Race_Unavailable";
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Race_Action";
		return true;
	}
}
