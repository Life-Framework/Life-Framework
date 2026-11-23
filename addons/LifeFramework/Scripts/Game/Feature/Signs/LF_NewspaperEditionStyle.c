// LF_NewspaperEditionStyle.c: the front page reads as newsprint, not a UI panel: off-white paper,
// near-black ink, black rules, a large masthead and no card rim colour. No serif face ships with the
// base game, so the masthead uses the bold condensed face at a larger size until a period serif font
// is added.
class LF_NewspaperEditionStyle
{
	static const int PAPER_R = 238; // lf-fixed: newsprint, off-white with a grey-yellow cast
	static const int PAPER_G = 233;
	static const int PAPER_B = 219;
	static const int INK_R = 28; // lf-fixed: press ink, not pure black
	static const int INK_G = 26;
	static const int INK_B = 24;

	//------------------------------------------------------------------------------------------------
	static LF_UISkin Newsprint()
	{
		LF_UISkin skin = LF_UISkin.Notice(null, INK_R, INK_G, INK_B);
		skin.m_sName = "newsprint";
		skin.m_fWidth = 680;
		skin.m_fTitleScale = 1.9;
		skin.m_PaperTexture = string.Empty;
		skin.m_Paper = Color.FromSRGBA(PAPER_R, PAPER_G, PAPER_B, 255);
		skin.m_Metal = Color.FromSRGBA(PAPER_R - 22, PAPER_G - 22, PAPER_B - 24, 255);
		skin.m_Ink = Color.FromSRGBA(INK_R, INK_G, INK_B, 255);
		skin.m_FieldInk = skin.m_Ink;
		skin.m_Rule = skin.m_Ink;
		skin.m_Accent = skin.m_Ink;
		skin.m_ButtonText = skin.m_Ink;
		skin.m_ButtonFocus = Color.FromSRGBA(110, 104, 96, 255);
		skin.m_bRecolorButtons = true;
		return skin;
	}
}
