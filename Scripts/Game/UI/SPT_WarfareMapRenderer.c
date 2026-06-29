//! Renderer cliente dos pontos Warfare sobre o mapa tatico nativo.
//! O ciclo de frame e controlado pelo SPT_WarfareGameModeComponent.
class SPT_WarfareMapRenderer : Managed
{
	protected SPT_WarfareGameModeComponent m_Warfare;
	protected SCR_MapEntity m_MapEntity;
	protected Widget m_wRoot;
	protected CanvasWidget m_wCanvas;
	protected Widget m_wTooltip;
	protected TextWidget m_wTooltipTitle;
	protected TextWidget m_wTooltipBody;
	//! CanvasWidget nao assume ownership; este array precisa sobreviver ao frame.
	protected ref array<ref CanvasWidgetCommand> m_aDrawCommands;
	protected bool m_bSubscribed;
	protected bool m_bOpen;
	protected bool m_bLoggedFirstDraw;
	protected float m_fPulseTime;

	void Init(SPT_WarfareGameModeComponent warfare)
	{
		if (m_bSubscribed)
			return;

		m_Warfare = warfare;
		SCR_MapEntity.GetOnMapOpen().Insert(OnMapOpen);
		SCR_MapEntity.GetOnMapClose().Insert(OnMapClose);
		m_bSubscribed = true;
		Print("[SPT_WarfareMap] Renderer inicializado.");
	}

	void Shutdown()
	{
		CloseOverlay();

		if (m_bSubscribed)
		{
			SCR_MapEntity.GetOnMapOpen().Remove(OnMapOpen);
			SCR_MapEntity.GetOnMapClose().Remove(OnMapClose);
			m_bSubscribed = false;
		}

		m_Warfare = null;
	}

	protected void OnMapOpen(MapConfiguration config)
	{
		CloseOverlay();

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_MapEntity = SCR_MapEntity.GetMapInstance();
		if (!m_MapEntity)
			return;

		Widget parent = config.RootWidgetRef;
		if (!parent)
			parent = m_MapEntity.GetMapMenuRoot();
		if (!parent)
			return;

		m_wRoot = workspace.CreateWidgets(
			"{A71C5E0B94D238F6}UI/Layouts/WarfareMap.layout",
			parent
		);
		if (!m_wRoot)
		{
			Print("[SPT_WarfareMap] ERRO: overlay nao foi criado.", LogLevel.ERROR);
			return;
		}

		m_wRoot.SetFlags(m_wRoot.GetFlags() | WidgetFlags.IGNORE_CURSOR);
		m_wCanvas = CanvasWidget.Cast(m_wRoot.FindAnyWidget("WarfareCanvas"));
		m_wTooltip = m_wRoot.FindAnyWidget("WarfareTooltip");
		m_wTooltipTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("TooltipTitle"));
		m_wTooltipBody = TextWidget.Cast(m_wRoot.FindAnyWidget("TooltipBody"));

		if (!m_wCanvas)
		{
			Print("[SPT_WarfareMap] ERRO: widget WarfareCanvas nao encontrado.", LogLevel.ERROR);
			CloseOverlay();
			return;
		}

		if (m_wTooltip)
			m_wTooltip.SetVisible(false);

		m_fPulseTime = 0.0;
		m_bLoggedFirstDraw = false;
		m_aDrawCommands = new array<ref CanvasWidgetCommand>();
		m_bOpen = true;
		m_Warfare.SetMapRenderingActive(true);
		Print("[SPT_WarfareMap] Mapa aberto; overlay ativo.");
	}

	protected void OnMapClose(MapConfiguration config)
	{
		CloseOverlay();
	}

	protected void CloseOverlay()
	{
		m_bOpen = false;

		if (m_Warfare)
			m_Warfare.SetMapRenderingActive(false);

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_aDrawCommands = null;
		m_wRoot = null;
		m_wCanvas = null;
		m_wTooltip = null;
		m_wTooltipTitle = null;
		m_wTooltipBody = null;
		m_MapEntity = null;
	}

	void Update(float timeSlice)
	{
		if (!m_bOpen || !m_Warfare || !m_wCanvas || !m_MapEntity)
			return;

		if (!m_MapEntity.IsOpen())
		{
			CloseOverlay();
			return;
		}

		m_fPulseTime += timeSlice;
		DrawPoints();
	}

	protected void DrawPoints()
	{
		ref array<ref CanvasWidgetCommand> commands = new array<ref CanvasWidgetCommand>();
		array<string> pointIds = {};
		m_Warfare.GetClientPointIds(pointIds);
		int renderedPointCount;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			m_aDrawCommands = commands;
			m_wCanvas.SetDrawCommands(m_aDrawCommands);
			HideTooltip();
			return;
		}

		float screenWidth = workspace.DPIUnscale(workspace.GetWidth());
		float screenHeight = workspace.DPIUnscale(workspace.GetHeight());

		int mouseNativeX;
		int mouseNativeY;
		WidgetManager.GetMousePos(mouseNativeX, mouseNativeY);
		float mouseX = workspace.DPIUnscale(mouseNativeX);
		float mouseY = workspace.DPIUnscale(mouseNativeY);

		string hoveredPointId;
		float hoveredDistanceSq = 999999.0;
		int hoveredX;
		int hoveredY;

		foreach (string pointId : pointIds)
		{
			string name;
			vector position;
			SPT_EWarfarePointType pointType;
			SPT_EWarfarePointState state;
			bool isHQ;
			int manpower;
			int waveIndex;

			if (!m_Warfare.GetClientPointMapData(pointId, name, position, pointType,
				state, isHQ, manpower, waveIndex))
				continue;

			int screenX;
			int screenY;
			m_MapEntity.WorldToScreen(position[0], position[2], screenX, screenY, true);

			if (screenX < -32 || screenY < -32 || screenX > screenWidth + 32 || screenY > screenHeight + 32)
				continue;

			float radius = 12.0;
			if (state == SPT_EWarfarePointState.CAPTURED_DEFENDING)
				radius += Math.Sin(m_fPulseTime * 4.0) * 2.0;

			if (isHQ)
				InsertShape(commands, pointType, screenX, screenY, radius + 5.0, ARGB(240, 255, 255, 255));

			InsertShape(commands, pointType, screenX, screenY, radius + 2.0, ARGB(245, 15, 15, 15));
			InsertShape(commands, pointType, screenX, screenY, radius, GetStateColor(state));
			renderedPointCount++;

			float deltaX = mouseX - screenX;
			float deltaY = mouseY - screenY;
			float distanceSq = deltaX * deltaX + deltaY * deltaY;
			if (distanceSq <= 324.0 && distanceSq < hoveredDistanceSq)
			{
				hoveredDistanceSq = distanceSq;
				hoveredPointId = pointId;
				hoveredX = screenX;
				hoveredY = screenY;
			}
		}

		// Sempre atualiza o canvas, inclusive vazio, para remover marcadores antigos.
		m_aDrawCommands = commands;
		m_wCanvas.SetDrawCommands(m_aDrawCommands);

		if (!m_bLoggedFirstDraw)
		{
			Print(string.Format("[SPT_WarfareMap] Primeiro desenho | pontosCliente=%1 | pontosVisiveis=%2 | comandos=%3",
				pointIds.Count(), renderedPointCount, m_aDrawCommands.Count()));
			m_bLoggedFirstDraw = true;
		}

		if (!hoveredPointId.IsEmpty())
			ShowTooltip(hoveredPointId, hoveredX, hoveredY, screenWidth, screenHeight);
		else
			HideTooltip();
	}

	protected void InsertShape(notnull array<ref CanvasWidgetCommand> commands,
		SPT_EWarfarePointType pointType, float centerX, float centerY, float radius, int color)
	{
		PolygonDrawCommand command = new PolygonDrawCommand();
		command.m_iColor = color;
		command.m_Vertices = MakeTypeShape(pointType, centerX, centerY, radius);
		commands.Insert(command);
	}

	protected array<float> MakeTypeShape(SPT_EWarfarePointType pointType,
		float centerX, float centerY, float radius)
	{
		if (pointType == SPT_EWarfarePointType.CITY)
			return MakeRegularPolygon(centerX, centerY, radius, 24, 0.0);
		if (pointType == SPT_EWarfarePointType.VILLAGE)
			return MakeRegularPolygon(centerX, centerY, radius, 4, Math.PI * 0.25);
		if (pointType == SPT_EWarfarePointType.MILITARY_BASE)
			return MakeRegularPolygon(centerX, centerY, radius, 4, 0.0);
		if (pointType == SPT_EWarfarePointType.AIRPORT)
			return MakeStar(centerX, centerY, radius, 4);
		if (pointType == SPT_EWarfarePointType.RUIN)
			return MakeRegularPolygon(centerX, centerY, radius, 8, Math.PI * 0.125);
		if (pointType == SPT_EWarfarePointType.PORT)
			return MakeRegularPolygon(centerX, centerY, radius, 5, Math.PI * 0.5);
		if (pointType == SPT_EWarfarePointType.FOREST)
			return MakeRegularPolygon(centerX, centerY, radius, 6, 0.0);
		if (pointType == SPT_EWarfarePointType.MOUNTAIN)
			return MakeRegularPolygon(centerX, centerY, radius, 3, -Math.PI * 0.5);
		if (pointType == SPT_EWarfarePointType.FIELD)
			return MakeRectangle(centerX, centerY, radius * 1.25, radius * 0.75);

		return MakeRegularPolygon(centerX, centerY, radius, 10, 0.0);
	}

	protected array<float> MakeRegularPolygon(float centerX, float centerY,
		float radius, int sides, float rotation)
	{
		array<float> vertices = {};
		for (int i = 0; i < sides; i++)
		{
			float angle = rotation + i * 2.0 * Math.PI / sides;
			vertices.Insert(centerX + Math.Cos(angle) * radius);
			vertices.Insert(centerY + Math.Sin(angle) * radius);
		}
		return vertices;
	}

	protected array<float> MakeStar(float centerX, float centerY, float radius, int points)
	{
		array<float> vertices = {};
		int vertexCount = points * 2;
		for (int i = 0; i < vertexCount; i++)
		{
			float vertexRadius = radius;
			if (i % 2 == 1)
				vertexRadius = radius * 0.42;

			float angle = -Math.PI * 0.5 + i * Math.PI / points;
			vertices.Insert(centerX + Math.Cos(angle) * vertexRadius);
			vertices.Insert(centerY + Math.Sin(angle) * vertexRadius);
		}
		return vertices;
	}

	protected array<float> MakeRectangle(float centerX, float centerY, float halfWidth, float halfHeight)
	{
		array<float> vertices = {};
		vertices.Insert(centerX - halfWidth);
		vertices.Insert(centerY - halfHeight);
		vertices.Insert(centerX + halfWidth);
		vertices.Insert(centerY - halfHeight);
		vertices.Insert(centerX + halfWidth);
		vertices.Insert(centerY + halfHeight);
		vertices.Insert(centerX - halfWidth);
		vertices.Insert(centerY + halfHeight);
		return vertices;
	}

	protected void ShowTooltip(string pointId, int screenX, int screenY,
		float screenWidth, float screenHeight)
	{
		if (!m_wTooltip || !m_wTooltipTitle || !m_wTooltipBody)
			return;

		string name;
		vector position;
		SPT_EWarfarePointType pointType;
		SPT_EWarfarePointState state;
		bool isHQ;
		int manpower;
		int waveIndex;
		if (!m_Warfare.GetClientPointMapData(pointId, name, position, pointType,
			state, isHQ, manpower, waveIndex))
		{
			HideTooltip();
			return;
		}

		string title = name;
		if (isHQ)
			title = string.Format("%1 [HQ]", name);

		string reinforcement = GetReinforcementText(state, waveIndex);
		string body = string.Format("Tipo: %1\nEstado: %2\nGuarnicao: %3\nReforcos: %4",
			GetPointTypeName(pointType), GetStateName(state), manpower, reinforcement);

		m_wTooltipTitle.SetText(title);
		m_wTooltipBody.SetText(body);

		float tooltipX = screenX + 20.0;
		float tooltipY = screenY + 20.0;
		if (tooltipX + 310.0 > screenWidth)
			tooltipX = screenX - 320.0;
		if (tooltipY + 126.0 > screenHeight)
			tooltipY = screenY - 136.0;
		if (tooltipX < 8.0)
			tooltipX = 8.0;
		if (tooltipY < 8.0)
			tooltipY = 8.0;

		FrameSlot.SetPos(m_wTooltip, tooltipX, tooltipY);
		m_wTooltip.SetVisible(true);
	}

	protected void HideTooltip()
	{
		if (m_wTooltip)
			m_wTooltip.SetVisible(false);
	}

	protected string GetReinforcementText(SPT_EWarfarePointState state, int waveIndex)
	{
		if (state == SPT_EWarfarePointState.UNDER_ATTACK)
		{
			if (waveIndex >= 0)
				return string.Format("onda %1 ativa", waveIndex + 1);
			return "a caminho";
		}

		if (state == SPT_EWarfarePointState.CAPTURED_DEFENDING)
		{
			if (waveIndex >= 0)
				return string.Format("onda %1 em combate", waveIndex + 1);
			return "em combate";
		}

		return "nenhum ativo";
	}

	protected string GetPointTypeName(SPT_EWarfarePointType pointType)
	{
		if (pointType == SPT_EWarfarePointType.CITY) return "Cidade";
		if (pointType == SPT_EWarfarePointType.VILLAGE) return "Vila";
		if (pointType == SPT_EWarfarePointType.MILITARY_BASE) return "Base militar";
		if (pointType == SPT_EWarfarePointType.AIRPORT) return "Aeroporto";
		if (pointType == SPT_EWarfarePointType.RUIN) return "Ruina";
		if (pointType == SPT_EWarfarePointType.PORT) return "Porto";
		if (pointType == SPT_EWarfarePointType.FOREST) return "Floresta";
		if (pointType == SPT_EWarfarePointType.MOUNTAIN) return "Montanha";
		if (pointType == SPT_EWarfarePointType.FIELD) return "Campo";
		return "Personalizado";
	}

	protected string GetStateName(SPT_EWarfarePointState state)
	{
		if (state == SPT_EWarfarePointState.LOCKED) return "Bloqueada";
		if (state == SPT_EWarfarePointState.FRONTLINE) return "Linha de frente";
		if (state == SPT_EWarfarePointState.UNDER_ATTACK) return "Sob ataque";
		if (state == SPT_EWarfarePointState.CLEARED_WAITING) return "Limpa, aguardando captura";
		if (state == SPT_EWarfarePointState.CAPTURED_DEFENDING) return "Capturada, defendendo";
		if (state == SPT_EWarfarePointState.CAPTURED) return "Capturada";
		return "Desconhecido";
	}

	protected int GetStateColor(SPT_EWarfarePointState state)
	{
		if (state == SPT_EWarfarePointState.LOCKED) return ARGB(255, 130, 20, 20);
		if (state == SPT_EWarfarePointState.FRONTLINE) return ARGB(255, 225, 45, 35);
		if (state == SPT_EWarfarePointState.UNDER_ATTACK) return ARGB(255, 255, 145, 20);
		if (state == SPT_EWarfarePointState.CLEARED_WAITING) return ARGB(255, 245, 220, 35);
		if (state == SPT_EWarfarePointState.CAPTURED_DEFENDING) return ARGB(255, 75, 145, 255);
		if (state == SPT_EWarfarePointState.CAPTURED) return ARGB(255, 20, 115, 235);
		return ARGB(255, 130, 130, 130);
	}
}
