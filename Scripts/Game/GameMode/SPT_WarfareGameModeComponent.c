//! Coordenador autoritativo da campanha de conquista territorial SPT Warfare.
//! Componente singleton colocado no GameMode. Responsavel por:
//! - Registrar e validar pontos Warfare (hibrido: automatico + manual)
//! - Manter o grafo territorial e a frente de batalha (BFS)
//! - Gerenciar estados de cada area (LOCKED -> CAPTURED)
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

	//-----------------------------------------------------------------------
	// ATRIBUTOS
	//-----------------------------------------------------------------------

	[Attribute("US", desc: "Chave da faccao dos jogadores (ex: 'US', 'USSR').")]
	protected string m_sPlayerFactionKey;

	[Attribute("USSR", desc: "Chave da faccao inimiga.")]
	protected string m_sEnemyFactionKey;

	[Attribute("2.0", desc: "Intervalo de atualizacao principal em segundos.")]
	protected float m_fUpdateInterval;

	[Attribute("1", desc: "Habilitar descoberta automatica de pontos a partir de descritores de mapa.")]
	protected bool m_bEnableAutoDiscovery;

	[Attribute("150", desc: "Distancia maxima (metros) para vincular um SPT_WarfarePointComponent manual a um descritor automatico.")]
	protected float m_fAutoLinkRadius;

	[Attribute("1", desc: "Exigir presenca de jogador dentro do raio para confirmar captura.")]
	protected bool m_bRequirePlayerPresence;

	[Attribute("1", desc: "Habilitar notificacoes HUD.")]
	protected bool m_bEnableNotifications;

	[Attribute("1", desc: "Habilitar condicao global de vitoria.")]
	protected bool m_bEnableVictoryCondition;

	[Attribute("", desc: "Prefab do icone do mapa para cada tipo de ponto (JSON/Resource). Deixe vazio para usar icones padrao.")]
	protected string m_sMapIconConfigPath;

	[Attribute("0", desc: "Habilitar logs detalhados de diagnostico com prefixo [SPT_Warfare][DEBUG].")]
	protected bool m_bDebug;

	[Attribute("1200", desc: "Distancia maxima (metros) para criar links automaticos entre pontos vizinhos quando nao ha links manuais.")]
	protected float m_fAutoNeighborDistance;

	[Attribute("1", desc: "Habilitar notificacoes HUD de transicoes de estado no cliente.")]
	protected bool m_bEnableHUD;

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

	//! Evento publico disparado quando a campanha e vencida.
	protected ref ScriptInvoker m_OnWarfareVictory = new ScriptInvoker();

	//-----------------------------------------------------------------------
	// MEMBROS (CLIENTE - estado visual)
	//-----------------------------------------------------------------------

	//! Estado de cada ponto no cliente (atualizado via RPC).
	protected ref map<string, SPT_EWarfarePointState> m_mClientPointStates;

	//! Nomes de exibicao dos pontos no cliente.
	protected ref map<string, string> m_mClientPointNames;

	//! Posicoes dos pontos no cliente (recebidas via RpcDo_RegisterPoint).
	protected ref map<string, vector> m_mClientPointPositions;

	//! Ordem estavel dos IDs no cliente.
	protected ref array<string> m_aClientPointOrder;

	//! Flag para evitar notificacao de vitoria duplicada.
	protected bool m_bClientVictoryNotified;

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

		// Inicializa estruturas do cliente
		m_mClientPointStates = new map<string, SPT_EWarfarePointState>();
		m_mClientPointNames = new map<string, string>();
		m_mClientPointPositions = new map<string, vector>();
		m_aClientPointOrder = new array<string>();

		s_Instance = this;

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

		Print("[SPT_Warfare] Inicializando componente de GameMode...");
		GetGame().GetCallqueue().CallLater(InitializeWarfare, 2000, false);
	}

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------
	// INICIALIZACAO
	//-----------------------------------------------------------------------

	protected void InitializeWarfare()
	{
		Print("[SPT_Warfare] Registrando pontos Warfare...");

		// 1. Coleta pontos manuais (SPT_WarfarePointComponent no mundo)
		CollectManualPoints();

		// 2. Se habilitado, coleta pontos automaticos de descritores de mapa
		if (m_bEnableAutoDiscovery)
			CollectAutoPoints();

		// 3. Cruza pontos manuais com locais de guarnicao
		LinkPointsToGarrisonLocations();

		// 3.5 Fallback de teste: auto-HQ e auto-vizinhos se nada foi configurado manualmente
		ApplyTestFallbacks();

		// 4. Inicializa estados (precisa rodar antes da validacao para popular m_aHQIds)
		InitializePointStates();

		// 5. Valida o grafo
		if (!ValidateGraph())
		{
			Print("[SPT_Warfare] ERRO: Validacao do grafo falhou. Warfare desabilitado.", LogLevel.ERROR);
			return;
		}

		// 6. Subscreve aos eventos da guarnicao
		SubscribeToGarrisonEvents();

		// 7. Inicia loops de atualizacao
		int presenceIntervalMs = Math.Round(PLAYER_PRESENCE_CHECK_INTERVAL_S * 1000);
		int stateIntervalMs = Math.Round(STATE_UPDATE_INTERVAL_S * 1000);
		GetGame().GetCallqueue().CallLater(CheckPlayerPresence, presenceIntervalMs, true);
		GetGame().GetCallqueue().CallLater(UpdateWarfareState, stateIntervalMs, true);

		// 8. Envia estado inicial para clientes
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

		SPT_WarfarePointComponent comp = SPT_WarfarePointComponent.Cast(
			entity.FindComponent(SPT_WarfarePointComponent));
		return comp != null;
	}

	protected bool CollectManualPoint(IEntity entity)
	{
		SPT_WarfarePointComponent comp = SPT_WarfarePointComponent.Cast(
			entity.FindComponent(SPT_WarfarePointComponent));
		if (!comp)
			return true;

		RegisterManualPoint(comp, entity);
		return true;
	}

	protected void RegisterManualPoint(SPT_WarfarePointComponent comp, IEntity entity)
	{
		string pointId = comp.GetPointId();
		if (pointId.IsEmpty())
		{
			// Gera ID a partir de contador (EntityID nao tem ToInt em tempo de compilacao)
			pointId = string.Format("MANUAL_%1", m_aPointOrder.Count());
		}

		if (m_mPointsById.Contains(pointId))
		{
			Print(string.Format("[SPT_Warfare] ID duplicado ignorado: %1", pointId), LogLevel.WARNING);
			return;
		}

		SPT_WarfarePointData data = new SPT_WarfarePointData();
		data.m_sPointId = pointId;
		data.m_sDisplayName = comp.GetDisplayName();
		data.m_ePointType = comp.GetPointType();
		data.m_vCenter = entity.GetOrigin();
		data.m_fRadius = comp.GetRadius();
		data.m_bIsHQ = comp.IsHQ();
		data.m_bCountsForVictory = comp.CountsForVictory();
		data.m_bManualOverride = true;

		comp.GetNeighborIds(data.m_aNeighborIds);

		m_mPointsById.Set(pointId, data);
		m_aPointOrder.Insert(pointId);

		Print(string.Format("[SPT_Warfare] Ponto manual registrado | id=%1 | nome=%2 | tipo=%3 | HQ=%4",
			pointId, data.m_sDisplayName, data.m_ePointType, data.m_bIsHQ));
	}

	protected void CollectAutoPoints()
	{
		SPT_WorldGarrisonManagerComponent garrisonMgr = SPT_WorldGarrisonManagerComponent.GetInstance();
		if (!garrisonMgr)
		{
			Print("[SPT_Warfare] SPT_WorldGarrisonManagerComponent nao encontrado. Pontos automaticos ignorados.", LogLevel.WARNING);
			return;
		}

		array<string> locationIds = {};
		garrisonMgr.GetAllLocationIds(locationIds);

		foreach (string locationId : locationIds)
		{
			// Verifica se ja existe um ponto manual que cobre esta localizacao
			if (HasManualPointForLocation(locationId, garrisonMgr))
				continue;

			vector center;
			string name;
			int descType;
			int manpower, targetManpower, pendingSpawns;
			bool cleared, battleActive;
			int budget, waveIndex, waveCount;

			if (!garrisonMgr.GetLocationState(locationId, center, name, descType,
				manpower, targetManpower, pendingSpawns, cleared, budget,
				battleActive, waveIndex, waveCount))
				continue;

			RegisterAutoPoint(locationId, name, descType, center);
		}

		Print(string.Format("[SPT_Warfare] Pontos automaticos registrados a partir de %1 locais de guarnicao.",
			locationIds.Count()));
	}

	protected bool HasManualPointForLocation(string locationId, SPT_WorldGarrisonManagerComponent garrisonMgr)
	{
		vector locCenter;
		string locName;
		int locType, m1, m2, m3;
		bool c, b;
		int b1, b2, b3;
		if (!garrisonMgr.GetLocationState(locationId, locCenter, locName, locType, m1, m2, m3, c, b1, b, b2, b3))
			return false;

		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data || !data.m_bManualOverride)
				continue;

			float distSq = HorizontalDistanceSq(data.m_vCenter, locCenter);
			float linkRadius = m_fAutoLinkRadius;
			if (linkRadius < 1)
				linkRadius = 150;

			if (distSq <= linkRadius * linkRadius)
				return true;
		}

		return false;
	}

	protected void RegisterAutoPoint(string locationId, string name, int descriptorType, vector center)
	{
		if (m_mPointsById.Contains(locationId))
			return;

		SPT_WarfarePointData data = new SPT_WarfarePointData();
		data.m_sPointId = locationId;
		data.m_sDisplayName = name;
		data.m_ePointType = DescriptorTypeToWarfareType(descriptorType);
		data.m_vCenter = center;
		data.m_fRadius = 150.0;
		data.m_bManualOverride = false;
		data.m_sGarrisonLocationId = locationId;

		m_mPointsById.Set(locationId, data);
		m_aPointOrder.Insert(locationId);

		Print(string.Format("[SPT_Warfare] Ponto automatico registrado | id=%1 | nome=%2 | tipo=%3",
			locationId, name, data.m_ePointType));
	}

	protected SPT_EWarfarePointType DescriptorTypeToWarfareType(int descriptorType)
	{
		if (descriptorType == EMapDescriptorType.MDT_NAME_CITY)
			return SPT_EWarfarePointType.CITY;
		if (descriptorType == EMapDescriptorType.MDT_NAME_VILLAGE || descriptorType == EMapDescriptorType.MDT_NAME_SETTLEMENT)
			return SPT_EWarfarePointType.VILLAGE;
		if (descriptorType == EMapDescriptorType.MDT_NAME_TOWN)
			return SPT_EWarfarePointType.VILLAGE;
		if (descriptorType == EMapDescriptorType.MDT_BASE)
			return SPT_EWarfarePointType.MILITARY_BASE;
		if (descriptorType == EMapDescriptorType.MDT_AIRPORT)
			return SPT_EWarfarePointType.AIRPORT;
		if (descriptorType == EMapDescriptorType.MDT_PORT)
			return SPT_EWarfarePointType.PORT;
		return SPT_EWarfarePointType.CUSTOM;
	}

	//-----------------------------------------------------------------------
	// VINCULACAO COM GUARNICAO
	//-----------------------------------------------------------------------

	protected void LinkPointsToGarrisonLocations()
	{
		SPT_WorldGarrisonManagerComponent garrisonMgr = SPT_WorldGarrisonManagerComponent.GetInstance();
		if (!garrisonMgr)
			return;

		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data)
				continue;

			// Pontos automaticos ja tem o garrisonLocationId definido
			if (!data.m_sGarrisonLocationId.IsEmpty())
				continue;

			// Para pontos manuais sem garrisonLocationId, encontra o local de
			// guarnicao mais proximo
			string nearestLocId = FindNearestGarrisonLocation(data.m_vCenter, garrisonMgr);
			if (!nearestLocId.IsEmpty())
				data.m_sGarrisonLocationId = nearestLocId;
		}
	}

	protected string FindNearestGarrisonLocation(vector position, SPT_WorldGarrisonManagerComponent garrisonMgr)
	{
		array<string> locationIds = {};
		garrisonMgr.GetAllLocationIds(locationIds);

		string bestId;
		float bestDistSq = float.MAX;

		foreach (string locationId : locationIds)
		{
			vector center;
			string name;
			int descType, m1, m2, m3;
			bool c, b;
			int b1, b2, b3;

			if (!garrisonMgr.GetLocationState(locationId, center, name, descType, m1, m2, m3, c, b1, b, b2, b3))
				continue;

			float distSq = HorizontalDistanceSq(position, center);
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestId = locationId;
			}
		}

		return bestId;
	}

	//-----------------------------------------------------------------------
	// FALLBACKS DE TESTE (auto-HQ + auto-vizinhos)
	//-----------------------------------------------------------------------

	//! Aplica configuracoes automaticas para permitir teste rapido sem
	//! configuracao manual no editor. Se nenhum HQ foi definido, o primeiro
	//! ponto vira HQ. Se nao ha links manuais, pontos proximos sao linkados.
	protected void ApplyTestFallbacks()
	{
		// Auto-HQ: se nenhum ponto manual tem HQ marcada, usa o primeiro ponto
		bool hasHQ = false;
		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (data && data.m_bIsHQ)
			{
				hasHQ = true;
				break;
			}
		}

		if (!hasHQ && m_aPointOrder.Count() > 0)
		{
			string firstId = m_aPointOrder[0];
			SPT_WarfarePointData firstData = m_mPointsById.Get(firstId);
			if (firstData)
			{
				firstData.m_bIsHQ = true;
				Print(string.Format("[SPT_Warfare] AUTO-HQ: Nenhum HQ configurado. Primeiro ponto definido como HQ: %1 (%2)",
					firstId, firstData.m_sDisplayName));
			}
		}

		// Auto-vizinhos: se nenhum ponto tem links manuais, cria links por proximidade
		bool hasManualLinks = false;
		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (data && !data.m_aNeighborIds.IsEmpty())
			{
				hasManualLinks = true;
				break;
			}
		}

		if (!hasManualLinks && m_aPointOrder.Count() > 1 && m_fAutoNeighborDistance > 0)
		{
			Print("[SPT_Warfare] AUTO-VIZINHOS: Nenhum link manual encontrado. Criando links por proximidade...");
			float maxDistSq = m_fAutoNeighborDistance * m_fAutoNeighborDistance;

			for (int i = 0; i < m_aPointOrder.Count(); i++)
			{
				for (int j = i + 1; j < m_aPointOrder.Count(); j++)
				{
					SPT_WarfarePointData dataA = m_mPointsById.Get(m_aPointOrder[i]);
					SPT_WarfarePointData dataB = m_mPointsById.Get(m_aPointOrder[j]);
					if (!dataA || !dataB)
						continue;

					float distSq = HorizontalDistanceSq(dataA.m_vCenter, dataB.m_vCenter);
					if (distSq <= maxDistSq)
					{
						dataA.m_aNeighborIds.Insert(dataB.m_sPointId);
						dataB.m_aNeighborIds.Insert(dataA.m_sPointId);
						DebugLog(string.Format("Link automatico: %1 <-> %2 (dist=%3m)",
							dataA.m_sPointId, dataB.m_sPointId, Math.Sqrt(distSq)));
					}
				}
			}
		}
	}

	//! Log de debug condicional.
	protected void DebugLog(string message)
	{
		if (!m_bDebug)
			return;
		Print("[SPT_Warfare][DEBUG] " + message);
	}

	//-----------------------------------------------------------------------
	// VALIDACAO DO GRAFO
	//-----------------------------------------------------------------------

	protected bool ValidateGraph()
	{
		bool valid = true;

		// Verifica HQs
		if (m_aHQIds.IsEmpty())
		{
			Print("[SPT_Warfare] ERRO: Nenhum ponto definido como HQ.", LogLevel.ERROR);
			valid = false;
		}

		// Verifica IDs duplicados e vizinhos inexistentes
		foreach (string pointId : m_aPointOrder)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data)
				continue;

			// Verifica se ha link para o proprio ponto
			if (data.m_aNeighborIds.Find(pointId) >= 0)
			{
				Print(string.Format("[SPT_Warfare] AVISO: Ponto %1 tem link para si mesmo. Removendo.",
					pointId), LogLevel.WARNING);
				data.m_aNeighborIds.RemoveItem(pointId);
			}

			// Verifica vizinhos inexistentes
			for (int i = data.m_aNeighborIds.Count() - 1; i >= 0; i--)
			{
				if (!m_mPointsById.Contains(data.m_aNeighborIds[i]))
				{
					Print(string.Format("[SPT_Warfare] AVISO: Vizinho '%1' do ponto %2 nao existe. Removendo.",
						data.m_aNeighborIds[i], pointId), LogLevel.WARNING);
					data.m_aNeighborIds.Remove(i);
				}
			}
		}

		// Verifica pontos inalcancaveis a partir das HQs (BFS)
		foreach (string pointId : m_aPointOrder)
		{
			if (m_aHQIds.Find(pointId) >= 0)
				continue;

			if (!IsReachableFromHQ(pointId))
			{
				Print(string.Format("[SPT_Warfare] AVISO: Ponto %1 (%2) nao e alcancavel a partir de nenhuma HQ.",
					pointId, GetPointDisplayName(pointId)), LogLevel.WARNING);
				// Nao e erro fatal - o ponto fica LOCKED para sempre
			}
		}

		return valid;
	}

	//! BFS simples para verificar se um ponto e alcancavel a partir de qualquer HQ.
	protected bool IsReachableFromHQ(string targetId)
	{
		if (m_aHQIds.Find(targetId) >= 0)
			return true;

		ref map<string, bool> visited = new map<string, bool>();
		ref array<string> queue = new array<string>();

		foreach (string hqId : m_aHQIds)
		{
			queue.Insert(hqId);
			visited.Set(hqId, true);
		}

		while (queue.Count() > 0)
		{
			string current = queue[0];
			queue.Remove(0);

			SPT_WarfarePointData data = m_mPointsById.Get(current);
			if (!data)
				continue;

			foreach (string neighborId : data.m_aNeighborIds)
			{
				if (visited.Contains(neighborId))
					continue;

				if (neighborId == targetId)
					return true;

				visited.Set(neighborId, true);
				queue.Insert(neighborId);
			}
		}

		return false;
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
				m_mPointStates.Set(pointId, SPT_EWarfarePointState.LOCKED);
				m_mPointCaptured.Set(pointId, false);
			}
		}

		// Calcula a frente inicial
		RecalculateFrontline();
	}

	//-----------------------------------------------------------------------
	// GRAFO TERRITORIAL - BFS DA FRENTE
	//-----------------------------------------------------------------------

	//! Recalcula quais pontos estao na frente (FRONTLINE).
	//! Um ponto inimigo e FRONTLINE se e vizinho de pelo menos um ponto capturado.
	protected void RecalculateFrontline()
	{
		m_aFrontlineIds.Clear();

		foreach (string pointId : m_aPointOrder)
		{
			if (IsPointCaptured(pointId))
				continue;

			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (!data)
				continue;

			// Verifica se tem vizinho capturado
			foreach (string neighborId : data.m_aNeighborIds)
			{
				if (IsPointCaptured(neighborId))
				{
					m_aFrontlineIds.Insert(pointId);
					ChangePointState(pointId, SPT_EWarfarePointState.FRONTLINE);
					break;
				}
			}

			// Se nao tem vizinhos capturados, permanece LOCKED
			if (m_aFrontlineIds.Find(pointId) < 0)
			{
				SPT_EWarfarePointState currentState = GetPointState(pointId);
				if (currentState != SPT_EWarfarePointState.LOCKED)
					ChangePointState(pointId, SPT_EWarfarePointState.LOCKED);
			}
		}

		Print(string.Format("[SPT_Warfare] Frente recalculada | pontosNaFrente=%1 | totalCapturados=%2",
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

		// Inicia monitoramento para pontos na frente
		foreach (string pointId : m_aFrontlineIds)
		{
			SPT_WarfarePointData data = m_mPointsById.Get(pointId);
			if (data && !data.m_sGarrisonLocationId.IsEmpty())
				garrisonMgr.StartMonitoringLocation(data.m_sGarrisonLocationId);
		}
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

		SPT_EWarfarePointState currentState = GetPointState(pointId);

		// Verifica se ha jogador presente no raio
		bool playerPresent = IsPlayerInRadius(pointId);
		bool isConnected = IsPointFrontline(pointId) || IsPointCapturedByNeighbor(pointId);

		if (playerPresent && isConnected)
		{
			// Captura imediata
			CapturePoint(pointId);
		}
		else
		{
			// Aguardando presenca ou conexao
			ChangePointState(pointId, SPT_EWarfarePointState.CLEARED_WAITING);

			if (!playerPresent)
				Print(string.Format("[SPT_Warfare] Aguardando jogador | ponto=%1", pointId));
			if (!isConnected)
				Print(string.Format("[SPT_Warfare] Aguardando conexao | ponto=%1", pointId));
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
			RecalculateFrontline();
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

		// Recalcula a frente para desbloquear novos vizinhos
		RecalculateFrontline();

		// Inicia monitoramento nos novos pontos da frente
		StartMonitoringNewFrontlinePoints(garrisonMgr);

		// Verifica vitoria
		if (newState == SPT_EWarfarePointState.CAPTURED)
			CheckVictory();
	}

	protected void StartMonitoringNewFrontlinePoints(SPT_WorldGarrisonManagerComponent garrisonMgr)
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

			// Jogador presente + conexao = captura
			if (IsPointFrontline(pointId) || IsPointCapturedByNeighbor(pointId))
			{
				CapturePoint(pointId);
			}
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

	bool IsPointCapturedByNeighbor(string pointId)
	{
		SPT_WarfarePointData data = m_mPointsById.Get(pointId);
		if (!data)
			return false;

		foreach (string neighborId : data.m_aNeighborIds)
		{
			if (IsPointCaptured(neighborId))
				return true;
		}
		return false;
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
		return m_aPointOrder.Count();
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
		foreach (string pointId : m_aClientPointOrder)
			outIds.Insert(pointId);
	}

	//-----------------------------------------------------------------------
	// REPLICACAO DE ESTADO
	//-----------------------------------------------------------------------

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

			RpcDo_PointStateChanged(pointId, stateRaw, capturedRaw, 0, -1);
			Rpc(RpcDo_PointStateChanged, pointId, stateRaw, capturedRaw, 0, -1);
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

	//! RPC de registro do ponto (nome, posicao, tipo) - enviado no estado inicial.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterPoint(string pointId, string name, int typeRaw, float posX, float posZ, int isHQ)
	{
		m_mClientPointNames.Set(pointId, name);
		m_mClientPointPositions.Set(pointId, Vector(posX, 0, posZ));
		if (m_aClientPointOrder.Find(pointId) < 0)
			m_aClientPointOrder.Insert(pointId);
	}

	//! RPC simples com tipos basicos: string, int, int (bool como 0/1).
	//! Clientes processam a atualizacao de estado de um ponto.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_PointStateChanged(string pointId, int stateRaw, int capturedRaw, int manpower, int waveIndex)
	{
		SPT_EWarfarePointState newState = stateRaw;

		// Obtem estado anterior para detectar transicao
		SPT_EWarfarePointState oldState = SPT_EWarfarePointState.LOCKED;
		if (m_mClientPointStates.Contains(pointId))
			oldState = m_mClientPointStates.Get(pointId);

		// Atualiza estado local
		m_mClientPointStates.Set(pointId, newState);

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
	// UTILITARIOS
	//-----------------------------------------------------------------------

	protected float HorizontalDistanceSq(vector a, vector b)
	{
		float deltaX = a[0] - b[0];
		float deltaZ = a[2] - b[2];
		return deltaX * deltaX + deltaZ * deltaZ;
	}
}
