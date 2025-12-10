// Enum para tipos de trabajos de recolección
enum EHarvestJobType
{
	APPLES,    // Manzanas
	TOMATOES,  // Tomates
	PLUMS,     // Ciruelas
	BERRIES    // Bayas
}

// Enum para estados de frutas
enum EFruitState
{
	CORRECT,   // Fruta del tipo correcto
	WRONG,     // Fruta de otro tipo
	ROTTEN,    // Fruta podrida
	POWERUP    // Power-up
}

// Clase para representar una fruta cayendo
class EL_FallingFruit
{
	Widget m_wFruitWidget;
	ImageWidget m_wFruitImage;
	EFruitState m_eFruitState;
	EHarvestJobType m_eFruitType;
	float m_fPositionX;
	float m_fPositionY;
	float m_fFallSpeed;
	
	void EL_FallingFruit(Widget parent, EFruitState state, EHarvestJobType type, float posX, float fallSpeed)
	{
		m_eFruitState = state;
		m_eFruitType = type;
		m_fPositionX = posX;
		m_fPositionY = 0;
		m_fFallSpeed = fallSpeed;
		
		// Crear widget de fruta
		m_wFruitWidget = GetGame().GetWorkspace().CreateWidgets("{EC56C6F70AE2FEDE}UI/Layouts/Minigames/EL_FruitWidget.layout", parent);
		if (!m_wFruitWidget)
			return;
		
		// Establecer tamaño fijo antes de posicionar (necesario para widgets dinámicos)
		FrameSlot.SetSize(m_wFruitWidget, 80, 80);
			
		m_wFruitImage = ImageWidget.Cast(m_wFruitWidget.FindAnyWidget("FruitImage"));
		if (!m_wFruitImage)
			return;
		
		// Configurar imagen según tipo y estado
		SetFruitAppearance();
		
		// Posicionar fruta
		FrameSlot.SetPos(m_wFruitWidget, m_fPositionX, m_fPositionY);
	}
	
	void SetFruitAppearance()
	{
		if (!m_wFruitImage)
			return;
			
		string texturePath;
		
		// Determine texture based on fruit state and type
		if (m_eFruitState == EFruitState.POWERUP)
		{
			texturePath = "{99F806159668FFFD}UI/Images/power.edds";
		}
		else if (m_eFruitState == EFruitState.ROTTEN)
		{
			// Use rotten version based on fruit type
			switch (m_eFruitType)
			{
				case EHarvestJobType.APPLES:
					texturePath = "{BF76F90A70D669C3}UI/Images/manzanapocha.edds";
					break;
				case EHarvestJobType.TOMATOES:
					texturePath = "{8C090D74DD7233AD}UI/Images/tomatepocho.edds";
					break;
				case EHarvestJobType.PLUMS:
					texturePath = "{560B6ADE10EDD72F}UI/Images/ciruelapocha.edds";
					break;
				case EHarvestJobType.BERRIES:
					texturePath = "{09C782469B954D83}UI/Images/bayaspochas.edds";
					break;
			}
		}
		else
		{
			// Use normal fruit texture
			switch (m_eFruitType)
			{
				case EHarvestJobType.APPLES:
					texturePath = "{227905D6ADEF4376}UI/Images/manzana.edds";
					break;
				case EHarvestJobType.TOMATOES:
					texturePath = "{7E2B1D3AE43D7D5E}UI/Images/Tomate.edds";
					break;
				case EHarvestJobType.PLUMS:
					texturePath = "{C237B4CC00CD3066}UI/Images/ciruela.edds";
					break;
				case EHarvestJobType.BERRIES:
					texturePath = "{56D7EE6D2CA53041}UI/Images/bayas.edds";
					break;
			}
		}
		
		// Set the texture
		if (!texturePath.IsEmpty())
		{
			m_wFruitImage.LoadImageTexture(0, texturePath);
			m_wFruitImage.SetColor(Color.FromRGBA(255, 255, 255, 255)); // White to show texture properly
		}
	}
	
	void Update(float timeSlice)
	{
		m_fPositionY = m_fPositionY + (m_fFallSpeed * timeSlice);
		FrameSlot.SetPos(m_wFruitWidget, m_fPositionX, m_fPositionY);
	}
	
	bool IsOutOfBounds(float maxHeight)
	{
		return m_fPositionY > maxHeight;
	}
	
	void Destroy()
	{
		if (m_wFruitWidget)
			m_wFruitWidget.RemoveFromHierarchy();
	}
	
	vector GetPosition()
	{
		float sizeX, sizeY;
		m_wFruitWidget.GetScreenSize(sizeX, sizeY);
		return Vector(m_fPositionX, m_fPositionY, 0);
	}
	
	vector GetSize()
	{
		float sizeX, sizeY;
		m_wFruitWidget.GetScreenSize(sizeX, sizeY);
		return Vector(sizeX, sizeY, 0);
	}
}
