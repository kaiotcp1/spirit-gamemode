enum SPT_EGarrisonCQBDebugView
{
	OVERVIEW,
	GRID_RAW,
	FLOOR_TRACE_MISS,
	FLOOR_SNAPPED,
	SNAP_DUPLICATE,
	REJECT_INTERIOR_OBB,
	REJECT_NO_ROOF,
	REJECT_UNDERGROUND,
	REJECT_NO_FLOOR,
	REJECT_FLOOR_GAP,
	REJECT_HEADROOM,
	REJECT_STANDING_SPACE,
	REJECT_WALLS,
	VALID_PHYSICAL,
	REJECT_DOOR_LEVEL,
	REACHABLE_LEVEL,
	REJECT_CLUSTER,
	CONTAINED_CLUSTER,
	REJECT_DOOR_CLEARANCE,
	REJECT_OCCUPIED,
	AVAILABLE,
	DOORS,
	OPENINGS_EXTERIOR,
	OPENINGS_INTERIOR,
	ROUTES,
	FINAL_POSTS
}

//! Workbench-only visualizer for the complete SPT garrison CQB pipeline.
[WorkbenchToolAttribute(
	category: "SPT",
	name: "SPT Garrison CQB Debug",
	description: "Clique no viewport para fixar o centro, ajuste os parametros e use Analyze locked area. Os marcadores sao temporarios.",
	awesomeFontCode: 0xF06E
)]
class SPT_GarrisonCQBDebugTool : WorldEditorTool
{
	// Analysis
	[Attribute("75", UIWidgets.Slider, "Raio de busca por construcoes.", "5 500 1", category: "Analysis")]
	protected float m_fAreaRadius;

	[Attribute("12", UIWidgets.Slider, "Quantidade maxima de postos finais por construcao.", "1 64 1", category: "Analysis")]
	protected int m_iPostsPerBuilding;

	// Preview tuning
	[Attribute("1.5", UIWidgets.Slider, "Espacamento horizontal da grade.", "0.25 5 0.05", category: "CQB Preview")]
	protected float m_fGridSpacing;

	[Attribute("2.5", UIWidgets.Slider, "Distancia vertical entre fatias.", "0.5 6 0.1", category: "CQB Preview")]
	protected float m_fYSliceSpacing;

	[Attribute("1.0", UIWidgets.Slider, "Altura da amostra acima da base da construcao.", "0 3 0.05", category: "CQB Preview")]
	protected float m_fFloorOffset;

	[Attribute("2.5 1.5 2.5", desc: "Meia extensao da busca de snap no navmesh.", category: "CQB Preview")]
	protected vector m_vNavmeshSearchHalf;

	[Attribute("2.0", UIWidgets.Slider, "Distancia minima de portas.", "0 6 0.1", category: "CQB Preview")]
	protected float m_fDoorClearance;

	[Attribute("2.0", UIWidgets.Slider, "Distancia minima de postos ocupados.", "0 6 0.1", category: "CQB Preview")]
	protected float m_fOccupiedClearance;

	[Attribute("2.0", UIWidgets.Slider, "Tolerancia vertical para niveis com portas.", "0 6 0.1", category: "CQB Preview")]
	protected float m_fDoorFloorTolerance;

	[Attribute("2.2", UIWidgets.Slider, "Distancia horizontal de conexao do cluster.", "0.5 6 0.1", category: "CQB Preview")]
	protected float m_fConnectDistance;

	[Attribute("3.5", UIWidgets.Slider, "Tolerancia vertical de conexao do cluster.", "0.5 8 0.1", category: "CQB Preview")]
	protected float m_fConnectY;

	[Attribute("1.8", UIWidgets.Slider, "Espacamento minimo entre postos finais.", "0.5 6 0.1", category: "CQB Preview")]
	protected float m_fMinPostSpacing;

	[Attribute("2.5", UIWidgets.Slider, "Peso vertical usado na distribuicao dos postos.", "1 8 0.1", category: "CQB Preview")]
	protected float m_fFloorWeight;

	[Attribute("3", UIWidgets.Slider, "Aleatoriedade entre os candidatos mais distantes.", "1 10 1", category: "CQB Preview")]
	protected int m_iFarthestJitter;

	[Attribute("0.25", UIWidgets.Slider, "Tolerancia quadratica para deduplicar snaps.", "0.01 2 0.01", category: "CQB Preview")]
	protected float m_fSnapDedupSq;

	[Attribute("0.6", UIWidgets.Slider, "Profundidade da sonda de piso.", "0.1 2 0.05", category: "CQB Preview")]
	protected float m_fFloorProbe;

	[Attribute("0.35", UIWidgets.Slider, "Distancia maxima entre ponto e piso.", "0.05 1.5 0.05", category: "CQB Preview")]
	protected float m_fMaxFloorGap;

	[Attribute("1.8", UIWidgets.Slider, "Altura livre minima.", "0.5 3 0.05", category: "CQB Preview")]
	protected float m_fMinHeadroom;

	[Attribute("0.4", UIWidgets.Slider, "Raio livre ao redor do soldado.", "0.1 1.5 0.05", category: "CQB Preview")]
	protected float m_fClearRadius;

	[Attribute("16", UIWidgets.Slider, "Comprimento dos raios de fechamento.", "2 30 0.5", category: "CQB Preview")]
	protected float m_fEncloseRay;

	[Attribute("4", UIWidgets.Slider, "Minimo de paredes detectadas em oito raios.", "1 8 1", category: "CQB Preview")]
	protected int m_iEncloseMinHits;

	[Attribute("0", desc: "Tambem exige que o ponto esteja em uma interior OBB autoral.", category: "CQB Preview")]
	protected bool m_bUseInteriorOBBGate;

	// Visibility
	[Attribute(
		SPT_EGarrisonCQBDebugView.OVERVIEW.ToString(),
		UIWidgets.ComboBox,
		"Seleciona uma unica etapa do pipeline. OVERVIEW mostra apenas available, aberturas e postos finais.",
		"",
		ParamEnumArray.FromEnum(SPT_EGarrisonCQBDebugView),
		category: "Visibility")]
	protected SPT_EGarrisonCQBDebugView m_eDebugView;

	[Attribute("1", desc: "Exibe o circulo do raio e o centro fixado.", category: "Visibility")]
	protected bool m_bShowAnalysisArea;

	[Attribute("0.12", UIWidgets.Slider, "Raio dos pontos intermediarios.", "0.03 0.5 0.01", category: "Visibility")]
	protected float m_fPointRadius;

	[Attribute("0.3", UIWidgets.Slider, "Raio dos postos finais.", "0.1 1 0.05", category: "Visibility")]
	protected float m_fPostRadius;

	// Colors: one stable color per diagnostic category/family.
	[Attribute("0.45 0.45 0.45 0.35", desc: "Grade bruta.", category: "Colors")]
	protected ref Color m_ColorGrid;
	[Attribute("1 0 0 0.9", desc: "Falha de navmesh.", category: "Colors")]
	protected ref Color m_ColorNavmeshMiss;
	[Attribute("0.25 0.45 1 0.75", desc: "Snap valido no navmesh.", category: "Colors")]
	protected ref Color m_ColorSnapped;
	[Attribute("0.75 0 1 0.9", desc: "Snap duplicado.", category: "Colors")]
	protected ref Color m_ColorDuplicate;
	[Attribute("0.65 0.1 1 0.9", desc: "Rejeitado por interior OBB.", category: "Colors")]
	protected ref Color m_ColorOBBReject;
	[Attribute("1 0.35 0.05 0.9", desc: "Rejeitado por falta de teto.", category: "Colors")]
	protected ref Color m_ColorRoofReject;
	[Attribute("0.55 0.2 0.05 0.9", desc: "Rejeitado por piso/subsolo.", category: "Colors")]
	protected ref Color m_ColorFloorReject;
	[Attribute("1 0.15 0.45 0.9", desc: "Rejeitado por espaco livre.", category: "Colors")]
	protected ref Color m_ColorSpaceReject;
	[Attribute("0.65 0 0 0.9", desc: "Rejeitado por fechamento de paredes.", category: "Colors")]
	protected ref Color m_ColorWallsReject;
	[Attribute("1 0.75 0.05 0.9", desc: "Rejeitado por nivel de porta.", category: "Colors")]
	protected ref Color m_ColorDoorLevelReject;
	[Attribute("0.9 0.5 0.05 0.9", desc: "Rejeitado pelo cluster.", category: "Colors")]
	protected ref Color m_ColorClusterReject;
	[Attribute("0.8 0.8 0.1 0.9", desc: "Rejeitado por clearance de porta.", category: "Colors")]
	protected ref Color m_ColorDoorClearanceReject;
	[Attribute("0.55 0.75 0.1 0.9", desc: "Rejeitado por posto ocupado.", category: "Colors")]
	protected ref Color m_ColorOccupiedReject;
	[Attribute("0 0.7 1 0.85", desc: "Pontos validos.", category: "Colors")]
	protected ref Color m_ColorValid;
	[Attribute("0 0.95 0.9 0.85", desc: "Pontos em niveis reachable.", category: "Colors")]
	protected ref Color m_ColorReachable;
	[Attribute("0.15 0.75 0.35 0.9", desc: "Cluster contido.", category: "Colors")]
	protected ref Color m_ColorContained;
	[Attribute("0.1 1 0.2 0.9", desc: "Candidatos disponiveis.", category: "Colors")]
	protected ref Color m_ColorAvailable;
	[Attribute("1 0.55 0 1", desc: "Portas e aberturas.", category: "Colors")]
	protected ref Color m_ColorOpening;
	[Attribute("1 1 0 1", desc: "Postos finais.", category: "Colors")]
	protected ref Color m_ColorPost;

	protected float m_fCursorX;
	protected float m_fCursorY;
	protected vector m_vCursorPosition;
	protected bool m_bHasCursorPosition;
	protected vector m_vAnalysisCenter;
	protected bool m_bHasAnalysisCenter;
	protected ref SCR_DebugShapeManager m_DebugShapeManager;
	protected ref array<ref SPT_GarrisonDetectionDebug> m_aResults = {};

	protected override void OnActivate()
	{
		m_DebugShapeManager = new SCR_DebugShapeManager();
		m_aResults.Clear();
	}

	protected override void OnDeActivate()
	{
		ClearPreview();
		m_DebugShapeManager = null;
	}

	protected override void OnMouseMoveEvent(float x, float y)
	{
		m_fCursorX = x;
		m_fCursorY = y;
		vector traceStart;
		vector traceDirection;
		m_bHasCursorPosition = m_API.TraceWorldPos(
			x,
			y,
			TraceFlags.WORLD | TraceFlags.ENTS,
			traceStart,
			m_vCursorPosition,
			traceDirection);
		if (!m_bHasCursorPosition || m_vCursorPosition == vector.Zero)
		{
			m_bHasCursorPosition = false;
			return;
		}

		Redraw();
	}

	protected override void OnMousePressEvent(float x, float y, WETMouseButtonFlag buttons)
	{
		if (buttons != WETMouseButtonFlag.LEFT)
			return;

		vector traceStart;
		vector traceDirection;
		vector clickedPosition;
		if (!m_API.TraceWorldPos(
			x,
			y,
			TraceFlags.WORLD | TraceFlags.ENTS,
			traceStart,
			clickedPosition,
			traceDirection))
			return;
		if (clickedPosition == vector.Zero)
			return;

		m_vAnalysisCenter = clickedPosition;
		m_bHasAnalysisCenter = true;
		Redraw();
		Print(string.Format("[SPT_GarrisonCQBDebug] Centro fixado em %1.", m_vAnalysisCenter));
		AnalyzeAtCursor();
	}

	[ButtonAttribute("Analyze locked area")]
	protected void AnalyzeAtCursor()
	{
		if (!m_bHasAnalysisCenter)
		{
			Print("[SPT_GarrisonCQBDebug] Clique no viewport para fixar o centro antes de analisar.", LogLevel.WARNING);
			return;
		}

		BaseWorld world = m_API.GetWorld();
		if (!world || !GetGame())
		{
			Print("[SPT_GarrisonCQBDebug] Mundo do editor ou Game indisponivel.", LogLevel.ERROR);
			return;
		}
		m_aResults.Clear();
		Redraw();

		Print("[SPT_GarrisonCQBDebug] Executando preview editorial por traces de piso.");
		RunAnalysis(null);
	}

	protected void RunAnalysis(AIPathfindingComponent pathing)
	{
		BaseWorld world = m_API.GetWorld();
		if (!world)
			return;

		NavmeshWorldComponent navmesh;
		if (pathing)
			navmesh = pathing.GetNavmeshComponent();
		if (pathing && !navmesh)
		{
			Print("[SPT_GarrisonCQBDebug] O helper nao esta associado a um navmesh de soldados.", LogLevel.ERROR);
			return;
		}

		array<vector> buildingCenters = {};
		SPT_GarrisonDetector.CollectBuildingCenters(world, m_vAnalysisCenter, m_fAreaRadius, buildingCenters);
		if (buildingCenters.IsEmpty())
		{
			Print("[SPT_GarrisonCQBDebug] Nenhuma construcao CQB valida encontrada dentro do raio.", LogLevel.WARNING);
			Redraw();
			return;
		}

		SPT_GarrisonDetectionSettings settings = BuildSettings();
		array<vector> excluded = {};
		int okCount;
		int totalPosts;
		foreach (vector buildingCenter : buildingCenters)
		{
			if (navmesh && !navmesh.IsTileLoaded(buildingCenter))
			{
				Print(string.Format(
					"[SPT_GarrisonCQBDebug] Tile de navmesh nao carregado no predio %1; resultado pode ser NAVMESH_NOT_READY.",
					buildingCenter),
					LogLevel.WARNING);
			}

			SPT_GarrisonDetectionDebug debugData = new SPT_GarrisonDetectionDebug();
			array<ref SPT_GarrisonPost> posts = {};
			SPT_EGarrisonDetectResult result = SPT_GarrisonDetector.DetectPostsDebug(
				world,
				buildingCenter,
				pathing,
				Math.Max(m_iPostsPerBuilding, 1),
				excluded,
				settings,
				debugData,
				posts);
			m_aResults.Insert(debugData);
			if (result == SPT_EGarrisonDetectResult.OK)
				okCount++;
			totalPosts += posts.Count();
			PrintBuildingSummary(debugData, result);
		}

		Redraw();
		PrintCurrentLegend();
		Print(string.Format(
			"[SPT_GarrisonCQBDebug] Analise concluida | predios=%1 | sucesso=%2 | postos=%3 | centro=%4 | raio=%5",
			buildingCenters.Count(),
			okCount,
			totalPosts,
			m_vAnalysisCenter,
			m_fAreaRadius));
	}

	[ButtonAttribute("Clear")]
	protected void ClearButton()
	{
		ClearPreview();
	}

	[ButtonAttribute("Refresh visualization")]
	protected void RefreshVisualization()
	{
		Redraw();
		PrintCurrentLegend();
	}

	protected SPT_GarrisonDetectionSettings BuildSettings()
	{
		SPT_GarrisonDetectionSettings settings = new SPT_GarrisonDetectionSettings();
		settings.m_fGridSpacing = Math.Max(m_fGridSpacing, 0.1);
		settings.m_fYSliceSpacing = Math.Max(m_fYSliceSpacing, 0.1);
		settings.m_fFloorOffset = m_fFloorOffset;
		settings.m_vNavmeshSearchHalf = m_vNavmeshSearchHalf;
		settings.m_fSnapDedupSq = Math.Max(m_fSnapDedupSq, 0.0001);
		settings.m_fDoorClearance = Math.Max(m_fDoorClearance, 0.0);
		settings.m_fOccupiedClearance = Math.Max(m_fOccupiedClearance, 0.0);
		settings.m_fDoorFloorTolerance = Math.Max(m_fDoorFloorTolerance, 0.0);
		settings.m_fConnectDistance = Math.Max(m_fConnectDistance, 0.1);
		settings.m_fConnectY = Math.Max(m_fConnectY, 0.1);
		settings.m_fFloorWeight = Math.Max(m_fFloorWeight, 0.1);
		settings.m_iFarthestJitter = Math.Max(m_iFarthestJitter, 1);
		settings.m_fMinPostSpacing = Math.Max(m_fMinPostSpacing, 0.1);
		settings.m_fFloorProbe = Math.Max(m_fFloorProbe, 0.1);
		settings.m_fMaxFloorGap = Math.Max(m_fMaxFloorGap, 0.01);
		settings.m_fMinHeadroom = Math.Max(m_fMinHeadroom, 0.1);
		settings.m_fClearRadius = Math.Max(m_fClearRadius, 0.05);
		settings.m_fEncloseRay = Math.Max(m_fEncloseRay, 0.5);
		settings.m_iEncloseMinHits = Math.ClampInt(m_iEncloseMinHits, 1, 8);
		settings.m_bUseInteriorOBBGate = m_bUseInteriorOBBGate;
		return settings;
	}

	protected void ClearPreview()
	{
		m_aResults.Clear();
		if (m_DebugShapeManager)
			m_DebugShapeManager.Clear();
	}

	protected void Redraw()
	{
		if (!m_DebugShapeManager)
			return;
		m_DebugShapeManager.Clear();
		if (!m_bHasCursorPosition && !m_bHasAnalysisCenter)
			return;

		vector displayCenter = m_vCursorPosition;
		if (m_bHasAnalysisCenter)
			displayCenter = m_vAnalysisCenter;
		if (m_bShowAnalysisArea)
			DrawRadius(displayCenter, m_fAreaRadius, m_ColorValid.PackToInt());
		if (m_bShowAnalysisArea && m_bHasAnalysisCenter)
			m_DebugShapeManager.AddSphere(
				m_vAnalysisCenter,
				Math.Max(m_fPostRadius, 0.2),
				m_ColorPost.PackToInt(),
				ShapeFlags.NOOUTLINE | ShapeFlags.DEPTH_DITHER);
		foreach (SPT_GarrisonDetectionDebug debugData : m_aResults)
			DrawDebug(debugData);
	}

	protected void DrawDebug(notnull SPT_GarrisonDetectionDebug debugData)
	{
		ShapeFlags shapeFlags = ShapeFlags.NOOUTLINE | ShapeFlags.DEPTH_DITHER;
		switch (m_eDebugView)
		{
		case SPT_EGarrisonCQBDebugView.OVERVIEW:
			DrawPoints(debugData.m_aAvailable, m_fPointRadius * 1.35, m_ColorAvailable.PackToInt(), shapeFlags);
			DrawAllOpenings(debugData, shapeFlags);
			DrawFinalPosts(debugData, shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.GRID_RAW:
			DrawPoints(debugData.m_aGrid, m_fPointRadius * 0.55, m_ColorGrid.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.FLOOR_TRACE_MISS:
			DrawPoints(debugData.m_aNavmeshMiss, m_fPointRadius, m_ColorNavmeshMiss.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.FLOOR_SNAPPED:
			DrawPoints(debugData.m_aSnapped, m_fPointRadius * 0.8, m_ColorSnapped.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.SNAP_DUPLICATE:
			DrawPoints(debugData.m_aSnapDuplicate, m_fPointRadius, m_ColorDuplicate.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_INTERIOR_OBB:
			DrawPoints(debugData.m_aRejectedOBB, m_fPointRadius, m_ColorOBBReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_NO_ROOF:
			DrawPoints(debugData.m_aRejectedRoof, m_fPointRadius * 1.05, m_ColorRoofReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_UNDERGROUND:
			DrawPoints(debugData.m_aRejectedUnderground, m_fPointRadius, m_ColorFloorReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_NO_FLOOR:
			DrawPoints(debugData.m_aRejectedNoFloor, m_fPointRadius, m_ColorFloorReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_FLOOR_GAP:
			DrawPoints(debugData.m_aRejectedFloorGap, m_fPointRadius, m_ColorFloorReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_HEADROOM:
			DrawPoints(debugData.m_aRejectedHeadroom, m_fPointRadius, m_ColorFloorReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_STANDING_SPACE:
			DrawPoints(debugData.m_aRejectedSpace, m_fPointRadius * 1.05, m_ColorSpaceReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_WALLS:
			DrawPoints(debugData.m_aRejectedWalls, m_fPointRadius * 1.1, m_ColorWallsReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.VALID_PHYSICAL:
			DrawPoints(debugData.m_aValid, m_fPointRadius, m_ColorValid.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_DOOR_LEVEL:
			DrawPoints(debugData.m_aRejectedDoorLevel, m_fPointRadius, m_ColorDoorLevelReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REACHABLE_LEVEL:
			DrawPoints(debugData.m_aReachable, m_fPointRadius * 1.1, m_ColorReachable.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_CLUSTER:
			DrawPoints(debugData.m_aRejectedCluster, m_fPointRadius * 1.05, m_ColorClusterReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.CONTAINED_CLUSTER:
			DrawPoints(debugData.m_aContained, m_fPointRadius * 1.2, m_ColorContained.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_DOOR_CLEARANCE:
			DrawPoints(debugData.m_aRejectedDoorClearance, m_fPointRadius * 1.1, m_ColorDoorClearanceReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_OCCUPIED:
			DrawPoints(debugData.m_aRejectedOccupied, m_fPointRadius * 1.15, m_ColorOccupiedReject.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.AVAILABLE:
			DrawPoints(debugData.m_aAvailable, m_fPointRadius * 1.35, m_ColorAvailable.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.DOORS:
			DrawPoints(debugData.m_aDoors, m_fPointRadius * 1.7, m_ColorOpening.PackToInt(), shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.OPENINGS_EXTERIOR:
			DrawFilteredOpenings(debugData, true, false, false, shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.OPENINGS_INTERIOR:
			DrawFilteredOpenings(debugData, false, true, false, shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.ROUTES:
			DrawFilteredOpenings(debugData, false, false, true, shapeFlags);
			break;
		case SPT_EGarrisonCQBDebugView.FINAL_POSTS:
			DrawFinalPosts(debugData, shapeFlags);
			break;
		}
	}

	protected void DrawAllOpenings(
		notnull SPT_GarrisonDetectionDebug debugData,
		ShapeFlags shapeFlags)
	{
		DrawPoints(debugData.m_aDoors, m_fPointRadius * 1.7, m_ColorOpening.PackToInt(), shapeFlags);
		DrawFilteredOpenings(debugData, true, true, true, shapeFlags);
	}

	protected void DrawFilteredOpenings(
		notnull SPT_GarrisonDetectionDebug debugData,
		bool showExterior,
		bool showInterior,
		bool showRoutes,
		ShapeFlags shapeFlags)
	{
		foreach (SPT_GarrisonOpening opening : debugData.m_aOpenings)
		{
			bool shouldDraw;
			int color = m_ColorOpening.PackToInt();
			if (opening.m_Route)
			{
				shouldDraw = showRoutes;
				color = m_ColorDuplicate.PackToInt();
			}
			else if (opening.m_Exterior)
			{
				shouldDraw = showExterior;
				color = m_ColorNavmeshMiss.PackToInt();
			}
			else
			{
				shouldDraw = showInterior;
			}
			if (!shouldDraw)
				continue;

			m_DebugShapeManager.AddSphere(opening.m_Pos, m_fPointRadius * 1.6, color, shapeFlags);
			DrawArrow(opening.m_Pos, opening.m_Normal, 1.25, color);
		}
	}

	protected void DrawFinalPosts(
		notnull SPT_GarrisonDetectionDebug debugData,
		ShapeFlags shapeFlags)
	{
		foreach (SPT_GarrisonPost post : debugData.m_aPosts)
		{
			m_DebugShapeManager.AddSphere(post.m_Pos, m_fPostRadius, m_ColorPost.PackToInt(), shapeFlags);
			DrawArrow(post.m_Pos + Vector(0, 0.15, 0), post.m_Facing, 2.0, m_ColorPost.PackToInt());
		}
	}

	protected void PrintCurrentLegend()
	{
		string description;
		switch (m_eDebugView)
		{
		case SPT_EGarrisonCQBDebugView.OVERVIEW:
			description = "OVERVIEW: verde=available, laranja/vermelho/roxo=aberturas, amarelo=postos e facing.";
			break;
		case SPT_EGarrisonCQBDebugView.GRID_RAW:
			description = "GRID_RAW: cinza=amostras brutas antes de procurar piso.";
			break;
		case SPT_EGarrisonCQBDebugView.FLOOR_TRACE_MISS:
			description = "FLOOR_TRACE_MISS: vermelho=grade sem superficie fisica encontrada abaixo.";
			break;
		case SPT_EGarrisonCQBDebugView.FLOOR_SNAPPED:
			description = "FLOOR_SNAPPED: azul=pontos projetados com sucesso sobre uma superficie.";
			break;
		case SPT_EGarrisonCQBDebugView.SNAP_DUPLICATE:
			description = "SNAP_DUPLICATE: roxo=snaps removidos por proximidade de outro snap.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_INTERIOR_OBB:
			description = "REJECT_INTERIOR_OBB: roxo escuro=fora das caixas interiores autorais.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_NO_ROOF:
			description = "REJECT_NO_ROOF: laranja=nenhum teto detectado acima.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_UNDERGROUND:
			description = "REJECT_UNDERGROUND: marrom=ponto abaixo do terreno alem da tolerancia.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_NO_FLOOR:
			description = "REJECT_NO_FLOOR: marrom=sonda para baixo nao encontrou piso.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_FLOOR_GAP:
			description = "REJECT_FLOOR_GAP: marrom=distancia do ponto ao piso excede Max Floor Gap.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_HEADROOM:
			description = "REJECT_HEADROOM: marrom=obstaculo antes de atingir Min Headroom.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_STANDING_SPACE:
			description = "REJECT_STANDING_SPACE: rosa=obstaculo dentro do Clear Radius na altura do peito.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_WALLS:
			description = "REJECT_WALLS: vermelho escuro=menos paredes que Enclose Min Hits.";
			break;
		case SPT_EGarrisonCQBDebugView.VALID_PHYSICAL:
			description = "VALID_PHYSICAL: ciano=passou OBB, teto, piso, espaco e paredes.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_DOOR_LEVEL:
			description = "REJECT_DOOR_LEVEL: amarelo=sem porta proxima na mesma altura.";
			break;
		case SPT_EGarrisonCQBDebugView.REACHABLE_LEVEL:
			description = "REACHABLE_LEVEL: turquesa=permaneceu depois do filtro vertical de portas.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_CLUSTER:
			description = "REJECT_CLUSTER: laranja=fora do componente conectado ao centro.";
			break;
		case SPT_EGarrisonCQBDebugView.CONTAINED_CLUSTER:
			description = "CONTAINED_CLUSTER: verde escuro=cluster conectado preservado.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_DOOR_CLEARANCE:
			description = "REJECT_DOOR_CLEARANCE: amarelo-esverdeado=muito proximo de porta.";
			break;
		case SPT_EGarrisonCQBDebugView.REJECT_OCCUPIED:
			description = "REJECT_OCCUPIED: verde oliva=muito proximo de posto ocupado.";
			break;
		case SPT_EGarrisonCQBDebugView.AVAILABLE:
			description = "AVAILABLE: verde vivo=candidatos entregues ao seletor final.";
			break;
		case SPT_EGarrisonCQBDebugView.DOORS:
			description = "DOORS: laranja=origens das entidades com BaseDoorComponent.";
			break;
		case SPT_EGarrisonCQBDebugView.OPENINGS_EXTERIOR:
			description = "OPENINGS_EXTERIOR: vermelho=porta/janela classificada como exterior; flecha=normal para fora.";
			break;
		case SPT_EGarrisonCQBDebugView.OPENINGS_INTERIOR:
			description = "OPENINGS_INTERIOR: laranja=abertura interna; flecha=normal calculada.";
			break;
		case SPT_EGarrisonCQBDebugView.ROUTES:
			description = "ROUTES: roxo=topo de rota/escada; flecha=sentido descendente detectado.";
			break;
		case SPT_EGarrisonCQBDebugView.FINAL_POSTS:
			description = "FINAL_POSTS: amarelo=posicao final; flecha=direcao de vigilancia do soldado.";
			break;
		}
		Print("[SPT_GarrisonCQBDebug] " + description);
	}

	protected void DrawPoints(
		notnull array<vector> points,
		float radius,
		int color,
		ShapeFlags shapeFlags)
	{
		foreach (vector point : points)
			m_DebugShapeManager.AddSphere(point, radius, color, shapeFlags);
	}

	protected void DrawRadius(vector center, float radius, int color)
	{
		const int SEGMENTS = 48;
		vector previous = center + Vector(radius, 0.05, 0);
		for (int i = 1; i <= SEGMENTS; i++)
		{
			float angle = Math.PI * 2.0 * i / SEGMENTS;
			vector next = center + Vector(Math.Cos(angle) * radius, 0.05, Math.Sin(angle) * radius);
			m_DebugShapeManager.AddLine(previous, next, color);
			previous = next;
		}
	}

	protected void DrawArrow(vector start, vector direction, float length, int color)
	{
		if (direction.Length() < 0.001)
			return;
		direction[1] = 0;
		direction.Normalize();
		vector end = start + direction * length;
		m_DebugShapeManager.AddLine(start, end, color);

		vector side = Vector(-direction[2], 0, direction[0]);
		vector wingBase = end - direction * 0.35;
		m_DebugShapeManager.AddLine(end, wingBase + side * 0.2, color);
		m_DebugShapeManager.AddLine(end, wingBase - side * 0.2, color);
	}

	protected void PrintBuildingSummary(
		notnull SPT_GarrisonDetectionDebug debugData,
		SPT_EGarrisonDetectResult result)
	{
		Print(string.Format(
			"[SPT_GarrisonCQBDebug] predio=%1 resultado=%2 grid=%3 traceMiss=%4 snapped=%5 roof=%6 space=%7 walls=%8",
			debugData.m_vBuildingCenter,
			result,
			debugData.m_aGrid.Count(),
			debugData.m_aNavmeshMiss.Count(),
			debugData.m_aSnapped.Count(),
			debugData.m_aRejectedRoof.Count(),
			debugData.m_aRejectedSpace.Count(),
			debugData.m_aRejectedWalls.Count()));
		Print(string.Format(
			"[SPT_GarrisonCQBDebug] predio=%1 underground=%2 noFloor=%3 floorGap=%4 headroom=%5 valid=%6 reachable=%7 contained=%8",
			debugData.m_vBuildingCenter,
			debugData.m_aRejectedUnderground.Count(),
			debugData.m_aRejectedNoFloor.Count(),
			debugData.m_aRejectedFloorGap.Count(),
			debugData.m_aRejectedHeadroom.Count(),
			debugData.m_aValid.Count(),
			debugData.m_aReachable.Count(),
			debugData.m_aContained.Count()));
		Print(string.Format(
			"[SPT_GarrisonCQBDebug] predio=%1 doorLevel=%2 cluster=%3 doorClearance=%4 occupied=%5 available=%6 posts=%7 openings=%8",
			debugData.m_vBuildingCenter,
			debugData.m_aRejectedDoorLevel.Count(),
			debugData.m_aRejectedCluster.Count(),
			debugData.m_aRejectedDoorClearance.Count(),
			debugData.m_aRejectedOccupied.Count(),
			debugData.m_aAvailable.Count(),
			debugData.m_aPosts.Count(),
			debugData.m_aOpenings.Count()));
	}
}
