//! Coordenador autoritativo da campanha de conquista territorial SPT Warfare.
//! Componente singleton colocado no GameMode. Responsavel por:
//! - Registrar e validar pontos Warfare colocados manualmente
//! - Manter todos os objetivos inimigos atacaveis em qualquer ordem
//! - Gerenciar estados de cada area (FRONTLINE -> CAPTURED)
//! - Integrar com SPT_WorldGarrisonManagerComponent para batalhas e reforcos
//! - Verificar condicao de vitoria
//! - Replicar estado para clientes
[ComponentEditorProps(category: "SPT/GameMode", description: "Coordenador central do modo SPT Warfare. Campanha de conquista territorial conectada.")]
class SPT_WarfareGameModeComponentClass : ScriptComponentClass
{
}

class SPT_WarfareGameModeComponent : ScriptComponent
{
	//-----------------------------------------------------------------------
	// CONSTANTES
	//-----------------------------------------------------------------------

	protected const float PLAYER_PRESENCE_CHECK_INTERVAL_S = 2.0;
	protected const float STATE_UPDATE_INTERVAL_S = 1.0;
	protected const ResourceName RESPAWN_POINT_PREFAB = "{E7F4D5562F48DDE4}Prefabs/MP/Spawning/SpawnPoint_Base.et";

	//-----------------------------------------------------------------------
	// ATRIBUTOS
	//-----------------------------------------------------------------------

	[Attribute("US", desc: "Chave da faccao dos jogadores (ex: 'US', 'USSR').")]
	protected string m_sPlayerFactionKey;

	[Attribute("USSR", desc: "Chave da faccao inimiga.")]
	protected string m_sEnemyFactionKey;

	[Attribute("2.0", desc: "Intervalo de atualizacao principal em segundos.")]
	protected float m_fUpdateInterval;

	[Attribute("1", desc: "Exigir presenca de jogador dentro do raio para confirmar captura.")]
	protected bool m_bRequirePlayerPresence;

	[Attribute("1", desc: "Habilitar notificacoes HUD.")]
	protected bool m_bEnableNotifications;

	[Attribute("1", desc: "Habilitar condicao global de vitoria.")]
	protected bool m_bEnableVictoryCondition;

	[Attribute("", desc: "Prefab do icone do mapa para cada tipo de ponto (JSON/Resource). Deixe vazio para usar icones padrao.")]
	protected string m_sMapIconConfigPath;

	[Attribute("1", desc: "Habilitar notificacoes HUD de transicoes de estado no cliente.")]
	protected bool m_bEnableHUD;

	[Attribute("1", desc: "Habilitar pontos de respawn Warfare no menu de deployment vanilla.")]
	protected bool m_bEnableHQRespawn;

	[Attribute("{F1C5F8E2EF1105F9}Prefabs/Compositions/Misc/SubCompositions/Tent_HQ_Camo_Cluttered_US_01.et", UIWidgets.ResourceNamePicker, desc: "Prefab puramente visual criado em cada local de respawn disponivel. Vazio desabilita apenas o ambiente.", params: "et")]
	protected ResourceName m_rRespawnEnvironmentPrefab;

	//-----------------------------------------------------------------------
	// MEMBROS (SERVIDOR)
	//-----------------------------------------------------------------------

	protected static SPT_WarfareGameModeComponent s_Instance;

	//! Todos os pontos Warfare registrados, indexados por ID.
	protected ref map<string, ref SPT_WarfarePointData> m_mPointsById;

	//! Ordem estavel dos IDs para iteracao deterministica.
	protected ref array<string> m_aPointOrder;

	//! Estado atual de cada ponto (servidor).
	protected ref map<string, SPT_EWarfarePointState> m_mPointStates;

	//! Flag de captura: verdadeiro se o ponto ja foi capturado pelo jogador.
	protected ref map<string, bool> m_mPointCaptured;

	//! IDs dos pontos que sao HQ.
	protected ref array<string> m_aHQIds;

	//! IDs dos pontos que estao na frente (FRONTLINE).
	protected ref array<string> m_aFrontlineIds;

	//! Flag global de vitoria ja disparada (evita multiplos disparos).
	protected bool m_bVictoryTriggered;

	//! Fila de RPCs de atualizacao de estado pendentes para clientes.
	protected ref array<ref SPT_WarfarePointStateSnapshot> m_aPendingStateUpdates;

	//! Contador auxiliar para coleta de pontos manuais.
	protected int m_iManualPointsCollected;

	//! Erros fatais encontrados durante a coleta/configuracao manual.
	protected bool m_bManualConfigurationInvalid;

	//! Evento publico disparado quando a campanha e vencida.
	protected ref ScriptInvoker m_OnWarfareVictory = new ScriptInvoker();

	//! Spawn points funcionais do menu vanilla, indexados pelo ID Warfare.
	protected ref map<string, EntityID> m_mRespawnPointIds;

	//! Prefabs ambientais dos locais de respawn, indexados pelo ID Warfare.
	protected ref map<string, EntityID> m_mRespawnEnvironmentIds;

	//-----------------------------------------------------------------------
	// MEMBROS (CLIENTE - estado visual)
	//-----------------------------------------------------------------------

	//! Estado de cada ponto no cliente (atualizado via RPC).
	protected ref map<string, SPT_EWarfarePointState> m_mClientPointStates;

	//! Nomes de exibicao dos pontos no cliente.
	protected ref map<string, string> m_mClientPointNames;

	//! Posicoes dos pontos no cliente (recebidas via RpcDo_RegisterPoint).
	protected ref map<string, vector> m_mClientPointPositions;

	//! Tipos dos pontos no cliente.
	protected ref map<string, SPT_EWarfarePointType> m_mClientPointTypes;

	//! Flags de HQ dos pontos no cliente.
	protected ref map<string, bool> m_mClientPointHQ;

	//! Manpower de guarnicao mais recente por ponto.
	protected ref map<string, int> m_mClientPointManpower;

	//! Indice de onda mais recente por ponto.
	protected ref map<string, int> m_mClientPointWaveIndices;

	//! Ordem estavel dos IDs no cliente.
	protected ref array<string> m_aClientPointOrder;

	//! Flag para evitar notificacao de vitoria duplicada.
	protected bool m_bClientVictoryNotified;

	//! Renderer com ownership forte durante toda a vida do componente.
	protected ref SPT_WarfareMapRenderer m_MapRenderer;

	//! Colecao temporaria usada exclusivamente pelo preview do World Editor.
	protected ref array<ref SPT_WarfarePointData> m_aEditorPreviewPoints = new array<ref SPT_WarfarePointData>();
	protected int m_iEditorInvalidPointCounter;

	//-----------------------------------------------------------------------
	// SINGLETON
	//-----------------------------------------------------------------------

	static SPT_WarfareGameModeComponent GetInstance()
	{
		return s_Instance;
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (SCR_Global.IsEditMode())
			return;

		InitializeClientCollections();

		s_Instance = this;

		// O renderer escuta apenas eventos locais de mapa. Em servidor dedicado
		// esses eventos nao sao emitidos e nenhum widget e criado.
		m_MapRenderer = new SPT_WarfareMapRenderer();
		m_MapRenderer.Init(this);

		// Servidor: inicializacao normal
		if (!Replication.IsServer())
			return;

		m_mPointsById = new map<string, ref SPT_WarfarePointData>();
		m_aPointOrder = new array<string>();
		m_mPointStates = new map<string, SPT_EWarfarePointState>();
		m_mPointCaptured = new map<string, bool>();
		m_aHQIds = new array<string>();
		m_aFrontlineIds = new array<string>();
		m_aPendingStateUpdates = new array<ref SPT_WarfarePointStateSnapshot>();
		m_mRespawnPointIds = new map<string, EntityID>();
		m_mRespawnEnvironmentIds = new map<string, EntityID>();

		Print("[SPT_Warfare] Inicializando componente de GameMode...");
		GetGame().GetCallqueue().CallLater(InitializeWarfare, 2000, false);
	}

	void ~SPT_WarfareGameModeComponent()
	{
		if (Replication.IsServer())
			CleanupRespawnPoints();

		if (m_MapRenderer)
		{
			m_MapRenderer.Shutdown();
			m_MapRenderer = null;
		}

		if (s_Instance == this)
			s_Instance = null;
	}

	protected void InitializeClientCollections()
	{
		m_mClientPointStates = new map<string, SPT_EWarfarePointState>();
		m_mClientPointNames = new map<string, string>();
		m_mClientPointPositions = new map<string, vector>();
		m_mClientPointTypes = new map<string, SPT_EWarfarePointType>();
		m_mClientPointHQ = new map<string, bool>();
		m_mClientPointManpower = new map<string, int>();
		m_mClientPointWaveIndices = new map<string, int>();
		m_aClientPointOrder = new array<string>();
	}

	//! Ativa frames somente enquanto o mapa tatico esta aberto.
	void SetMapRenderingActive(bool active)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (active)
			SetEventMask(owner, EntityEvent.FRAME);
		else
			ClearEventMask(owner, EntityEvent.FRAME);
	}

	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);

		if (m_MapRenderer)
			m_MapRenderer.Update(timeSlice);
	}

	//! Solicita atualizacao somente quando a entidade dona estiver selecionada.
	override int _WB_GetAfterWorldUpdateSpecs(IEntity owner, IEntitySource src)
	{
		return 1;
	}

	override void _WB_AfterWorldUpdate(IEntity owner, float timeSlice)
	{
		if (!SCR_Global.IsEditMode())
			return;

		DrawEditorPreview(owner);
	}

	//! Reconstroi e desenha os pontos a cada atualizacao do World Editor.
	//! Shapes ONCE evitam manter handles ou deixar desenhos residuais.
	protected void DrawEditorPreview(IEntity owner)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		m_aEditorPreviewPoints.Clear();
		m_iEditorInvalidPointCounter = 0;

		world.QueryEntitiesBySphere(
			vector.Zero,
			99999999,
			CollectEditorManualPoint,
			FilterWarfarePoint,
			EQueryEntitiesFlags.ALL);

		foreach (SPT_WarfarePointData point : m_aEditorPreviewPoints)
		{
			int color = GetEditorPointColor(point);
			DrawEditorPoint(point, color);
		}
	}

	protected bool CollectEditorManualPoint(IEntity entity)
	{
		SPT_WarfareNodeComponent comp = FindWarfareNodeComponent(entity);
		if (!comp)
			return true;

		SPT_WarfarePointData point = new SPT_WarfarePointData();
		point.m_sPointId = comp.GetPointId().Trim();
		point.m_sDisplayName = comp.GetDisplayName().Trim();
		point.m_ePointType = comp.GetPointType();
		point.m_vCenter = entity.GetOrigin();
		point.m_fRadius = comp.GetRadius();
		point.m_bIsHQ = comp.IsHQ();
		point.m_bCountsForVictory = comp.CountsForVictory();

		if (point.m_sPointId.IsEmpty())
		{
			point.m_bPreviewInvalid = true;
			point.m_sPointId = string.Format("__INVALID_%1", m_iEditorInvalidPointCounter++);
		}
		else
		{
			SPT_WarfarePointData duplicate = FindEditorPreviewPoint(point.m_sPointId);
			if (duplicate)
			{
				duplicate.m_bPreviewInvalid = true;
				point.m_bPreviewInvalid = true;
				point.m_sPointId = string.Format("%1__DUPLICATE_%2",
					point.m_sPointId, m_iEditorInvalidPointCounter++);
			}
		}

		if (point.m_sDisplayName.IsEmpty())
			point.m_sDisplayName = point.m_sPointId;
		m_aEditorPreviewPoints.Insert(point);
		return true;
	}

	protected SPT_WarfarePointData FindEditorPreviewPoint(string pointId)
	{
		foreach (SPT_WarfarePointData point : m_aEditorPreviewPoints)
		{
			if (point.m_sPointId == pointId)
				return point;
		}
		return null;
	}

	protected int GetEditorPointColor(SPT_WarfarePointData point)
	{
		if (point.m_bPreviewInvalid)
			return ARGB(255, 255, 0, 255);
		if (point.m_bIsHQ)
			return ARGB(255, 40, 120, 255);
		return ARGB(255, 255, 140, 0);
	}

	protected void DrawEditorPoint(SPT_WarfarePointData point, int color)
	{
		vector center = point.m_vCenter + Vector(0, 1, 0);
		Shape.CreateSphere(color, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, center, 4);

		const int SEGMENTS = 32;
		vector circle[64];
		float radius = Math.Max(point.m_fRadius, 1);
		for (int i = 0; i < SEGMENTS; i++)
		{
			float angleA = Math.PI * 2.0 * i / SEGMENTS;
			float angleB = Math.PI * 2.0 * (i + 1) / SEGMENTS;
			circle[i * 2] = center + Vector(Math.Cos(angleA) * radius, 0, Math.Sin(angleA) * radius);
			circle[i * 2 + 1] = center + Vector(Math.Cos(angleB) * radius, 0, Math.Sin(angleB) * radius);
		}
		Shape.CreateLines(color, ShapeFlags.ONCE | ShapeFlags.NOZBUFFER, circle, SEGMENTS * 2);

		string label = string.Format("%1 | %2", point.m_sPointId, point.m_sDisplayName);
		DebugTextWorldSpace.Create(
			GetGame().GetWorld(),
			label,
			DebugTextFlags.ONCE | DebugTextFlags.CENTER | DebugTextFlags.FACE_CAMERA,
			center[0], center[1] + 5, center[2],
			18, color, ARGB(160, 0, 0, 0), 0);
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------
	// INICIALIZACAO
	//-----------------------------------------------------------------------

	protected void InitializeWarfare()
	{
		Print("[SPT_Warfare] Registrando pontos Warfare...");

		// A configuracao Warfare e exclusivamente manual.
		CollectManualPoints();

		// Inicializa estados antes da validacao para popular m_aHQIds.
		InitializePointStates();

		if (!ValidateConfiguration())
		{
			Print("[SPT_Warfare] ERRO: Validacao da configuracao falhou. Warfare desabilitado.", LogLevel.ERROR);
			return;
		}

		// Cria as guarnicoes nos centros manuais somente apos validar os pontos.
		ConfigureManualGarrisonLocations();
		if (m_bManualConfigurationInvalid)
		{
			Print("[SPT_Warfare] ERRO: Falha ao configurar guarnicoes manuais. Warfare desabilitado.", LogLevel.ERROR);
			return;
		}

		// 6. Subscreve aos eventos da guarnicao
		SubscribeToGarrisonEvents();

		// 7. Registra HQs no deployment menu vanilla.
		if (m_bEnableHQRespawn)
			InitializeRespawnPoints();

		// 8. Inicia loops de atualizacao
		int presenceIntervalMs = Math.Round(PLAYER_PRESENCE_CHECK_INTERVAL_S * 1000);
		int stateIntervalMs = Math.Round(STATE_UPDATE_INTERVAL_S * 1000);
		GetGame().GetCallqueue().CallLater(CheckPlayerPresence, presenceIntervalMs, true);
		GetGame().GetCallqueue().CallLater(UpdateWarfareState, stateIntervalMs, true);

		// 9. Envia estado inicial para clientes
		BroadcastFullState();

		Print(string.Format("[SPT_Warfare] Inicializacao concluida | pontos=%1 | HQs=%2 | tipo=%3",
			m_aPointOrder.Count(), m_aHQIds.Count(), m_sPlayerFactionKey));
	}

	//-----------------------------------------------------------------------
	// COLETA DE PONTOS
	//-----------------------------------------------------------------------

	protected void CollectManualPoints()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		m_iManualPointsCollected = 0;

		world.QueryEntitiesBySphere(
			vector.Zero,
			99999999,
			CollectManualPoint,
			FilterWarfarePoint,
			EQueryEntitiesFlags.ALL
		);

		Print(string.Format("[SPT_Warfare] Pontos manuais registrados: %1", m_iManualPointsCollected));
	}

	protected bool FilterWarfarePoint(IEntity entity)
	{
		if (!entity)
			return false;

		SPT_WarfareNodeComponent comp = FindWarfareNodeComponent(entity);
		return comp != null;
	}

	protected bool CollectManualPoint(IEntity entity)
	{
		SPT_WarfareNodeComponent comp = FindWarfareNodeComponent(entity);
		if (!comp)
			return true;

		RegisterManualPoint(comp, entity);
		return true;
	}

	protected SPT_WarfareNodeComponent FindWarfareNodeComponent(IEntity entity)
	{
		if (!entity)
			return null;

		SPT_WarfareHQComponent hq = SPT_WarfareHQComponent.Cast(
			entity.FindComponent(SPT_WarfareHQComponent));
		if (hq)
			return hq;

		return SPT_WarfarePointComponent.Cast(
			entity.FindComponent(SPT_WarfarePointComponent));
	}

	protected void RegisterManualPoint(SPT_WarfareNodeComponent comp, IEntity entity)
	{
		string pointId = comp.GetPointId().Trim();
		if (pointId.IsEmpty())
		{
			Print(string.Format("[SPT_Warfare] ERRO: Ponto manual em %1 nao possui ID estavel.",
				entity.GetOrigin()), LogLevel.ERROR);
			m_bManualConfigurationInvalid = true;
			return;
		}

		if (m_mPointsById.Contains(pointId))
		{
			Print(string.Format("[SPT_Warfare] ERRO: ID manual duplicado: %1", pointId), LogLevel.ERROR);
			m_bManualConfigurationInvalid = true;
			return;
		}

		SPT_WarfarePointData data = new SPT_WarfarePointData();
		data.m_sPointId = pointId;
		data.m_sDisplayName = comp.GetDisplayName().Trim();
		if (data.m_sDisplayName.IsEmpty())
			data.m_sDisplayName = pointId;
		data.m_ePointType = comp.GetPointType();
		data.m_vCenter = entity.GetOrigin();
		data.m_fRadius = comp.GetRadius();
		data.m_bIsHQ = comp.IsHQ();
		data.m_bCountsForVictory = comp.CountsForVictory();
		if (!data.m_bIsHQ)
		{
			SPT_WarfarePointComponent hostilePoint = SPT_WarfarePointComponent.Cast(comp);
			if (!hostilePoint)
			{
				Print(string.Format("[SPT_Warfare] ERRO: Objetivo inimigo %1 nao usa SPT_WarfarePointComponent.",
					pointId), LogLevel.ERROR);
				m_bManualConfigurationInvalid = true;
				return;
			}

			data.m_sGarrisonLocationId = pointId;
			data.m_GarrisonConfig = hostilePoint.CreateGarrisonConfig();
		}

		m_mPointsById.Set(pointId, data);
		m_aPointOrder.Insert(pointId);
		m_iManualPointsCollected++;

		Print(string.Format("[SPT_Warfare] Ponto manual registrado | id=%1 | nome=%2 | tipo=%3 | HQ=%4",
			pointId, data.m_sDisplayName, data.m_ePointType, data.m_bIsHQ));
	}

	protected void ConfigureManualGarrisonLocations()
	{
		SPT_WorldGarrisonManagerComponent garrisonMgr = SPT_WorldGarrisonManagerComponent.GetInstance();
		if (!garrisonMgr)
		{
			Print("[SPT_Warfare] ERRO: WorldGarrisonManager nao encontrado.", LogLevel.ERROR);
			m_bManualConfigurationInvalid = true;
			return;
		}

		garrisonMgr.ResetLocationsForManualWarfare();
		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData point = m_mPointsById.Get(pointId);
			if (!point)
				continue;
			if (point.m_bIsHQ)
				continue;
			if (!garrisonMgr.RegisterManualWarfareLocation(
				pointId, point.m_sDisplayName, point.m_vCenter, point.m_GarrisonConfig))
			{
				Print(string.Format("[SPT_Warfare] ERRO: Falha ao registrar guarnicao manual %1.",
					pointId), LogLevel.ERROR);
				m_bManualConfigurationInvalid = true;
			}
		}
	}

	//-----------------------------------------------------------------------
	// VALIDACAO DA CONFIGURACAO
	//-----------------------------------------------------------------------

	protected bool ValidateConfiguration()
	{
		bool valid = true;

		if (m_bManualConfigurationInvalid)
			valid = false;

		// Verifica HQs
		if (m_aHQIds.IsEmpty())
		{
			Print("[SPT_Warfare] ERRO: Nenhum SPT_WarfareHQ foi colocado na missao.", LogLevel.ERROR);
			valid = false;
		}

		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data)
				continue;

			if (!data.m_bIsHQ && data.m_sGarrisonLocationId.IsEmpty())
			{
				Print(string.Format("[SPT_Warfare] ERRO: Ponto inimigo %1 nao possui guarnicao vinculada.",
					pointId), LogLevel.ERROR);
				valid = false;
			}
		}

		return valid;
	}

	//-----------------------------------------------------------------------
	// INICIALIZACAO DE ESTADOS
	//-----------------------------------------------------------------------

	protected void InitializePointStates()
	{
		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data)
				continue;

			if (data.m_bIsHQ)
			{
				m_mPointStates.Set(pointId, SPT_EWarfarePointState.CAPTURED);
				m_mPointCaptured.Set(pointId, true);
				m_aHQIds.Insert(pointId);
				Print(string.Format("[SPT_Warfare] HQ configurada | id=%1 | nome=%2", pointId, data.m_sDisplayName));
			}
			else
			{
				m_mPointStates.Set(pointId, SPT_EWarfarePointState.FRONTLINE);
				m_mPointCaptured.Set(pointId, false);
			}
		}

		// Todos os objetivos inimigos comecam atacaveis.
		RecalculateFrontline();
	}

	//-----------------------------------------------------------------------
	// OBJETIVOS ATACAVEIS
	//-----------------------------------------------------------------------

	//! Mantem na lista de frente todos os objetivos inimigos ainda nao capturados.
	protected void RecalculateFrontline()
	{
		m_aFrontlineIds.Clear();

		foreach (string pointId : m_aPointOrder)
		{
			if (IsPointCaptured(pointId))
				continue;

			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data || data.m_bIsHQ)
				continue;

			m_aFrontlineIds.Insert(pointId);
		}

		Print(string.Format("[SPT_Warfare] Objetivos atacaveis atualizados | disponiveis=%1 | totalCapturados=%2",
			m_aFrontlineIds.Count(), GetCapturedPointCount()));
	}

	//-----------------------------------------------------------------------
	// EVENTOS DA GUARNICAO
	//-----------------------------------------------------------------------

	protected void SubscribeToGarrisonEvents()
	{
		SPT_WorldGarrisonManagerComponent garrisonMgr = SPT_WorldGarrisonManagerComponent.GetInstance();
		if (!garrisonMgr)
			return;

		garrisonMgr.GetOnGarrisonFirstCasualty().Insert(OnGarrisonFirstCasualty);
		garrisonMgr.GetOnGarrisonCleared().Insert(OnGarrisonCleared);
		garrisonMgr.GetOnBattleStarted().Insert(OnBattleStarted);
		garrisonMgr.GetOnBattleWaveScheduled().Insert(OnBattleWaveScheduled);
		garrisonMgr.GetOnBattleWaveDeployed().Insert(OnBattleWaveDeployed);
		garrisonMgr.GetOnBattleEnded().Insert(OnBattleEnded);

		StartMonitoringAllAttackablePoints(garrisonMgr);
	}

	protected void OnGarrisonFirstCasualty(string locationId)
	{
		string pointId = FindPointByGarrisonLocation(locationId);
		if (pointId.IsEmpty())
			return;

		SPT_EWarfarePointState currentState = GetPointState(pointId);
		if (currentState == SPT_EWarfarePointState.FRONTLINE)
		{
			ChangePointState(pointId, SPT_EWarfarePointState.UNDER_ATTACK);

			// Inicia batalha de reforcos
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (data)
			{
				SPT_WorldGarrisonManagerComponent garrisonMgr = SPT_WorldGarrisonManagerComponent.GetInstance();
				if (garrisonMgr)
					garrisonMgr.StartLocationBattle(data.m_vCenter);
			}
		}
	}

	protected void OnGarrisonCleared(string locationId)
	{
		string pointId = FindPointByGarrisonLocation(locationId);
		if (pointId.IsEmpty())
			return;

		// Verifica se ha jogador presente no raio
		bool playerPresent = !m_bRequirePlayerPresence || IsPlayerInRadius(pointId);

		if (playerPresent)
		{
			// Captura imediata
			CapturePoint(pointId);
		}
		else
		{
			// Aguardando presenca
			ChangePointState(pointId, SPT_EWarfarePointState.CLEARED_WAITING);

			if (!playerPresent)
				Print(string.Format("[SPT_Warfare] Aguardando jogador | ponto=%1", pointId));
		}
	}

	protected void OnBattleStarted(string locationId)
	{
		string pointId = FindPointByGarrisonLocation(locationId);
		if (pointId.IsEmpty())
			return;

		Print(string.Format("[SPT_Warfare] Batalha iniciada no ponto %1", pointId));
	}

	protected void OnBattleWaveScheduled(string locationId, int waveIndex)
	{
		string pointId = FindPointByGarrisonLocation(locationId);
		if (pointId.IsEmpty())
			return;

		Print(string.Format("[SPT_Warfare] Onda %1 agendada | ponto=%2", waveIndex, pointId));
	}

	protected void OnBattleWaveDeployed(string locationId, int waveIndex, int unitCount)
	{
		string pointId = FindPointByGarrisonLocation(locationId);
		if (pointId.IsEmpty())
			return;

		Print(string.Format("[SPT_Warfare] Onda %1 materializada | ponto=%2 | unidades=%3",
			waveIndex, pointId, unitCount));
	}

	protected void OnBattleEnded(string locationId)
	{
		string pointId = FindPointByGarrisonLocation(locationId);
		if (pointId.IsEmpty())
			return;

		SPT_EWarfarePointState currentState = GetPointState(pointId);

		if (currentState == SPT_EWarfarePointState.CAPTURED_DEFENDING)
		{
			ChangePointState(pointId, SPT_EWarfarePointState.CAPTURED);
			CheckVictory();
		}
	}

	//! Encontra o ID do ponto Warfare associado a uma localizacao de guarnicao.
	protected string FindPointByGarrisonLocation(string garrisonLocationId)
	{
		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (data && data.m_sGarrisonLocationId == garrisonLocationId)
				return pointId;
		}
		return "";
	}

	//-----------------------------------------------------------------------
	// REGRA DE CAPTURA
	//-----------------------------------------------------------------------

	protected void CapturePoint(string pointId)
	{
		SPT_WarfarePointData data = m_mPointsById.Get(pointId);
		if (!data)
			return;

		m_mPointCaptured.Set(pointId, true);

		SPT_WorldGarrisonManagerComponent garrisonMgr = SPT_WorldGarrisonManagerComponent.GetInstance();
		bool hasActiveBattle = garrisonMgr && garrisonMgr.IsLocationBattleActive(data.m_vCenter);

		SPT_EWarfarePointState newState;
		if (hasActiveBattle)
		{
			newState = SPT_EWarfarePointState.CAPTURED_DEFENDING;
		}
		else
		{
			newState = SPT_EWarfarePointState.CAPTURED;
		}

		ChangePointState(pointId, newState);

		Print(string.Format("[SPT_Warfare] Ponto capturado | id=%1 | nome=%2 | novoEstado=%3 | batalhaAtiva=%4",
			pointId, data.m_sDisplayName, newState, hasActiveBattle));

		// Verifica vitoria
		if (newState == SPT_EWarfarePointState.CAPTURED)
			CheckVictory();
	}

	protected void StartMonitoringAllAttackablePoints(SPT_WorldGarrisonManagerComponent garrisonMgr)
	{
		if (!garrisonMgr)
			return;

		foreach (string pointId : m_aFrontlineIds)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data || data.m_sGarrisonLocationId.IsEmpty())
				continue;

			garrisonMgr.StartMonitoringLocation(data.m_sGarrisonLocationId);
		}
	}

	//-----------------------------------------------------------------------
	// VERIFICACAO DE PRESENCA DE JOGADOR
	//-----------------------------------------------------------------------

	protected void CheckPlayerPresence()
	{
		if (!m_bRequirePlayerPresence)
			return;

		array<vector> playerPositions = {};
		CollectPlayerPositions(playerPositions);

		foreach (string pointId : m_aPointOrder)
		{
			SPT_EWarfarePointState state = GetPointState(pointId);
			if (state != SPT_EWarfarePointState.CLEARED_WAITING)
				continue;

			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data)
				continue;

			bool playerPresent = IsAnyPlayerInRadius(data.m_vCenter, data.m_fRadius, playerPositions);
			if (!playerPresent)
				continue;

			CapturePoint(pointId);
		}
	}

	protected bool IsPlayerInRadius(string pointId)
	{
		SPT_WarfarePointData data = m_mPointsById.Get(pointId);
		if (!data)
			return false;

		array<vector> playerPositions = {};
		CollectPlayerPositions(playerPositions);
		return IsAnyPlayerInRadius(data.m_vCenter, data.m_fRadius, playerPositions);
	}

	protected bool IsAnyPlayerInRadius(vector center, float radius, notnull array<vector> playerPositions)
	{
		float radiusSq = radius * radius;
		foreach (vector playerPos : playerPositions)
		{
			if (HorizontalDistanceSq(center, playerPos) <= radiusSq)
				return true;
		}
		return false;
	}

	protected void CollectPlayerPositions(out array<vector> positions)
	{
		positions.Clear();

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		foreach (int playerId : playerIds)
		{
			IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerId);
			if (!playerEntity)
				continue;

			positions.Insert(playerEntity.GetOrigin());
		}
	}

	//-----------------------------------------------------------------------
	// LOOP DE ATUALIZACAO DE ESTADO
	//-----------------------------------------------------------------------

	protected void UpdateWarfareState()
	{
		// Envia atualizacoes pendentes para clientes
		if (!m_aPendingStateUpdates.IsEmpty())
		{
			BroadcastPendingStateUpdates();
		}
	}

	//-----------------------------------------------------------------------
	// GERENCIAMENTO DE ESTADO
	//-----------------------------------------------------------------------

	protected void ChangePointState(string pointId, SPT_EWarfarePointState newState)
	{
		SPT_EWarfarePointState oldState = GetPointState(pointId);

		m_mPointStates.Set(pointId, newState);

		// Enfileira atualizacao para replicacao
		EnqueueStateUpdate(pointId, newState);

		// Remove da frente se capturado
		if (newState == SPT_EWarfarePointState.CAPTURED || newState == SPT_EWarfarePointState.CAPTURED_DEFENDING)
		{
			int frontlineIdx = m_aFrontlineIds.Find(pointId);
			if (frontlineIdx >= 0)
				m_aFrontlineIds.Remove(frontlineIdx);
		}

		// Objetivos so viram respawn depois que a batalha foi estabilizada.
		if (m_bEnableHQRespawn && newState == SPT_EWarfarePointState.CAPTURED)
			EnsureRespawnPoint(pointId);
	}

	protected void EnqueueStateUpdate(string pointId, SPT_EWarfarePointState state)
	{
		SPT_WarfarePointStateSnapshot snapshot = new SPT_WarfarePointStateSnapshot();
		snapshot.m_sPointId = pointId;
		snapshot.m_eState = state;
		snapshot.m_bCaptured = IsPointCaptured(pointId);

		// Preenche dados de manpower
		SPT_WarfarePointData data = m_mPointsById.Get(pointId);
		if (data && !data.m_sGarrisonLocationId.IsEmpty())
		{
			SPT_WorldGarrisonManagerComponent garrisonMgr = SPT_WorldGarrisonManagerComponent.GetInstance();
			if (garrisonMgr)
			{
				vector center;
				string name;
				int descType, manpower, targetManpower, pendingSpawns;
				bool cleared, battleActive;
				int budget, waveIndex, waveCount;

				if (garrisonMgr.GetLocationState(data.m_sGarrisonLocationId, center, name, descType,
					manpower, targetManpower, pendingSpawns, cleared, budget,
					battleActive, waveIndex, waveCount))
				{
					snapshot.m_iGarrisonManpower = manpower;
					snapshot.m_iGarrisonInitialManpower = targetManpower;
					snapshot.m_iBattleWaveIndex = waveIndex;
				}
			}
		}

		m_aPendingStateUpdates.Insert(snapshot);
	}

	//-----------------------------------------------------------------------
	// CONDICAO DE VITORIA
	//-----------------------------------------------------------------------

	protected void CheckVictory()
	{
		if (!m_bEnableVictoryCondition || m_bVictoryTriggered)
			return;

		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data)
				continue;

			if (!data.m_bCountsForVictory)
				continue;

			if (!IsPointCaptured(pointId))
				return;
		}

		// Todos os pontos capturados!
		m_bVictoryTriggered = true;
		Print("[SPT_Warfare] VITORIA! Todos os pontos foram capturados.");
		m_OnWarfareVictory.Invoke();

		// Broadcast de vitoria para clientes
		Rpc(RpcDo_WarfareVictory);
	}

	//-----------------------------------------------------------------------
	// API PUBLICA
	//-----------------------------------------------------------------------

	SPT_EWarfarePointState GetPointState(string pointId)
	{
		return m_mPointStates.Get(pointId);
	}

	bool IsPointCaptured(string pointId)
	{
		return m_mPointCaptured.Get(pointId);
	}

	bool IsPointAttackable(string pointId)
	{
		return m_aFrontlineIds.Find(pointId) >= 0;
	}

	bool IsPointFrontline(string pointId)
	{
		return m_aFrontlineIds.Find(pointId) >= 0;
	}

	float GetCampaignProgress()
	{
		int capturable = 0;
		int captured = 0;

		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data || !data.m_bCountsForVictory)
				continue;

			capturable++;
			if (IsPointCaptured(pointId))
				captured++;
		}

		if (capturable == 0)
			return 0.0;

		return (float)captured / (float)capturable * 100.0;
	}

	int GetCapturedPointCount()
	{
		int count;
		foreach (string pointId : m_aPointOrder)
		{
			if (IsPointCaptured(pointId))
				count++;
		}
		return count;
	}

	int GetTotalPointCount()
	{
		if (Replication.IsServer() && m_aPointOrder)
			return m_aPointOrder.Count();
		if (m_aClientPointOrder)
			return m_aClientPointOrder.Count();
		return 0;
	}

	string GetPointDisplayName(string pointId)
	{
		SPT_WarfarePointData data = m_mPointsById.Get(pointId);
		if (!data)
			return "";
		return data.m_sDisplayName;
	}

	//! Evento publico de vitoria. Inscreva-se via GetOnWarfareVictory().Insert().
	ScriptInvoker GetOnWarfareVictory() { return m_OnWarfareVictory; }

	//! Retorna o estado atual de um ponto no cliente (atualizado via RPC).
	SPT_EWarfarePointState GetClientPointState(string pointId)
	{
		return m_mClientPointStates.Get(pointId);
	}

	//! Retorna o nome de exibicao de um ponto no cliente.
	string GetClientPointName(string pointId)
	{
		if (m_mClientPointNames.Contains(pointId))
		{
			string name = m_mClientPointNames.Get(pointId);
			if (!name.IsEmpty())
				return name;
		}
		return pointId;
	}

	//! Retorna a posicao de um ponto no cliente (recebida via RpcDo_RegisterPoint).
	bool GetClientPointPosition(string pointId, out vector outPos)
	{
		outPos = vector.Zero;
		if (m_mClientPointPositions.Contains(pointId))
		{
			outPos = m_mClientPointPositions.Get(pointId);
			return true;
		}
		return false;
	}

	//! Preenche um array com todos os IDs de ponto conhecidos no cliente.
	void GetClientPointIds(notnull array<string> outIds)
	{
		outIds.Clear();
		if (!m_aClientPointOrder)
			return;

		foreach (string pointId : m_aClientPointOrder)
			outIds.Insert(pointId);
	}

	//! Retorna todos os dados necessarios para renderizar um ponto no mapa.
	bool GetClientPointMapData(string pointId, out string outName, out vector outPosition,
		out SPT_EWarfarePointType outType, out SPT_EWarfarePointState outState,
		out bool outIsHQ, out int outManpower, out int outWaveIndex)
	{
		outName = pointId;
		outPosition = vector.Zero;
		outType = SPT_EWarfarePointType.CUSTOM;
		outState = SPT_EWarfarePointState.FRONTLINE;
		outIsHQ = false;
		outManpower = 0;
		outWaveIndex = -1;

		if (!m_mClientPointPositions || !m_mClientPointPositions.Contains(pointId))
			return false;

		outPosition = m_mClientPointPositions.Get(pointId);

		if (m_mClientPointNames && m_mClientPointNames.Contains(pointId))
		{
			string displayName = m_mClientPointNames.Get(pointId);
			if (!displayName.IsEmpty())
				outName = displayName;
		}

		if (m_mClientPointTypes && m_mClientPointTypes.Contains(pointId))
			outType = m_mClientPointTypes.Get(pointId);
		if (m_mClientPointStates && m_mClientPointStates.Contains(pointId))
			outState = m_mClientPointStates.Get(pointId);
		if (m_mClientPointHQ && m_mClientPointHQ.Contains(pointId))
			outIsHQ = m_mClientPointHQ.Get(pointId);
		if (m_mClientPointManpower && m_mClientPointManpower.Contains(pointId))
			outManpower = m_mClientPointManpower.Get(pointId);
		if (m_mClientPointWaveIndices && m_mClientPointWaveIndices.Contains(pointId))
			outWaveIndex = m_mClientPointWaveIndices.Get(pointId);

		return true;
	}

	//-----------------------------------------------------------------------
	// REPLICACAO DE ESTADO
	//-----------------------------------------------------------------------

	protected void GetPointCombatInfo(SPT_WarfarePointData data, out int manpower, out int waveIndex)
	{
		manpower = 0;
		waveIndex = -1;
		if (!data || data.m_sGarrisonLocationId.IsEmpty())
			return;

		SPT_WorldGarrisonManagerComponent garrisonMgr = SPT_WorldGarrisonManagerComponent.GetInstance();
		if (!garrisonMgr)
			return;

		vector center;
		string name;
		int descriptorType;
		int targetManpower;
		int pendingSpawns;
		bool cleared;
		int budget;
		bool battleActive;
		int waveCount;
		garrisonMgr.GetLocationState(data.m_sGarrisonLocationId, center, name, descriptorType,
			manpower, targetManpower, pendingSpawns, cleared, budget,
			battleActive, waveIndex, waveCount);
	}

	//! Envia o estado completo de todos os pontos para clientes (JIP / inicial).
	void BroadcastFullState()
	{
		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			int stateRaw = GetPointState(pointId);
			int capturedRaw;
			if (IsPointCaptured(pointId))
				capturedRaw = 1;
			else
				capturedRaw = 0;

			// Envia registro do ponto (nome, posicao, tipo)
			if (data)
			{
				string name = data.m_sDisplayName;
				int typeRaw = data.m_ePointType;
				int isHQ;
				if (data.m_bIsHQ)
					isHQ = 1;
				else
					isHQ = 0;

				RpcDo_RegisterPoint(pointId, name, typeRaw, data.m_vCenter[0], data.m_vCenter[2], isHQ);
				Rpc(RpcDo_RegisterPoint, pointId, name, typeRaw, data.m_vCenter[0], data.m_vCenter[2], isHQ);
			}

			int manpower;
			int waveIndex;
			GetPointCombatInfo(data, manpower, waveIndex);
			RpcDo_PointStateChanged(pointId, stateRaw, capturedRaw, manpower, waveIndex);
			Rpc(RpcDo_PointStateChanged, pointId, stateRaw, capturedRaw, manpower, waveIndex);
		}
	}

	protected void BroadcastPendingStateUpdates()
	{
		if (m_aPendingStateUpdates.IsEmpty())
			return;

		foreach (int i, SPT_WarfarePointStateSnapshot s : m_aPendingStateUpdates)
		{
			int stateRaw = s.m_eState;
			int capturedRaw;
			if (s.m_bCaptured)
				capturedRaw = 1;
			else
				capturedRaw = 0;

			RpcDo_PointStateChanged(s.m_sPointId, stateRaw, capturedRaw,
				s.m_iGarrisonManpower, s.m_iBattleWaveIndex);
			Rpc(RpcDo_PointStateChanged, s.m_sPointId, stateRaw, capturedRaw,
				s.m_iGarrisonManpower, s.m_iBattleWaveIndex);
		}
		m_aPendingStateUpdates.Clear();
	}

	//! Snapshot completo para jogadores que entram depois da inicializacao.
	override bool RplSave(ScriptBitWriter writer)
	{
		int count = 0;
		if (m_aPointOrder)
			count = m_aPointOrder.Count();

		writer.WriteInt(count);
		if (count == 0)
			return true;

		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data)
			{
				writer.WriteString(pointId);
				writer.WriteString(pointId);
				writer.WriteInt(SPT_EWarfarePointType.CUSTOM);
				writer.WriteFloat(0.0);
				writer.WriteFloat(0.0);
				writer.WriteInt(0);
				writer.WriteInt(SPT_EWarfarePointState.FRONTLINE);
				writer.WriteInt(0);
				writer.WriteInt(0);
				writer.WriteInt(-1);
				continue;
			}

			int isHQ = 0;
			if (data.m_bIsHQ)
				isHQ = 1;

			int captured = 0;
			if (IsPointCaptured(pointId))
				captured = 1;

			int manpower;
			int waveIndex;
			GetPointCombatInfo(data, manpower, waveIndex);

			writer.WriteString(pointId);
			writer.WriteString(data.m_sDisplayName);
			writer.WriteInt(data.m_ePointType);
			writer.WriteFloat(data.m_vCenter[0]);
			writer.WriteFloat(data.m_vCenter[2]);
			writer.WriteInt(isHQ);
			writer.WriteInt(GetPointState(pointId));
			writer.WriteInt(captured);
			writer.WriteInt(manpower);
			writer.WriteInt(waveIndex);
		}

		return true;
	}

	override bool RplLoad(ScriptBitReader reader)
	{
		InitializeClientCollections();

		int count;
		reader.ReadInt(count);
		for (int i = 0; i < count; i++)
		{
			string pointId;
			string displayName;
			int typeRaw;
			float positionX;
			float positionZ;
			int isHQ;
			int stateRaw;
			int captured;
			int manpower;
			int waveIndex;

			reader.ReadString(pointId);
			reader.ReadString(displayName);
			reader.ReadInt(typeRaw);
			reader.ReadFloat(positionX);
			reader.ReadFloat(positionZ);
			reader.ReadInt(isHQ);
			reader.ReadInt(stateRaw);
			reader.ReadInt(captured);
			reader.ReadInt(manpower);
			reader.ReadInt(waveIndex);

			m_aClientPointOrder.Insert(pointId);
			m_mClientPointNames.Set(pointId, displayName);
			m_mClientPointPositions.Set(pointId, Vector(positionX, 0, positionZ));
			m_mClientPointTypes.Set(pointId, typeRaw);
			m_mClientPointHQ.Set(pointId, isHQ != 0);
			m_mClientPointStates.Set(pointId, stateRaw);
			m_mClientPointManpower.Set(pointId, manpower);
			m_mClientPointWaveIndices.Set(pointId, waveIndex);
		}

		return true;
	}

	//! RPC de registro do ponto (nome, posicao, tipo) - enviado no estado inicial.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterPoint(string pointId, string name, int typeRaw, float posX, float posZ, int isHQ)
	{
		if (!m_aClientPointOrder)
			InitializeClientCollections();

		m_mClientPointNames.Set(pointId, name);
		m_mClientPointPositions.Set(pointId, Vector(posX, 0, posZ));
		m_mClientPointTypes.Set(pointId, typeRaw);
		m_mClientPointHQ.Set(pointId, isHQ != 0);
		if (m_aClientPointOrder.Find(pointId) < 0)
			m_aClientPointOrder.Insert(pointId);
	}

	//! RPC simples com tipos basicos: string, int, int (bool como 0/1).
	//! Clientes processam a atualizacao de estado de um ponto.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_PointStateChanged(string pointId, int stateRaw, int capturedRaw, int manpower, int waveIndex)
	{
		if (!m_aClientPointOrder)
			InitializeClientCollections();

		SPT_EWarfarePointState newState = stateRaw;

		// Obtem estado anterior para detectar transicao
		SPT_EWarfarePointState oldState = SPT_EWarfarePointState.FRONTLINE;
		if (m_mClientPointStates.Contains(pointId))
			oldState = m_mClientPointStates.Get(pointId);

		// Atualiza estado local
		m_mClientPointStates.Set(pointId, newState);
		m_mClientPointManpower.Set(pointId, manpower);
		m_mClientPointWaveIndices.Set(pointId, waveIndex);

		// Notifica transicao se houve mudanca
		if (newState != oldState && m_bEnableHUD)
		{
			NotifyClientTransition(pointId, oldState, newState, manpower, waveIndex);
		}
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_WarfareVictory()
	{
		if (m_bClientVictoryNotified)
			return;

		m_bClientVictoryNotified = true;

		if (m_bEnableHUD)
		{
			SCR_HintManagerComponent hintMgr = SCR_HintManagerComponent.GetInstance();
			if (hintMgr)
				hintMgr.ShowCustom("VITORIA! Todas as areas foram conquistadas.");
		}
	}

	//-----------------------------------------------------------------------
	// NOTIFICACOES HUD (CLIENTE)
	//-----------------------------------------------------------------------

	protected void NotifyClientTransition(string pointId, SPT_EWarfarePointState oldState,
		SPT_EWarfarePointState newState, int manpower, int waveIndex)
	{
		string pointName = GetClientPointName(pointId);
		string message;

		if (newState == SPT_EWarfarePointState.UNDER_ATTACK)
		{
			if (oldState == SPT_EWarfarePointState.FRONTLINE)
				message = string.Format("%1 sob ataque! Reforcos inimigos a caminho.", pointName);
			else
				message = string.Format("%1 sob ataque!", pointName);
		}
		else if (newState == SPT_EWarfarePointState.CLEARED_WAITING)
		{
			message = string.Format("%1: area limpa — permaneca no local para confirmar.", pointName);
		}
		else if (newState == SPT_EWarfarePointState.CAPTURED_DEFENDING)
		{
			message = string.Format("%1 capturada! Prepare a defesa — reforcos chegando.", pointName);
		}
		else if (newState == SPT_EWarfarePointState.CAPTURED)
		{
			message = string.Format("%1 conquistada. Area segura.", pointName);
		}
		else if (newState == SPT_EWarfarePointState.FRONTLINE)
		{
			message = string.Format("%1 disponivel para ataque.", pointName);
		}

		if (!message.IsEmpty())
		{
			SCR_HintManagerComponent hintMgr = SCR_HintManagerComponent.GetInstance();
			if (hintMgr)
				hintMgr.ShowCustom(message);
		}
	}

	//-----------------------------------------------------------------------
	// RESPAWN WARFARE
	//-----------------------------------------------------------------------

	//! Registra todos os HQs e eventuais pontos ja estabilizados.
	protected void InitializeRespawnPoints()
	{
		if (!Replication.IsServer())
			return;

		CleanupRespawnPoints();

		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data)
				continue;

			if (!data.m_bIsHQ && GetPointState(pointId) != SPT_EWarfarePointState.CAPTURED)
				continue;

			EnsureRespawnPoint(pointId);
		}

		Print(string.Format("[SPT_Warfare] Respawn: %1 local(is) disponivel(is) para a faccao %2.",
			m_mRespawnPointIds.Count(), m_sPlayerFactionKey));
	}

	//! Cria uma opcao funcional no menu vanilla e seu ambiente visual.
	protected bool EnsureRespawnPoint(string pointId)
	{
		if (!Replication.IsServer() || !m_mRespawnPointIds || m_mRespawnPointIds.Contains(pointId))
			return false;

		SPT_WarfarePointData data = m_mPointsById.Get(pointId);
		if (!data)
			return false;

		Resource spawnPointResource = Resource.Load(RESPAWN_POINT_PREFAB);
		if (!spawnPointResource)
		{
			Print(string.Format("[SPT_Warfare] Respawn: prefab funcional indisponivel para %1: %2",
				pointId, RESPAWN_POINT_PREFAB), LogLevel.ERROR);
			return false;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		vector position = data.m_vCenter;
		position[1] = world.GetSurfaceY(position[0], position[2]) + 0.1;

		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = position;

		IEntity spawnEntity = GetGame().SpawnEntityPrefab(spawnPointResource, world, spawnParams);
		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.Cast(spawnEntity);
		if (!spawnPoint)
		{
			if (spawnEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(spawnEntity);
			Print(string.Format("[SPT_Warfare] Respawn: prefab funcional nao criou SCR_SpawnPoint para %1.",
				pointId), LogLevel.ERROR);
			return false;
		}

		float spawnRadius = Math.Clamp(data.m_fRadius * 0.25, 10.0, 30.0);
		spawnPoint.SetFactionKey(m_sPlayerFactionKey);
		spawnPoint.SetSpawnPointName(data.m_sDisplayName);
		spawnPoint.SetSpawnRadius(spawnRadius);
		spawnPoint.SetVisibleInDeployMapOnly(true);
		spawnPoint.SetSpawnPointEnabled_S(true);
		m_mRespawnPointIds.Set(pointId, spawnPoint.GetID());

		CreateRespawnEnvironment(pointId, position);

		Print(string.Format("[SPT_Warfare] Respawn disponivel | id=%1 | nome=%2 | faccao=%3 | posicao=%4 | raio=%5",
			pointId, data.m_sDisplayName, m_sPlayerFactionKey, position, spawnRadius));
		return true;
	}

	//! O ambiente e opcional e nunca interfere no spawn point funcional.
	protected void CreateRespawnEnvironment(string pointId, vector position)
	{
		if (m_rRespawnEnvironmentPrefab.IsEmpty() || m_mRespawnEnvironmentIds.Contains(pointId))
			return;

		Resource environmentResource = Resource.Load(m_rRespawnEnvironmentPrefab);
		if (!environmentResource)
		{
			Print(string.Format("[SPT_Warfare] Respawn: ambiente visual indisponivel para %1: %2",
				pointId, m_rRespawnEnvironmentPrefab), LogLevel.WARNING);
			return;
		}

		EntitySpawnParams environmentParams();
		environmentParams.TransformMode = ETransformMode.WORLD;
		environmentParams.Transform[3] = position;

		IEntity environment = GetGame().SpawnEntityPrefab(
			environmentResource,
			GetGame().GetWorld(),
			environmentParams);
		if (!environment)
		{
			Print(string.Format("[SPT_Warfare] Respawn: falha ao criar ambiente visual em %1.",
				pointId), LogLevel.WARNING);
			return;
		}

		m_mRespawnEnvironmentIds.Set(pointId, environment.GetID());
	}

	//! Remove entidades dinamicas para evitar duplicatas em reinicializacoes.
	protected void CleanupRespawnPoints()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		if (m_mRespawnPointIds)
		{
			foreach (string pointId, EntityID spawnPointId : m_mRespawnPointIds)
			{
				IEntity spawnPoint = world.FindEntityByID(spawnPointId);
				if (spawnPoint)
					SCR_EntityHelper.DeleteEntityAndChildren(spawnPoint);
			}
			m_mRespawnPointIds.Clear();
		}

		if (m_mRespawnEnvironmentIds)
		{
			foreach (string environmentPointId, EntityID environmentId : m_mRespawnEnvironmentIds)
			{
				IEntity environment = world.FindEntityByID(environmentId);
				if (environment)
					SCR_EntityHelper.DeleteEntityAndChildren(environment);
			}
			m_mRespawnEnvironmentIds.Clear();
		}
	}

	//-----------------------------------------------------------------------
	// UTILITARIOS
	//-----------------------------------------------------------------------

	protected float HorizontalDistanceSq(vector a, vector b)
	{
		float deltaX = a[0] - b[0];
		float deltaZ = a[2] - b[2];
		return deltaX * deltaX + deltaZ * deltaZ;
	}
}
