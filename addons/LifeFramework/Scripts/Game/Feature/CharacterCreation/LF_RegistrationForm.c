// LF_RegistrationForm.c — civil registry form R-89, filled in once on first arrival.
// Client-only, on LF_UIScreen: three blanks (given name, surname, age) and a Sign button.
// The form cannot be put away: MenuBack or a lost sheet brings it back until it is signed,
// because a player without a character has nothing else to do. The checks here only write the
// error line; the server re-checks (EL_CharacterCreationRules.RefuseCharacter) and sends the form
// back with its reason when it refuses.

class LF_RegistrationForm
{
	static const string PART_FIRST = "RegFirstName";
	static const string PART_LAST = "RegLastName";
	static const string PART_AGE = "RegAge";
	static const string PART_ERROR = "RegError";
	static const string PART_SIGN = "SignButton";

	protected static ref LF_RegistrationForm s_Form;

	protected ref LF_UIScreen m_Screen;
	protected SCR_PlayerController m_Controller;
	protected bool m_bSigned;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts(string refusal)
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "RegTitle", "#LF-Reg_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "RegIntro", "#LF-Reg_Intro"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, PART_FIRST, "#LF-Reg_FirstName"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, PART_LAST, "#LF-Reg_LastName"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, PART_AGE, "#LF-Reg_Age"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_ERROR, refusal));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_SIGN, "#LF-Reg_Sign"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	//! Opens (or re-opens) the form. A form already on screen is replaced so the refusal shows.
	static LF_UIScreen Open(SCR_PlayerController controller, string refusal)
	{
		Hide();
		if (!controller)
		{
			EL_Debug.Error("CharacterCreation", "registration form refused: no player controller");
			return null;
		}

		LF_RegistrationForm form = new LF_RegistrationForm();
		form.m_Controller = controller;
		form.m_Screen = LF_UIScreen.Open(BuildParts(refusal));
		if (!form.m_Screen)
			return null;

		form.m_Screen.GetOnButton().Insert(form.OnButton);
		form.m_Screen.GetOnClosed().Insert(form.OnClosed);
		s_Form = form;
		EL_Debug.Info("CharacterCreation", "registration form open");
		return form.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Form)
			return null;

		return s_Form.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the form down without re-opening it.
	static void Hide()
	{
		if (!s_Form)
			return;

		LF_RegistrationForm form = s_Form;
		s_Form = null;
		form.m_bSigned = true;
		if (form.m_Screen)
			form.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (name != PART_SIGN || m_bSigned || !m_Screen)
			return;

		string firstName = m_Screen.GetFieldText(PART_FIRST).Trim();
		string lastName = m_Screen.GetFieldText(PART_LAST).Trim();
		string ageText = m_Screen.GetFieldText(PART_AGE);

		string refusal = LF_RegistrationRules.RefuseForm(firstName, lastName, ageText);
		if (!refusal.IsEmpty())
		{
			EL_Debug.Info("CharacterCreation", "registration form not signed: " + refusal);
			m_Screen.SetLine(PART_ERROR, refusal);
			return;
		}

		if (!m_Controller)
		{
			EL_Debug.Error("CharacterCreation", "registration form not sent: no player controller");
			return;
		}

		int age = LF_RegistrationRules.ParseAge(ageText);
		EL_Debug.Info("CharacterCreation", "registration form signed, age " + age);
		SCR_PlayerController controller = m_Controller;
		Hide();
		controller.EL_AskCreateCharacter(firstName, lastName, age);
	}

	//------------------------------------------------------------------------------------------------
	//! MenuBack closed an unsigned form: put it back on the next tick (not inside the close).
	protected void OnClosed()
	{
		if (m_bSigned || s_Form != this)
			return;

		EL_Debug.Info("CharacterCreation", "registration form put away unsigned, opening it again");
		GetGame().GetCallqueue().CallLater(Reopen, 0, false, m_Controller);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Reopen(SCR_PlayerController controller)
	{
		Open(controller, "#LF-Reg_MustSign");
	}
}
