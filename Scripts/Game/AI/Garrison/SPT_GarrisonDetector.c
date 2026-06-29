// data da versao 06-14-26
// SPT AI Garrison – detector de postos de guarnicao em construcoes

// Um posto finalizado para posicionar um soldado: onde ficar e para qual direcao olhar
class SPT_GarrisonPost
{
	vector m_Pos;
	vector m_Facing;

	void SPT_GarrisonPost(vector pos, vector facing)
	{
		m_Pos = pos;
		m_Facing = facing;
	}
}

// Uma abertura que vale a pena vigiar; aberturas externas (exterior) olham para fora,
// aberturas de rota (route) sao caminhos internos como escadas
class SPT_GarrisonOpening
{
	vector m_Pos;
	vector m_Normal;
	bool m_Exterior;
	bool m_Route;

	void SPT_GarrisonOpening(vector pos, vector normal)
	{
		m_Pos = pos;
		m_Normal = normal;
		m_Exterior = false;
		m_Route = false;
	}
}

//! Registro de construcao candidata a CQB com pontuacao de qualidade.
//! Usado pelo detector para ordenar construcoes da melhor para a pior,
//! priorizando edificios com varios andares, janelas, escadas e terraços.
class SPT_BuildingCQBRecord
{
	//! Centro da casca estrutural no mundo
	vector m_vCenter;
	//! Pontuacao de qualidade CQB (maior = melhor)
	float m_fScore;
	//! Volume da bounding box (m³) – usado como metrica confiavel de tamanho
	float m_fVolumeM3;

	void SPT_BuildingCQBRecord(vector center, float score, float volumeM3)
	{
		m_vCenter = center;
		m_fScore = score;
		m_fVolumeM3 = volumeM3;
	}
}

// Detector de postos de guarnicao: analisa construcoes, encontra posicoes validas no interior,
// distribui soldados e atribui direcoes de vigilancia com base em portas, janelas e rotas internas
class SPT_GarrisonDetector
{
	// --- Constantes de busca e grade ---
	protected const float BUILDING_SEARCH_RADIUS = 25.0;    // raio de busca por construcoes ao redor do centro
	protected const float GRID_SPACING_M         = 1.5;     // espacamento da grade de amostragem horizontal
	protected const float Y_SLICE_SPACING_M      = 2.5;     // espacamento vertical entre fatias de piso
	protected const float FLOOR_OFFSET_M         = 1.0;     // deslocamento acima do piso para iniciar a amostra
	protected const vector NAVMESH_SEARCH_HALF   = "2.5 1.5 2.5"; // meia-extensao de busca no navmesh
	protected const float  SNAP_DEDUP_SQ         = 0.25;    // tolerancia quadratica para deduplicacao de snaps
	protected const float ROOF_PROBE_HEIGHT_M    = 50.0;    // altura do raio de teste de teto
	protected const float ROOF_PROBE_START_M     = 0.3;     // deslocamento inicial do raio de teto
	protected const float DOOR_CLEARANCE_M       = 2.0;     // distancia minima de uma porta
	protected const float OCCUPIED_CLEARANCE_M   = 2.0;     // distancia minima de posto ja ocupado
	protected const float DOOR_FLOOR_TOL_M       = 2.0;     // tolerancia vertical porta-piso
	protected const float CONNECT_DIST_M         = 2.2;     // distancia maxima de conexao entre pontos (union-find)
	protected const float CONNECT_Y_M            = 3.5;     // tolerancia vertical para conexao entre pontos
	protected const float FLOOR_WEIGHT           = 2.5;     // peso do eixo Y na distancia ponderada
	protected const int   FPS_JITTER_K           = 3;       // fator de aleatoriedade na selecao farthest-point
	protected const float MIN_POST_SPACING_M     = 1.8;     // espacamento minimo entre postos

	// --- Constantes de linha de visao e vigilancia ---
	protected const float LOS_H_M                = 1.5;     // altura do olho para teste de linha de visao
	protected const float LOS_STOP_SHORT_M       = 0.7;     // distancia de parada antes do alvo no trace LOS
	protected const float EXT_RAY_M              = 3.0;     // comprimento do raio de teste de abertura externa
	protected const float MAX_WATCH_M            = 25.0;    // distancia maxima de vigilancia
	protected const float W_EXT                  = 1.0;     // peso para abertura externa
	protected const float W_APPROACH             = 0.6;     // peso para alinhamento com direcao de aproximacao
	protected const float W_NEAR                 = 0.4;     // peso para proximidade
	protected const float W_ROUTE                = 0.65;    // peso para abertura de rota (escada)

	// --- Constantes de deteccao de rotas (escadas) ---
	protected const float ROUTE_HDIST_M          = 2.0;     // distancia horizontal maxima entre degraus
	protected const float ROUTE_HDIST_MIN_M      = 0.6;     // distancia horizontal minima entre degraus
	protected const float ROUTE_STEP_MIN_M       = 0.4;     // altura minima de um degrau
	protected const float ROUTE_STEP_MAX_M       = 2.5;     // altura maxima de um degrau
	protected const float ROUTE_DEDUP_M          = 4.5;     // raio de deduplicacao de rotas
	protected const float ROUTE_CLAIM_M          = 3.0;     // raio de reivindicacao de rota por um soldado

	// --- Constantes de validacao de piso e espaco ---
	protected const float UNDERGROUND_SLOP_M     = 0.5;     // tolerancia para considerar ponto como subterraneo
	protected const float FLOOR_PROBE_M          = 0.6;     // profundidade do raio de sondagem de piso
	protected const float MAX_FLOOR_GAP_M        = 0.35;    // distancia maxima entre ponto e piso real
	protected const float MIN_HEADROOM_M         = 1.8;     // altura minima livre acima do ponto
	protected const float CLEAR_RADIUS_M         = 0.4;     // raio de espaco livre ao redor do soldado
	protected const float CHEST_H_M              = 1.2;     // altura do peito para verificacao de obstrucao
	protected const float ENCLOSE_RAY_M          = 16.0;    // comprimento do raio de verificacao de paredes ao redor
	protected const float ENCLOSE_H_M            = 1.5;     // altura dos raios de fechamento (paredes)
	protected const int   ENCLOSE_MIN_HITS       = 4;       // numero minimo de paredes ao redor para considerar fechado
	// Folga nas caixas internas autorais para que amostras na borda ainda contem como dentro
	protected const float INTERIOR_MARGIN_M      = 0.3;

	// Quando true, usa as caixas delimitadoras internas (OBB) como porteiro adicional de validacao
	protected const bool  USE_INTERIOR_OBB_GATE  = false;

	// --- Constantes de qualidade de construcao para CQB ---
	// Filtro minimo para descartar toneis, destroços e props muito pequenos
	protected const float MIN_CQB_BUILDING_VOLUME_M3      = 16.0;   // volume minimo (m³) – toneis e props ficam abaixo
	protected const float MIN_CQB_BUILDING_HEIGHT_M       = 2.0;    // altura minima (m) – menos que isso nao cabe um soldado em pe
	protected const float MIN_CQB_BUILDING_HORIZONTAL_M   = 1.8;    // extensao horizontal minima (m) em X ou Z

	// Pesos da pontuacao de qualidade CQB (todos somam para produzir o score final)
	protected const float SCORE_VOLUME_WEIGHT             = 2.5;    // peso do volume (construcoes maiores = melhores)
	protected const float SCORE_HEIGHT_WEIGHT             = 3.0;    // peso da altura (proxy para andares; mais andares = melhor CQB)
	protected const float SCORE_DOOR_WEIGHT               = 0.8;    // peso por porta encontrada (mais portas = mais caminhos)
	protected const float SCORE_WINDOW_WEIGHT             = 0.5;    // peso por janela encontrada (mais janelas = mais angulos)
	protected const float SCORE_TOWER_BONUS               = 6.0;    // bonus para torres de vigia e guaritas
	protected const float SCORE_BUNKER_BONUS              = 4.0;    // bonus para bunkers/pillboxes
	protected const float SCORE_LARGE_BUILDING_BONUS      = 4.0;    // bonus extra para construcoes muito grandes (> 500 m³)
	protected const float SCORE_GARAGE_PENALTY            = -5.0;   // penalidade para garagens e galpoes abertos
	protected const float SCORE_ROOFTOP_BONUS             = 3.0;    // bonus para construcoes com terraço acessivel
	protected const float SCORE_SMALL_BUILDING_PENALTY    = -2.0;   // penalidade para construcoes muito pequenas (< 40 m³)

	// Limiares de categoria para classificacao rapida
	protected const float LARGE_BUILDING_VOLUME_M3        = 500.0;  // acima disso = construcao grande
	protected const float SMALL_BUILDING_VOLUME_M3        = 40.0;   // abaixo disso = construcao muito pequena
	protected const float TOWER_MIN_HEIGHT_M              = 4.0;    // altura minima para considerar torre (proxy)

	protected static ref array<IEntity> s_QueryAccumulator;

	static ref array<ref SPT_GarrisonOpening> s_DebugOpenings;
	static ref array<vector> s_DebugContained;
	static string s_DebugSummary;

	// Expõe a descoberta de construcoes do detector para guarnicoes dinamicas no mundo.
	// Os resultados sao centros de casca estrutural com duplicatas removidas.
	static void CollectBuildingCenters(
		BaseWorld world,
		vector center,
		float radius,
		out array<vector> outCenters)
	{
		outCenters = {};
		if (!world || radius <= 0)
			return;

		array<IEntity> buildings = {};
		s_QueryAccumulator = buildings;
		world.QueryEntitiesBySphere(center, radius, BuildingFilterCallback, null, EQueryEntitiesFlags.STATIC);
		s_QueryAccumulator = null;

		foreach (IEntity building : buildings)
		{
			IEntity shell = ResolveStructuralShell(building);
			if (!shell)
				continue;

			vector mins, maxs;
			shell.GetWorldBounds(mins, maxs);

			// Filtra toneis, props e destroços muito pequenos
			if (!IsValidBuildingForCQB(mins, maxs))
				continue;

			vector shellCenter = (mins + maxs) * 0.5;

			bool duplicate = false;
			foreach (vector existingCenter : outCenters)
			{
				if (HorizontalDistSq(existingCenter, shellCenter) < 4.0)
				{
					duplicate = true;
					break;
				}
			}

			if (!duplicate)
				outCenters.Insert(shellCenter);
		}
	}

	//! Expõe uma lista de construcoes pontuadas e ordenadas por qualidade CQB.
	//! Filtra toneis, props e predios muito pequenos automaticamente.
	//! Construcoes com multiplos andares, janelas, escadas e torres de vigia
	//! recebem pontuacao mais alta e aparecem primeiro no resultado.
	static void CollectCQBBuildingCenters(
		BaseWorld world,
		vector center,
		float radius,
		out array<ref SPT_BuildingCQBRecord> outRecords)
	{
		outRecords = {};
		if (!world || radius <= 0)
			return;

		array<IEntity> buildings = {};
		s_QueryAccumulator = buildings;
		world.QueryEntitiesBySphere(center, radius, BuildingFilterCallback, null, EQueryEntitiesFlags.STATIC);
		s_QueryAccumulator = null;

		foreach (IEntity building : buildings)
		{
			IEntity shell = ResolveStructuralShell(building);
			if (!shell)
				continue;

			vector mins, maxs;
			shell.GetWorldBounds(mins, maxs);

			// Filtra toneis, props e destroços muito pequenos
			if (!IsValidBuildingForCQB(mins, maxs))
				continue;

			vector shellCenter = (mins + maxs) * 0.5;

			// Deduplica por proximidade do centro
			bool duplicate = false;
			foreach (SPT_BuildingCQBRecord existing : outRecords)
			{
				if (HorizontalDistSq(existing.m_vCenter, shellCenter) < 4.0)
				{
					duplicate = true;
					break;
				}
			}
			if (duplicate)
				continue;

			float volume = GetBoundingVolume(mins, maxs);
			float score = GetBuildingCQBScore(shell, mins, maxs);

			outRecords.Insert(new SPT_BuildingCQBRecord(shellCenter, score, volume));
		}

		// Ordena da melhor construcao (maior score) para a pior
		SortBuildingRecords(outRecords);
	}

	//! Verifica se as dimensoes da bounding box sao compativeis com uma
	//! construcao real onde um soldado pode ficar em pe e se movimentar.
	//! Rejeita toneis, props de cenario e destroços muito pequenos.
	protected static bool IsValidBuildingForCQB(vector mins, vector maxs)
	{
		float sizeX = maxs[0] - mins[0];
		float sizeY = maxs[1] - mins[1];
		float sizeZ = maxs[2] - mins[2];

		// Altura minima: o soldado precisa ficar em pe
		if (sizeY < MIN_CQB_BUILDING_HEIGHT_M)
			return false;

		// Extensao horizontal minima: precisa ser mais largo que um tonel
		if (sizeX < MIN_CQB_BUILDING_HORIZONTAL_M && sizeZ < MIN_CQB_BUILDING_HORIZONTAL_M)
			return false;

		// Volume minimo: filtra toneis, cadeiras e props de cenario
		float volume = sizeX * sizeY * sizeZ;
		if (volume < MIN_CQB_BUILDING_VOLUME_M3)
			return false;

		return true;
	}

	//! Calcula o volume da bounding box em metros cubicos
	protected static float GetBoundingVolume(vector mins, vector maxs)
	{
		float sizeX = maxs[0] - mins[0];
		float sizeY = maxs[1] - mins[1];
		float sizeZ = maxs[2] - mins[2];
		return sizeX * sizeY * sizeZ;
	}

	//! Pontua uma construcao para CQB com base em dimensoes, portas, janelas e
	//! heuristicas de nome do prefab. Retorna um score onde valores maiores
	//! indicam local melhor para combate em ambientes fechados.
	protected static float GetBuildingCQBScore(IEntity shell, vector mins, vector maxs)
	{
		float sizeX = maxs[0] - mins[0];
		float sizeY = maxs[1] - mins[1];
		float sizeZ = maxs[2] - mins[2];
		float volume = sizeX * sizeY * sizeZ;
		float horizontalArea = sizeX * sizeZ;

		// --- Score baseado em tamanho ---
		// Volume: usa raiz cubica para achatar a curva (diferenca entre 50m³ e 500m³
		// importa mais que entre 500m³ e 5000m³)
		float volumeScore = Math.Sqrt(Math.Sqrt(volume)) * SCORE_VOLUME_WEIGHT;

		// Altura: proxy para numero de andares (cada 2.5m ≈ 1 andar)
		float heightScore = (sizeY / Y_SLICE_SPACING_M) * SCORE_HEIGHT_WEIGHT;

		// --- Score baseado em componentes (portas e janelas) ---
		int doorCount = CountDoorsInShell(shell);
		int windowCount = CountWindowsInShell(shell);
		float componentScore = doorCount * SCORE_DOOR_WEIGHT + windowCount * SCORE_WINDOW_WEIGHT;

		// --- Heuristicas por nome do prefab ---
		float nameScore = ClassifyBuildingByPrefabName(shell);

		// --- Bonus de terraço ---
		float rooftopScore = 0.0;
		if (HasRooftopPotential(mins, maxs, volume, horizontalArea))
			rooftopScore = SCORE_ROOFTOP_BONUS;

		// --- Bonus/penalidade por categoria de tamanho ---
		float categoryScore = 0.0;
		if (volume >= LARGE_BUILDING_VOLUME_M3)
			categoryScore = SCORE_LARGE_BUILDING_BONUS;
		else if (volume < SMALL_BUILDING_VOLUME_M3)
			categoryScore = SCORE_SMALL_BUILDING_PENALTY;

		// Soma ponderada final
		float score = volumeScore + heightScore + componentScore + nameScore + rooftopScore + categoryScore;

		return score;
	}

	//! Conta portas (BaseDoorComponent) na hierarquia da construcao.
	//! Caminha ate profundidade 10, igual ao WalkForDoors.
	protected static int CountDoorsInShell(IEntity shell)
	{
		int count = 0;
		CountDoorsRecursive(shell, count, 0);
		return count;
	}

	//! Percorre recursivamente os filhos contando portas
	protected static void CountDoorsRecursive(IEntity entity, out int count, int depth)
	{
		if (!entity || depth > 10)
			return;
		BaseDoorComponent door = BaseDoorComponent.Cast(entity.FindComponent(BaseDoorComponent));
		if (door)
			count++;

		IEntity child = entity.GetChildren();
		while (child)
		{
			CountDoorsRecursive(child, count, depth + 1);
			child = child.GetSibling();
		}
	}

	//! Conta janelas (SCR_WindowHitZone) na hierarquia da construcao.
	//! Caminha ate profundidade 10, igual ao WalkForOpenings.
	protected static int CountWindowsInShell(IEntity shell)
	{
		int count = 0;
		CountWindowsRecursive(shell, count, 0);
		return count;
	}

	//! Percorre recursivamente os filhos contando janelas
	protected static void CountWindowsRecursive(IEntity entity, out int count, int depth)
	{
		if (!entity || depth > 10)
			return;

		HitZoneContainerComponent hzc = HitZoneContainerComponent.Cast(entity.FindComponent(HitZoneContainerComponent));
		if (hzc)
		{
			array<HitZone> hitzones = {};
			hzc.GetAllHitZones(hitzones);
			foreach (HitZone hz : hitzones)
			{
				SCR_WindowHitZone win = SCR_WindowHitZone.Cast(hz);
				if (win)
					count++;
			}
		}

		IEntity child = entity.GetChildren();
		while (child)
		{
			CountWindowsRecursive(child, count, depth + 1);
			child = child.GetSibling();
		}
	}

	//! Avalia se a construcao tem potencial de terraço acessivel.
	//! Considera o volume construido vs area horizontal: se o volume e
	//! pequeno em relacao a area (altura media baixa) e a construcao e
	//! grande o suficiente, provavelmente tem um terraço plano no topo.
	protected static bool HasRooftopPotential(vector mins, vector maxs, float volume, float horizontalArea)
	{
		// Precisa ter area horizontal significativa
		if (horizontalArea < 25.0)
			return false;

		// Altura media = volume / area. Se for baixa (1-2 andares) mas a area
		// for grande, provavelmente e um terraço.
		float avgHeight = volume / horizontalArea;

		// Terraços tipicos: area grande com altura media entre 2.5m e 6m
		// (1 a 2 andares com cobertura plana)
		if (avgHeight >= 2.0 && avgHeight <= 6.0 && horizontalArea >= 30.0)
			return true;

		// Construcoes muito altas mas com topo grande tambem podem ter heliponto/terraço
		float sizeY = maxs[1] - mins[1];
		if (sizeY >= 6.0 && horizontalArea >= 50.0)
			return true;

		return false;
	}

	//! Classifica a construcao com base no caminho do prefab.
	//! Retorna bonus positivo para torres, bunkers e predios bons,
	//! e penalidade para garagens, galpoes e construcoes ruins.
	protected static float ClassifyBuildingByPrefabName(IEntity shell)
	{
		if (!shell)
			return 0.0;

		EntityPrefabData data = shell.GetPrefabData();
		if (!data)
			return 0.0;

		string path = data.GetPrefabName();
		if (path.IsEmpty())
			return 0.0;

		// --- Bonus alto: torres de vigia e guaritas ---
		// Nota: caminhos de prefab no Arma sao sempre lowercase
		if (path.Contains("tower") || path.Contains("watchtower") || path.Contains("guard_tower")
			|| path.Contains("watch_tower") || path.Contains("guardhouse") || path.Contains("sentry"))
		{
			return SCORE_TOWER_BONUS;
		}

		// --- Bonus medio: bunkers e pillboxes ---
		if (path.Contains("bunker") || path.Contains("pillbox") || path.Contains("fortification")
			|| path.Contains("guard_post") || path.Contains("machinegun_nest"))
		{
			return SCORE_BUNKER_BONUS;
		}

		// --- Penalidade: garagens, galpoes e construcoes abertas ---
		if (path.Contains("garage") || path.Contains("carport") || path.Contains("shed")
			|| path.Contains("hangar"))
		{
			return SCORE_GARAGE_PENALTY;
		}

		// --- Penalidade leve: banheiros, quiosques, abrigos minusculos ---
		if (path.Contains("toilet") || path.Contains("outhouse") || path.Contains("kiosk"))
		{
			return SCORE_SMALL_BUILDING_PENALTY;
		}

		// --- Bonus leve: construcoes boas para CQB ---
		if (path.Contains("barracks") || path.Contains("headquarters") || path.Contains("police")
			|| path.Contains("military") || path.Contains("armoury") || path.Contains("command"))
		{
			return SCORE_BUNKER_BONUS * 0.5;
		}

		return 0.0;
	}

	//! Ordena registros de construcao por pontuacao decrescente (melhor primeiro).
	//! Usa ordenacao por insercao simples – o numero de construcoes por local e pequeno.
	protected static void SortBuildingRecords(notnull array<ref SPT_BuildingCQBRecord> records)
	{
		int n = records.Count();
		if (n <= 1)
			return;

		for (int i = 1; i < n; i++)
		{
			SPT_BuildingCQBRecord key = records[i];
			float keyScore = key.m_fScore;
			int j = i - 1;

			while (j >= 0 && records[j].m_fScore < keyScore)
			{
				records[j + 1] = records[j];
				j = j - 1;
			}
			records[j + 1] = key;
		}
	}

	// Pipeline completo: coleta construcoes, amostra navmesh, valida posicoes, distribui postos
	// e atribui direcoes de vigilancia para cada soldado
	static SPT_EGarrisonDetectResult DetectPosts(
		BaseWorld world,
		vector center,
		AIPathfindingComponent pathing,
		int desiredCount,
		notnull array<vector> excluded,
		out array<ref SPT_GarrisonPost> outPosts)
	{
		outPosts = {};

		if (!world || !pathing)
			return SPT_EGarrisonDetectResult.NO_BUILDING;

		array<IEntity> buildings = {};
		FindBuildings(world, center, buildings);
		if (buildings.IsEmpty())
			return SPT_EGarrisonDetectResult.NO_BUILDING;

		IEntity target = ResolveStructuralShell(NearestBuilding(buildings, center));
		if (!target)
			return SPT_EGarrisonDetectResult.NO_BUILDING;

		vector mins, maxs;
		target.GetWorldBounds(mins, maxs);

		array<vector> snapped = {};
		SampleNavmesh(pathing, mins, maxs, snapped);
		if (snapped.IsEmpty())
			return SPT_EGarrisonDetectResult.NAVMESH_NOT_READY;

		// mantem apenas pontos dentro da construcao, sob teto, com piso e espaco para ficar em pe
		array<vector> valid = {};
		foreach (vector p : snapped)
		{
			if (USE_INTERIOR_OBB_GATE && !IsInsideInteriorVolume(target, p))
				continue;
			if (!IsUnderRoof(world, target, p))
				continue;
			if (IsUnderground(world, p))
				continue;
			if (!HasClearStandingSpace(world, target, p))
				continue;
			if (!HasWallsAround(world, target, p))
				continue;
			valid.Insert(p);
		}
		if (valid.IsEmpty())
			return SPT_EGarrisonDetectResult.NO_POSITIONS;

		array<vector> doors = {};
		CollectBuildingDoors(target, doors);
		vector buildingCenter = (mins + maxs) * 0.5;

		// descarta pontos em pisos sem porta proxima na mesma altura (provavelmente inalcancaveis)
		array<vector> reachable = {};
		FilterToDoorLevels(valid, doors, reachable);

		// mantem apenas o cluster conectado ao clique, para nao guarnecer um galpao isolado no fundo
		array<vector> contained = {};
		KeepCenterComponent(world, target, reachable, center, contained);

		// evita posicionar soldados bem em frente a portas e nao reutiliza postos ja ocupados
		array<vector> spaced = {};
		FilterByClearance(contained, doors, DOOR_CLEARANCE_M, spaced);

		array<vector> available = {};
		FilterByClearance(spaced, excluded, OCCUPIED_CLEARANCE_M, available);
		if (available.IsEmpty())
			return SPT_EGarrisonDetectResult.NO_POSITIONS;

		// espalha os postos o maximo possivel para cobrir o local, em vez de agrupar todos no mesmo canto
		array<vector> positions = {};
		FarthestPointSample(available, buildingCenter, desiredCount, positions);
		if (positions.IsEmpty())
			return SPT_EGarrisonDetectResult.NO_POSITIONS;

		// determina o que cada soldado deve vigiar: portas, janelas, escadas e a direcao provavel de aproximacao
		array<ref SPT_GarrisonOpening> openings = {};
		CollectOpenings(world, target, buildingCenter, openings);
		DetectRoutes(contained, openings);
		vector approach = ApproachDir(openings);

		s_DebugOpenings = openings;
		s_DebugContained = contained;

		array<vector> facings = {};
		AssignFacings(world, positions, openings, buildingCenter, approach, facings);

		for (int i = 0; i < positions.Count(); i++)
			outPosts.Insert(new SPT_GarrisonPost(positions[i], facings[i]));

		return SPT_EGarrisonDetectResult.OK;
	}

	// Coleta uma rede de postos em todas as construcoes da area. Cada ponto continua
	// validado pelo pipeline interno e orientado para portas, janelas e escadas.
	static void DetectAreaPosts(
		BaseWorld world,
		vector center,
		float radius,
		AIPathfindingComponent pathing,
		out array<ref SPT_GarrisonPost> outPosts)
	{
		outPosts = {};
		if (!world || !pathing || radius <= 0)
			return;

		array<vector> buildingCenters = {};
		CollectBuildingCenters(world, center, radius, buildingCenters);
		array<vector> noExclusions = {};

		foreach (vector buildingCenter : buildingCenters)
		{
			array<ref SPT_GarrisonPost> buildingPosts = {};
			SPT_EGarrisonDetectResult result = DetectPosts(
				world,
				buildingCenter,
				pathing,
				12,
				noExclusions,
				buildingPosts
			);
			if (result != SPT_EGarrisonDetectResult.OK)
				continue;

			foreach (SPT_GarrisonPost post : buildingPosts)
				outPosts.Insert(new SPT_GarrisonPost(post.m_Pos, post.m_Facing));
		}
	}

	// Apenas a bounding box da construcao, usada para saber quais tiles do navmesh esperar
	static bool ResolveBuildingBounds(BaseWorld world, vector center, out vector mins, out vector maxs)
	{
		mins = vector.Zero;
		maxs = vector.Zero;
		if (!world)
			return false;

		array<IEntity> buildings = {};
		FindBuildings(world, center, buildings);
		if (buildings.IsEmpty())
			return false;

		IEntity target = ResolveStructuralShell(NearestBuilding(buildings, center));
		if (!target)
			return false;

		target.GetWorldBounds(mins, maxs);
		return true;
	}

	// Toda estrutura destrutivel dentro do alcance do clique
	protected static void FindBuildings(BaseWorld world, vector origin, out array<IEntity> result)
	{
		s_QueryAccumulator = result;
		world.QueryEntitiesBySphere(origin, BUILDING_SEARCH_RADIUS, BuildingFilterCallback, null, EQueryEntitiesFlags.STATIC);
		s_QueryAccumulator = null;
	}

	protected static bool BuildingFilterCallback(IEntity entity)
	{
		if (!entity)
			return true;
		SCR_DestructibleBuildingComponent comp = SCR_DestructibleBuildingComponent.Cast(
			entity.FindComponent(SCR_DestructibleBuildingComponent));
		if (comp && s_QueryAccumulator)
			s_QueryAccumulator.Insert(entity);
		return true;
	}

	protected static IEntity NearestBuilding(notnull array<IEntity> buildings, vector origin)
	{
		IEntity nearest;
		float nearestDist = float.MAX;
		foreach (IEntity b : buildings)
		{
			if (!b)
				continue;
			float d = vector.Distance(origin, b.GetOrigin());
			if (d < nearestDist)
			{
				nearestDist = d;
				nearest = b;
			}
		}
		return nearest;
	}

	// Podemos ter capturado uma cadeira ou um prop; sobe ate a peca real de paredes e teto
	protected static IEntity ResolveStructuralShell(IEntity picked)
	{
		if (!picked)
			return null;

		IEntity root = picked.GetRootParent();
		if (!root)
			root = picked;

		if (IsFurniturePiece(root) || !HasOwnGeometry(root))
		{
			IEntity better = FindShellChild(root);
			if (better)
				return better;
		}

		return root;
	}

	// Verificacao simples por nome: moveis nao sao a casca estrutural que queremos guarnecer
	protected static bool IsFurniturePiece(IEntity entity)
	{
		if (!entity)
			return false;
		EntityPrefabData data = entity.GetPrefabData();
		if (!data)
			return false;
		string path = data.GetPrefabName();
		return path.Contains("/Furniture/") || path.Contains("_Furniture");
	}

	protected static bool HasOwnGeometry(IEntity entity)
	{
		if (!entity)
			return false;
		return entity.GetVObject() != null && entity.GetPhysics() != null;
	}

	protected static IEntity FindShellChild(IEntity root)
	{
		if (!root)
			return null;
		IEntity child = root.GetChildren();
		while (child)
		{
			SCR_DestructibleBuildingComponent comp = SCR_DestructibleBuildingComponent.Cast(
				child.FindComponent(SCR_DestructibleBuildingComponent));
			if (comp && !IsFurniturePiece(child) && HasOwnGeometry(child))
				return child;
			child = child.GetSibling();
		}
		return null;
	}

	// Cria uma grade sobre a construcao em algumas alturas de piso e encaixa cada ponto no navmesh,
	// resultando apenas em locais onde e possivel ficar em pe
	protected static void SampleNavmesh(AIPathfindingComponent pathing, vector mins, vector maxs, out array<vector> snapped)
	{
		snapped = {};

		float xSize = maxs[0] - mins[0];
		float zSize = maxs[2] - mins[2];
		float ySize = maxs[1] - mins[1];
		int xSteps = Math.Floor(xSize / GRID_SPACING_M);
		int zSteps = Math.Floor(zSize / GRID_SPACING_M);

		int floorCount = Math.Floor(ySize / Y_SLICE_SPACING_M);
		if (floorCount < 1)
			floorCount = 1;

		for (int yi = 0; yi < floorCount; yi++)
		{
			float sliceY = mins[1] + yi * Y_SLICE_SPACING_M + FLOOR_OFFSET_M;
			if (sliceY >= maxs[1])
				continue;

			for (int xi = 0; xi <= xSteps; xi++)
			{
				for (int zi = 0; zi <= zSteps; zi++)
				{
					vector gridPt = Vector(
						mins[0] + xi * GRID_SPACING_M,
						sliceY,
						mins[2] + zi * GRID_SPACING_M);

					vector snappedPt;
					if (!pathing.GetClosestPositionOnNavmesh(gridPt, NAVMESH_SEARCH_HALF, snappedPt))
						continue;

					bool dup = false;
					foreach (vector s : snapped)
					{
						if (vector.DistanceSq(s, snappedPt) < SNAP_DEDUP_SQ)
						{
							dup = true;
							break;
						}
					}
					if (!dup)
						snapped.Insert(snappedPt);
				}
			}
		}
	}

	// Dispara um raio diretamente para cima; se atingir algo, ha um teto sobre este ponto
	protected static bool IsUnderRoof(BaseWorld world, IEntity building, vector point)
	{
		TraceParam p = new TraceParam();
		p.Start = point + Vector(0, ROOF_PROBE_START_M, 0);
		p.End = p.Start + Vector(0, ROOF_PROBE_HEIGHT_M, 0);
		p.Flags = TraceFlags.ENTS;
		p.LayerMask = EPhysicsLayerDefs.Projectile;

		float ratio = world.TraceMove(p, null);
		return ratio < 0.99;
	}

	// Rejeita pontos sob o terreno, sem piso abaixo ou sem altura livre; mantem-nos fora de poroes
	protected static bool IsUnderground(BaseWorld world, vector point)
	{
		float terrainY = world.GetSurfaceY(point[0], point[2]);
		if (point[1] < terrainY - UNDERGROUND_SLOP_M)
			return true;

		vector floorHit;
		vector downStart = point + Vector(0, 0.1, 0);
		float fDown = TraceSeg(world, downStart, downStart - Vector(0, FLOOR_PROBE_M + 0.1, 0), floorHit);
		if (fDown >= 1.0)
			return true;
		if ((point[1] - floorHit[1]) > MAX_FLOOR_GAP_M)
			return true;

		vector ceilHit;
		vector upStart = floorHit + Vector(0, 0.05, 0);
		float fUp = TraceSeg(world, upStart, upStart + Vector(0, MIN_HEADROOM_M, 0), ceilHit);
		if (fUp < 1.0)
			return true;

		return false;
	}

	// Verifica espaco livre na altura do peito em 8 direcoes, garantindo que o soldado nao esta
	// preso em um canto de parede
	protected static bool HasClearStandingSpace(BaseWorld world, IEntity shell, vector point)
	{
		vector chest = point + Vector(0, CHEST_H_M, 0);
		float step = Math.PI * 2.0 / 8.0;
		for (int i = 0; i < 8; i++)
		{
			float a = i * step;
			vector dir = Vector(Math.Cos(a), 0, Math.Sin(a));

			TraceParam p = new TraceParam();
			p.Start = chest;
			p.End = chest + dir * CLEAR_RADIUS_M;
			p.Flags = TraceFlags.ENTS;
			p.LayerMask = EPhysicsLayerDefs.Projectile;
			p.Exclude = shell;

			if (world.TraceMove(p, null) < 0.99)
				return false;
		}
		return true;
	}

	// Fechado por ESTA construcao? Somente as paredes desta construcao contam, nao as dos vizinhos
	// (evita postos em paredes externas em vilarejos densos)
	protected static bool HasWallsAround(BaseWorld world, IEntity building, vector point)
	{
		IEntity buildingRoot = building.GetRootParent();
		if (!buildingRoot)
			buildingRoot = building;

		vector eye = point + Vector(0, ENCLOSE_H_M, 0);
		float step = Math.PI * 2.0 / 8.0;
		int hits = 0;
		for (int i = 0; i < 8; i++)
		{
			float a = i * step;
			vector dir = Vector(Math.Cos(a), 0, Math.Sin(a));

			TraceParam p = new TraceParam();
			p.Start = eye;
			p.End = eye + dir * ENCLOSE_RAY_M;
			p.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			p.LayerMask = EPhysicsLayerDefs.Projectile;
			float frac = world.TraceMove(p, null);
			if (frac >= 1.0)
				continue;

			// apenas as paredes desta construcao contam, paredes vizinhas nao nos fecham
			if (IsPartOfBuilding(p.TraceEnt, buildingRoot))
				hits++;
		}
		return hits >= ENCLOSE_MIN_HITS;
	}

	// True se a entidade atingida pertence a construcao alvo (compartilha a mesma raiz)
	protected static bool IsPartOfBuilding(IEntity hit, IEntity buildingRoot)
	{
		if (!hit || !buildingRoot)
			return false;
		IEntity hitRoot = hit.GetRootParent();
		if (!hitRoot)
			hitRoot = hit;
		return hitRoot == buildingRoot;
	}

	// Testa o ponto contra as caixas internas autorais da construcao (frame local).
	// Se nao houver caixas, retorna TRUE para que as heuristicas de raio continuem no comando.
	protected static bool IsInsideInteriorVolume(IEntity shell, vector point)
	{
		SCR_DestructibleBuildingComponent comp = SCR_DestructibleBuildingComponent.Cast(
			shell.FindComponent(SCR_DestructibleBuildingComponent));
		if (!comp)
			return true;

		array<ref SCR_InteriorBoundingBox> boxes = comp.SPT_GetInteriorBoxes();
		if (!boxes || boxes.IsEmpty())
			return true;

		vector mat[4];
		shell.GetWorldTransform(mat);
		vector local = point.InvMultiply4(mat);

		foreach (SCR_InteriorBoundingBox box : boxes)
		{
			if (!box)
				continue;
			vector center;
			if (!box.GetCenter(center))
				continue;
			vector half = box.GetScale() * 0.5;
			vector mn = center - half;
			vector mx = center + half;
			float m = INTERIOR_MARGIN_M;
			if (local[0] >= mn[0] - m && local[0] <= mx[0] + m
				&& local[1] >= mn[1] - m && local[1] <= mx[1] + m
				&& local[2] >= mn[2] - m && local[2] <= mx[2] + m)
				return true;
		}
		return false;
	}

	// Um unico raio; retorna a fracao percorrida e o ponto de parada
	protected static float TraceSeg(BaseWorld world, vector from, vector to, out vector outHit)
	{
		TraceParam p = new TraceParam();
		p.Start = from;
		p.End = to;
		p.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		p.LayerMask = EPhysicsLayerDefs.Projectile;
		float frac = world.TraceMove(p, null);
		outHit = vector.Lerp(from, to, frac);
		return frac;
	}

	// Coleta a posicao de todas as portas na construcao, usadas como ancoras de piso e para evitar bloquea-las
	protected static void CollectBuildingDoors(IEntity target, out array<vector> outDoors)
	{
		outDoors = {};
		IEntity root = target.GetRootParent();
		if (!root)
			root = target;
		WalkForDoors(root, outDoors, 0);
	}

	protected static void WalkForDoors(IEntity entity, notnull array<vector> outDoors, int depth)
	{
		if (!entity || depth > 10)
			return;
		BaseDoorComponent door = BaseDoorComponent.Cast(entity.FindComponent(BaseDoorComponent));
		if (door)
		{
			outDoors.Insert(entity.GetOrigin());
		}
		IEntity child = entity.GetChildren();
		while (child)
		{
			WalkForDoors(child, outDoors, depth + 1);
			child = child.GetSibling();
		}
	}

	// Descarta pontos muito proximos de um marcador (porta ou posto ocupado);
	// se todos forem descartados, mantem todos em vez de ficar sem nenhum
	protected static void FilterByClearance(notnull array<vector> points, notnull array<vector> markers, float clearance, out array<vector> outKept)
	{
		outKept = {};
		if (markers.IsEmpty())
		{
			outKept.Copy(points);
			return;
		}

		float clr2 = clearance * clearance;
		foreach (vector p : points)
		{
			bool clear = true;
			foreach (vector m : markers)
			{
				if (HorizontalDistSq(p, m) < clr2)
				{
					clear = false;
					break;
				}
			}
			if (clear)
				outKept.Insert(p);
		}

		if (outKept.IsEmpty())
			outKept.Copy(points);
	}

	// So confia em pisos que tenham uma porta proxima na mesma altura,
	// evitando mezaninos inalcancaveis
	protected static void FilterToDoorLevels(notnull array<vector> points, notnull array<vector> doors, out array<vector> outKept)
	{
		outKept = {};
		if (doors.IsEmpty())
		{
			outKept.Copy(points);
			return;
		}

		foreach (vector p : points)
		{
			foreach (vector d : doors)
			{
				float dy = p[1] - d[1];
				if (dy < 0)
					dy = -dy;
				if (dy <= DOOR_FLOOR_TOL_M)
				{
					outKept.Insert(p);
					break;
				}
			}
		}

		if (outKept.IsEmpty())
			outKept.Copy(points);
	}

	// Union-find: agrupa pontos proximos entre si e mantem apenas o grupo mais proximo do clique,
	// para que a guarnicao nao transborde para o predio vizinho
	protected static void KeepCenterComponent(BaseWorld world, IEntity target, notnull array<vector> points, vector center, out array<vector> outKept)
	{
		outKept = {};
		int n = points.Count();
		if (n == 0)
			return;

		array<int> parent = {};
		parent.Resize(n);
		for (int i = 0; i < n; i++)
			parent[i] = i;

		float link2 = CONNECT_DIST_M * CONNECT_DIST_M;
		for (int i = 0; i < n; i++)
		{
			for (int j = i + 1; j < n; j++)
			{
				float dy = points[i][1] - points[j][1];
				if (dy < 0)
					dy = -dy;
				if (dy > CONNECT_Y_M)
					continue;
				if (HorizontalDistSq(points[i], points[j]) > link2)
					continue;

				vector mid = (points[i] + points[j]) * 0.5;
				if (!IsUnderRoof(world, target, mid))
					continue;

				Union(parent, i, j);
			}
		}

		int nearest = 0;
		float best = float.MAX;
		for (int i = 0; i < n; i++)
		{
			float d = WeightedDistSq(points[i], center);
			if (d < best)
			{
				best = d;
				nearest = i;
			}
		}

		int keepRoot = Find(parent, nearest);
		for (int i = 0; i < n; i++)
		{
			if (Find(parent, i) == keepRoot)
				outKept.Insert(points[i]);
		}

		if (outKept.IsEmpty())
			outKept.Copy(points);
	}

	// Seleciona pontos o mais distantes possivel dos ja escolhidos, para que o esquadrao se espalhe
	// em vez de se aglomerar. Usa algoritmo farthest-point sampling com fator de aleatoriedade.
	protected static void FarthestPointSample(notnull array<vector> candidates, vector center, int want, out array<vector> outPicked)
	{
		outPicked = {};
		int c = candidates.Count();
		if (c == 0)
			return;

		float minSpace2 = MIN_POST_SPACING_M * MIN_POST_SPACING_M;

		int seed = 0;
		float far = -1.0;
		for (int i = 0; i < c; i++)
		{
			float d = WeightedDistSq(candidates[i], center);
			if (d > far)
			{
				far = d;
				seed = i;
			}
		}

		array<bool> taken = {};
		taken.Resize(c);
		array<float> minDist = {};
		minDist.Resize(c);
		for (int i = 0; i < c; i++)
		{
			taken[i] = false;
			minDist[i] = WeightedDistSq(candidates[i], candidates[seed]);
		}
		outPicked.Insert(candidates[seed]);
		taken[seed] = true;

		while (outPicked.Count() < want)
		{
			int pick = TopKFarthest(minDist, taken, FPS_JITTER_K, minSpace2);
			if (pick < 0)
				break;
			outPicked.Insert(candidates[pick]);
			taken[pick] = true;
			for (int i = 0; i < c; i++)
			{
				if (taken[i])
					continue;
				float d = WeightedDistSq(candidates[i], candidates[pick]);
				if (d < minDist[i])
					minDist[i] = d;
			}
		}
	}

	// Escolhe aleatoriamente um dos K pontos mais distantes, para que layouts identicos
	// nao preencham sempre os mesmos postos exatos
	protected static int TopKFarthest(notnull array<float> minDist, notnull array<bool> taken, int k, float minSpace2)
	{
		array<int> top = {};
		array<float> topVal = {};
		for (int i = 0; i < minDist.Count(); i++)
		{
			if (taken[i] || minDist[i] < minSpace2)
				continue;

			float d = minDist[i];
			int pos = top.Count();
			while (pos > 0 && topVal[pos - 1] < d)
				pos = pos - 1;
			if (pos >= k)
				continue;

			top.InsertAt(i, pos);
			topVal.InsertAt(d, pos);
			if (top.Count() > k)
			{
				top.Remove(top.Count() - 1);
				topVal.Remove(topVal.Count() - 1);
			}
		}

		if (top.IsEmpty())
			return -1;
		return top[Math.RandomInt(0, top.Count())];
	}

	// Distancia que pondera mais a separacao vertical, para que pisos diferentes sejam
	// tratados como genuinamente distantes entre si
	protected static float WeightedDistSq(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dy = (a[1] - b[1]) * FLOOR_WEIGHT;
		float dz = a[2] - b[2];
		return dx * dx + dy * dy + dz * dz;
	}

	// Este posto tem linha de visao para aquela abertura? O raio para um pouco antes do alvo
	// para que a propria abertura nao bloqueie o teste
	protected static bool LosClear(BaseWorld world, vector a, vector b)
	{
		vector start = a + Vector(0, LOS_H_M, 0);
		vector end = b + Vector(0, LOS_H_M, 0);
		float dist = vector.Distance(start, end);
		if (dist <= LOS_STOP_SHORT_M)
			return true;

		vector dir = (end - start) * (1.0 / dist);
		end = end - dir * LOS_STOP_SHORT_M;

		TraceParam p = new TraceParam();
		p.Start = start;
		p.End = end;
		p.Flags = TraceFlags.ENTS;
		p.LayerMask = EPhysicsLayerDefs.Projectile;

		return world.TraceMove(p, null) >= 0.99;
	}

	// Union-find com compressao de caminho
	protected static int Find(notnull array<int> parent, int i)
	{
		while (parent[i] != i)
		{
			parent[i] = parent[parent[i]];
			i = parent[i];
		}
		return i;
	}

	protected static void Union(notnull array<int> parent, int a, int b)
	{
		int ra = Find(parent, a);
		int rb = Find(parent, b);
		if (ra != rb)
			parent[ra] = rb;
	}

	// Atribui direcoes de vigilancia: primeiro escadas sao reivindicadas pelo soldado mais proximo,
	// depois as melhores aberturas visiveis sao distribuida uma a uma, e os que sobrarem olham
	// para a melhor abertura que conseguem ver
	protected static void AssignFacings(BaseWorld world, notnull array<vector> posts, notnull array<ref SPT_GarrisonOpening> openings, vector center, vector approach, out array<vector> outFacings)
	{
		int np = posts.Count();
		int no = openings.Count();

		outFacings = {};
		outFacings.Resize(np);
		for (int i = 0; i < np; i++)
			outFacings[i] = OutwardDir(posts[i], center);

		if (no == 0 || np == 0)
			return;

		array<float> score = {};
		score.Resize(np * no);
		for (int i = 0; i < np; i++)
		{
			for (int o = 0; o < no; o++)
			{
				if (LosClear(world, posts[i], openings[o].m_Pos))
					score[i * no + o] = ScoreEdge(posts[i], openings[o], approach);
				else
					score[i * no + o] = -1.0;
			}
		}

		array<bool> postUsed = {};
		postUsed.Resize(np);
		for (int i = 0; i < np; i++)
			postUsed[i] = false;

		array<bool> covered = {};
		covered.Resize(no);
		for (int o = 0; o < no; o++)
			covered[o] = false;

		// Escadas primeiro: o soldado mais proximo vigia o caminho de subida
		float claim2 = ROUTE_CLAIM_M * ROUTE_CLAIM_M;
		for (int o = 0; o < no; o++)
		{
			if (!openings[o].m_Route)
				continue;

			int bestPost = -1;
			float best = claim2;
			for (int i = 0; i < np; i++)
			{
				if (postUsed[i])
					continue;
				float d = HorizontalDistSq(posts[i], openings[o].m_Pos);
				if (d < best)
				{
					best = d;
					bestPost = i;
				}
			}
			if (bestPost < 0)
				continue;

			if (openings[o].m_Normal.Length() > 0.001)
				outFacings[bestPost] = openings[o].m_Normal;
			else
				outFacings[bestPost] = FaceDir(posts[bestPost], openings[o].m_Pos);
			postUsed[bestPost] = true;
			covered[o] = true;
		}

		// Depois, emparelha guloso as melhores combinacoes restantes de posto-abertura ate acabar
		while (true)
		{
			int bestPost = -1;
			int bestOpening = -1;
			float best = 0.0;
			for (int o = 0; o < no; o++)
			{
				if (covered[o])
					continue;
				for (int i = 0; i < np; i++)
				{
					if (postUsed[i])
						continue;
					float s = score[i * no + o];
					if (s > best)
					{
						best = s;
						bestPost = i;
						bestOpening = o;
					}
				}
			}
			if (bestPost < 0)
				break;

			outFacings[bestPost] = FaceDir(posts[bestPost], openings[bestOpening].m_Pos);
			postUsed[bestPost] = true;
			covered[bestOpening] = true;
		}

		// Quem ainda estiver sem funcao vigia a melhor abertura que consegue ver, mesmo que compartilhada
		for (int i = 0; i < np; i++)
		{
			if (postUsed[i])
				continue;
			int bestO = -1;
			float best = 0.0;
			for (int o = 0; o < no; o++)
			{
				float s = score[i * no + o];
				if (s > best)
				{
					best = s;
					bestO = o;
				}
			}
			if (bestO >= 0)
				outFacings[i] = FaceDir(posts[i], openings[bestO].m_Pos);
		}
	}

	// Pontua um posto vigiando uma abertura: favorece aberturas externas, alinhadas com a aproximacao e proximas
	protected static float ScoreEdge(vector post, SPT_GarrisonOpening opening, vector approach)
	{
		float ext = 0.2;
		if (opening.m_Exterior)
			ext = 1.0;
		else if (opening.m_Route)
			ext = W_ROUTE;

		float align = 0.0;
		if (approach.Length() > 0.001)
		{
			float a = vector.Dot(opening.m_Normal, approach);
			if (a > 0.0)
				align = a;
		}

		float d = vector.Distance(post, opening.m_Pos);
		float near = 1.0 - d / MAX_WATCH_M;
		if (near < 0.0)
			near = 0.0;

		return W_EXT * ext + W_APPROACH * align + W_NEAR * near;
	}

	// Direcao horizontal do posto ate o alvo, normalizada
	protected static vector FaceDir(vector pos, vector target)
	{
		vector dir = target - pos;
		dir[1] = 0;
		if (dir.Length() < 0.001)
			return vector.Zero;
		dir.Normalize();
		return dir;
	}

	// Direcao oposta ao centro da construcao: fallback quando nao ha nada melhor para vigiar
	protected static vector OutwardDir(vector pos, vector center)
	{
		vector outward = pos - center;
		outward[1] = 0;
		if (outward.Length() < 0.001)
			return vector.Zero;
		outward.Normalize();
		return outward;
	}

	// Coleta portas e janelas, inverte cada normal para fora e sinaliza as que dao para o exterior
	protected static void CollectOpenings(BaseWorld world, IEntity target, vector center, out array<ref SPT_GarrisonOpening> outOpenings)
	{
		outOpenings = {};
		IEntity root = target.GetRootParent();
		if (!root)
			root = target;
		WalkForOpenings(root, outOpenings, 0);

		// Para cada abertura, orienta a normal para fora do centro da construcao
		foreach (SPT_GarrisonOpening op : outOpenings)
		{
			vector toOut = op.m_Pos - center;
			toOut[1] = 0;
			vector n = op.m_Normal;
			n[1] = 0;
			if (n.Length() < 0.001)
				n = toOut;
			else if (vector.Dot(n, toOut) < 0)
				n = n * -1.0;
			if (n.Length() > 0.001)
				n.Normalize();
			op.m_Normal = n;

			op.m_Exterior = IsExteriorFacing(world, op);
		}
	}

	// Percorre recursivamente as pecas da construcao, extraindo origens de portas e zonas de acerto
	// de janelas como aberturas a serem vigiadas
	protected static void WalkForOpenings(IEntity entity, notnull array<ref SPT_GarrisonOpening> outOpenings, int depth)
	{
		if (!entity || depth > 10)
			return;

		BaseDoorComponent door = BaseDoorComponent.Cast(entity.FindComponent(BaseDoorComponent));
		if (door)
			outOpenings.Insert(new SPT_GarrisonOpening(entity.GetOrigin(), vector.Zero));
		else
		{
			HitZoneContainerComponent hzc = HitZoneContainerComponent.Cast(entity.FindComponent(HitZoneContainerComponent));
			if (hzc)
			{
				array<HitZone> hitzones = {};
				hzc.GetAllHitZones(hitzones);
				foreach (HitZone hz : hitzones)
				{
					SCR_WindowHitZone win = SCR_WindowHitZone.Cast(hz);
					if (!win)
						continue;

					IEntity owner = entity;
					HitZoneContainerComponent winContainer = win.GetHitZoneContainer();
					if (winContainer && winContainer.GetOwner())
						owner = winContainer.GetOwner();

					vector wm[4];
					owner.GetWorldTransform(wm);
					if (!AlreadyHaveOpening(outOpenings, wm[3]))
						outOpenings.Insert(new SPT_GarrisonOpening(wm[3], wm[2]));
				}
			}
		}

		IEntity child = entity.GetChildren();
		while (child)
		{
			WalkForOpenings(child, outOpenings, depth + 1);
			child = child.GetSibling();
		}
	}

	// Verifica se uma abertura na mesma posicao ja foi coletada (evita duplicatas de janela)
	protected static bool AlreadyHaveOpening(notnull array<ref SPT_GarrisonOpening> have, vector p)
	{
		foreach (SPT_GarrisonOpening h : have)
		{
			if (vector.DistanceSq(h.m_Pos, p) < 0.25)
				return true;
		}
		return false;
	}

	// Dispara um raio curto atraves da abertura; se nao atingir nada, ela se abre para o exterior
	protected static bool IsExteriorFacing(BaseWorld world, SPT_GarrisonOpening op)
	{
		if (op.m_Normal.Length() < 0.001)
			return false;
		vector start = op.m_Pos + op.m_Normal * 0.3 + Vector(0, 0.3, 0);
		vector hit;
		float frac = TraceSeg(world, start, start + op.m_Normal * EXT_RAY_M, hit);
		return frac >= 0.99;
	}

	// Media das normais das aberturas externas: estimativa grosseira da direcao de onde vem o perigo
	protected static vector ApproachDir(notnull array<ref SPT_GarrisonOpening> openings)
	{
		vector sum = vector.Zero;
		foreach (SPT_GarrisonOpening op : openings)
		{
			if (op.m_Exterior)
				sum = sum + op.m_Normal;
		}
		sum[1] = 0;
		if (sum.Length() < 0.001)
			return vector.Zero;
		sum.Normalize();
		return sum;
	}

	// Detecta escadas: um ponto com outro degrau abaixo e ao lado, para que alguem vigie a subida
	protected static void DetectRoutes(notnull array<vector> points, notnull array<ref SPT_GarrisonOpening> openings)
	{
		int n = points.Count();
		float hd2 = ROUTE_HDIST_M * ROUTE_HDIST_M;
		float hmin2 = ROUTE_HDIST_MIN_M * ROUTE_HDIST_MIN_M;

		for (int i = 0; i < n; i++)
		{
			vector down = vector.Zero;
			bool stairTop = false;
			for (int j = 0; j < n; j++)
			{
				if (i == j)
					continue;
				float step = points[i][1] - points[j][1];
				if (step < ROUTE_STEP_MIN_M || step > ROUTE_STEP_MAX_M)
					continue;
				float hsq = HorizontalDistSq(points[i], points[j]);
				if (hsq < hmin2 || hsq > hd2)
					continue;
				down = points[j] - points[i];
				stairTop = true;
				break;
			}
			if (!stairTop)
				continue;
			if (NearExistingRoute(openings, points[i]))
				continue;

			down[1] = 0;
			if (down.Length() > 0.001)
				down.Normalize();
			SPT_GarrisonOpening op = new SPT_GarrisonOpening(points[i], down);
			op.m_Route = true;
			openings.Insert(op);
		}
	}

	// Verifica se ja existe uma rota proxima a este ponto (evita duplicatas de escada)
	protected static bool NearExistingRoute(notnull array<ref SPT_GarrisonOpening> openings, vector p)
	{
		float r2 = ROUTE_DEDUP_M * ROUTE_DEDUP_M;
		foreach (SPT_GarrisonOpening o : openings)
		{
			if (o.m_Route && vector.DistanceSq(o.m_Pos, p) < r2)
				return true;
		}
		return false;
	}

	// Distancia horizontal quadratica (ignora o eixo Y)
	protected static float HorizontalDistSq(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz;
	}

	// Retorna o intervalo de alturas [min..max] dos pontos, para depuracao
	protected static string YRange(notnull array<vector> pts)
	{
		if (pts.IsEmpty())
			return "[-]";
		float mn = float.MAX;
		float mx = -float.MAX;
		foreach (vector p : pts)
		{
			if (p[1] < mn)
				mn = p[1];
			if (p[1] > mx)
				mx = p[1];
		}
		return "[" + mn + ".." + mx + "]";
	}
}

enum SPT_EGarrisonDetectResult
{
	OK,
	NO_BUILDING,
	NAVMESH_NOT_READY,
	NO_POSITIONS
}
