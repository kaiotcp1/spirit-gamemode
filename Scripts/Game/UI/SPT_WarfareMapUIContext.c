//! Contexto de UI que exibe o mapa Warfare com icones coloridos
//! para cada ponto estrategico. Usa CanvasWidget para desenho.
class SPT_WarfareMapUIContext : SPT_UIContext
{
	protected CanvasWidget m_Canvas;
	protected float m_fMinX, m_fMaxX, m_fMinZ, m_fMaxZ;
	protected int m_iScreenW, m_iScreenH;

	override void ShowLayout()
	{
		super.ShowLayout();
		if (m_wRoot)
		{
			m_Canvas = CanvasWidget.Cast(m_wRoot.FindAnyWidget("WarfareCanvas"));
			if (m_Canvas)
			{
				m_Canvas.GetScreenSize(m_iScreenW, m_iScreenH);
			}
		}
		CalcWorldBounds();
	}

	override void OnActiveFrame(float timeSlice)
	{
		super.OnActiveFrame(timeSlice);
		DrawWarfareIcons();
	}

	override void RegisterInputs()
	{
		super.RegisterInputs();
		// ESC para fechar
		if (m_InputManager)
		{
			m_InputManager.ActivateContext("SPT_WarfareMapContext");
		}
	}

	override void UnregisterInputs()
	{
		super.UnregisterInputs();
	}

	//! Calcula os limites do mundo para normalizar coordenadas.
	protected void CalcWorldBounds()
	{
		m_fMinX = float.MAX;
		m_fMaxX = float.MIN;
		m_fMinZ = float.MAX;
		m_fMaxZ = float.MIN;

		SPT_WarfareGameModeComponent warfare = SPT_WarfareGameModeComponent.GetInstance();
		if (!warfare)
			return;

		array<string> pointIds = {};
		warfare.GetClientPointIds(pointIds);

		foreach (string pointId : pointIds)
		{
			vector pos;
			if (!warfare.GetClientPointPosition(pointId, pos))
				continue;

			float x = pos[0];
			float z = pos[2];

			if (x < m_fMinX) m_fMinX = x;
			if (x > m_fMaxX) m_fMaxX = x;
			if (z < m_fMinZ) m_fMinZ = z;
			if (z > m_fMaxZ) m_fMaxZ = z;
		}

		// Adiciona margem de 10%
		float dx = (m_fMaxX - m_fMinX) * 0.1;
		float dz = (m_fMaxZ - m_fMinZ) * 0.1;
		if (dx < 50) dx = 50;
		if (dz < 50) dz = 50;
		m_fMinX -= dx;
		m_fMaxX += dx;
		m_fMinZ -= dz;
		m_fMaxZ += dz;
	}

	//! Converte coordenada do mundo para coordenada da tela.
	protected void WorldToScreen(float wx, float wz, out int sx, out int sy)
	{
		float margin = 60;
		float usableW = m_iScreenW - margin * 2;
		float usableH = m_iScreenH - margin * 2 - 50;

		float nx, nz;
		if (m_fMaxX - m_fMinX > 0)
			nx = (wx - m_fMinX) / (m_fMaxX - m_fMinX);
		else
			nx = 0.5;

		if (m_fMaxZ - m_fMinZ > 0)
			nz = (wz - m_fMinZ) / (m_fMaxZ - m_fMinZ);
		else
			nz = 0.5;

		sx = margin + nx * usableW;
		sy = margin + 50 + (1.0 - nz) * usableH;
	}

	//! Desenha os icones no canvas.
	protected void DrawWarfareIcons()
	{
		if (!m_Canvas)
			return;

		SPT_WarfareGameModeComponent warfare = SPT_WarfareGameModeComponent.GetInstance();
		if (!warfare)
			return;

		ref array<ref CanvasWidgetCommand> cmds = new array<ref CanvasWidgetCommand>();

		array<string> pointIds = {};
		warfare.GetClientPointIds(pointIds);

		foreach (string pointId : pointIds)
		{
			vector pos;
			if (!warfare.GetClientPointPosition(pointId, pos))
				continue;

			SPT_EWarfarePointState state = warfare.GetClientPointState(pointId);
			int fillColor = GetStateColor(state);
			int borderColor = ARGB(255, 255, 255, 255);

			int sx, sy;
			WorldToScreen(pos[0], pos[2], sx, sy);

			// Borda branca
			PolygonDrawCommand bc = new PolygonDrawCommand();
			bc.m_iColor = borderColor;
			bc.m_Vertices = MakeCircle(sx, sy, 14);
			cmds.Insert(bc);

			// Preenchimento
			PolygonDrawCommand fc = new PolygonDrawCommand();
			fc.m_iColor = fillColor;
			fc.m_Vertices = MakeCircle(sx, sy, 12);
			cmds.Insert(fc);

			// Nome do ponto
			string name = warfare.GetClientPointName(pointId);
			if (!name.IsEmpty())
			{
				TextDrawCommand tc = new TextDrawCommand();
				tc.m_sText = name;
				tc.m_vPosition = Vector(sx + 18, sy - 6, 0);
				tc.m_iColor = ARGB(255, 255, 255, 255);
				cmds.Insert(tc);
			}
		}

		if (!cmds.IsEmpty())
			m_Canvas.SetDrawCommands(cmds);
	}

	protected array<float> MakeCircle(int cx, int cy, int r)
	{
		array<float> v = {};
		for (int i = 0; i < 32; i++)
		{
			float a = i * 2.0 * Math.PI / 32.0;
			v.Insert(cx + Math.Cos(a) * r);
			v.Insert(cy + Math.Sin(a) * r);
		}
		return v;
	}

	static int GetStateColor(SPT_EWarfarePointState state)
	{
		if (state == SPT_EWarfarePointState.LOCKED)            return ARGB(255, 150, 0, 0);
		if (state == SPT_EWarfarePointState.FRONTLINE)         return ARGB(255, 220, 30, 30);
		if (state == SPT_EWarfarePointState.UNDER_ATTACK)      return ARGB(255, 255, 140, 0);
		if (state == SPT_EWarfarePointState.CLEARED_WAITING)   return ARGB(255, 255, 255, 0);
		if (state == SPT_EWarfarePointState.CAPTURED_DEFENDING) return ARGB(255, 70, 130, 255);
		if (state == SPT_EWarfarePointState.CAPTURED)           return ARGB(255, 0, 150, 255);
		return ARGB(255, 128, 128, 128);
	}
}
