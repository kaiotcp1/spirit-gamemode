# Icones Warfare no mapa tatico

Data de referencia: 2026-06-29.

## Estado atual

Os objetivos Warfare aparecem automaticamente no mapa tatico padrao do jogador.

Nao existe mais:

- mapa Canvas independente;
- action no mundo para abrir mapa Warfare;
- proxy/prefab de quadro para mapa;
- dependencia de descritores automaticos para criar objetivos.

Os pontos exibidos no mapa sao exclusivamente os `SPT_WarfarePoint` colocados no World Editor.

## Arquitetura

| Arquivo | Responsabilidade |
|---|---|
| `Scripts/Game/UI/SPT_WarfareMapRenderer.c` | Overlay, desenho dos icones, tooltip, pulso e limpeza de callbacks |
| `Scripts/Game/GameMode/SPT_WarfareGameModeComponent.c` | Estado autoritativo, cache cliente, RPCs e snapshot JIP |
| `UI/Layouts/WarfareMap.layout` | Layout transparente anexado ao root do mapa |
| `Prefabs/Warfare/SPT_WarfarePoint.et` | Fonte manual dos objetivos exibidos |

O renderer cliente e mantido por referencia forte no `SPT_WarfareGameModeComponent`.

## Ciclo do mapa

1. `SCR_MapEntity.GetOnMapOpen()` cria o overlay como filho do root real do mapa.
2. Enquanto o mapa esta aberto, `EOnFrame` atualiza os icones.
3. `SCR_MapEntity.WorldToScreen()` converte a posicao de mundo para tela, acompanhando pan e zoom.
4. `SCR_MapEntity.GetOnMapClose()` remove widgets, referencias e frame mask.

O renderer impede overlays duplicados quando o mapa e reaberto varias vezes.

## Dados usados pelo renderer

O renderer nao consulta entidades do mundo diretamente. Ele usa:

```c
GetClientPointMapData(...)
```

Esse getter agrega no cliente:

- ID;
- nome;
- posicao;
- tipo;
- estado;
- se e HQ;
- manpower atual;
- indice/situacao da onda de reforco.

O servidor envia:

- snapshot completo por `RplSave`/`RplLoad`, usado por JIP;
- RPC confiavel para mudancas posteriores de estado;
- RPC de registro dos pontos.

## Formas por tipo

| Tipo | Forma |
|---|---|
| Cidade | Circulo |
| Vila | Losango |
| Base militar | Quadrado |
| Aeroporto | Estrela |
| Ruina | Octogono |
| Porto | Pentagono |
| Floresta | Hexagono |
| Montanha | Triangulo |
| Campo | Retangulo |
| Personalizado | Decagono |

As formas sao procedurais. Nao ha dependencia de textura nova.

## Cores por estado

| Estado | Cor |
|---|---|
| `FRONTLINE` | Vermelho |
| `UNDER_ATTACK` | Laranja |
| `CLEARED_WAITING` | Amarelo |
| `CAPTURED_DEFENDING` | Azul claro pulsante |
| `CAPTURED` | Azul |

Pontos HQ recebem contorno extra para diferenciar a area inicial SAFE.

## Tooltip

Ao passar o mouse sobre um icone, o tooltip mostra:

- nome;
- categoria/tipo;
- estado territorial;
- indicador de HQ;
- manpower;
- situacao/onda dos reforcos.

## Relacao com a campanha livre

O mapa apenas apresenta o estado calculado pelo Warfare.

HQs dedicados representam as origens aliadas. Todos os objetivos
`SPT_WarfarePoint` iniciam como `FRONTLINE`, aparecem no mapa e podem ser
atacados em qualquer sequencia. Cada captura e replicada imediatamente.

## Checklist de teste

1. Compile os scripts no Workbench.
2. Garanta que existem `SPT_WarfarePoint` salvos na layer.
3. Rode Play Mode.
4. Aguarde o log de inicializacao Warfare.
5. Abra o mapa tatico nativo.
6. Confirme que todos os pontos aparecem.
7. Verifique cor, forma, tooltip, pan e zoom.
8. Abra e feche o mapa varias vezes; nao deve duplicar icones.
9. Capture um ponto e confirme atualizacao imediata.
10. Teste JIP em cliente remoto/dedicado.

O minimapa e HUD nao fazem parte desta implementacao.
