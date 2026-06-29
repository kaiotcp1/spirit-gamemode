# Ícones Warfare no mapa tático

## Implementação atual

Os objetivos Warfare são exibidos automaticamente sobre o mapa tático padrão do
jogador. Não existe mais um mapa fullscreen separado nem uma ação no mundo para
abri-lo.

O renderer cliente usa os eventos de `SCR_MapEntity`:

1. `OnMapOpen` cria um overlay transparente como filho do root do mapa.
2. `EOnFrame` usa `SCR_MapEntity.WorldToScreen()` para acompanhar pan e zoom.
3. `OnMapClose` remove widgets, referências e o frame event.

O renderer é mantido por referência forte no
`SPT_WarfareGameModeComponent`. O componente somente habilita frames enquanto o
mapa está aberto.

## Arquivos principais

| Arquivo | Responsabilidade |
|---|---|
| `Scripts/Game/UI/SPT_WarfareMapRenderer.c` | Renderização, formas, cores, pulso e tooltip |
| `Scripts/Game/GameMode/SPT_WarfareGameModeComponent.c` | Estado autoritativo, RPCs e snapshot JIP |
| `UI/Layouts/WarfareMap.layout` | Canvas transparente e widgets do tooltip |

## Apresentação

Cada tipo de ponto recebe uma forma geométrica própria:

| Tipo | Forma |
|---|---|
| Cidade | Círculo |
| Vila | Losango |
| Base militar | Quadrado |
| Aeroporto | Estrela |
| Ruína | Octógono |
| Porto | Pentágono |
| Floresta | Hexágono |
| Montanha | Triângulo |
| Campo | Retângulo |
| Personalizado | Decágono |

O preenchimento representa o estado:

| Estado | Cor |
|---|---|
| `LOCKED` | Vermelho escuro |
| `FRONTLINE` | Vermelho |
| `UNDER_ATTACK` | Laranja |
| `CLEARED_WAITING` | Amarelo |
| `CAPTURED_DEFENDING` | Azul claro pulsante |
| `CAPTURED` | Azul |

Pontos HQ recebem um contorno branco adicional. Ao posicionar o mouse sobre um
ícone, o tooltip mostra nome, HQ, categoria, estado, manpower e situação da onda
de reforços.

## Replicação

- O servidor continua sendo a autoridade dos estados Warfare.
- Mudanças durante a partida usam RPC confiável.
- `RplSave` serializa todos os pontos, posições, tipos, estados e dados de
  combate.
- `RplLoad` reconstrói o cache cliente para jogadores JIP sem disparar
  notificações antigas.
- `GetClientPointMapData()` fornece ao renderer uma leitura agregada e segura.

## Teste manual

1. Compile o projeto no Workbench e verifique o console.
2. Inicie Play Mode e aguarde a inicialização Warfare.
3. Abra o mapa tático padrão.
4. Confirme posição, cor, forma, tooltip, pan e zoom.
5. Abra e feche o mapa repetidamente e confirme que não há ícones duplicados.
6. Force uma transição de estado e confirme atualização imediata.
7. Em servidor dedicado, conecte um cliente depois de uma captura e confirme o
   snapshot JIP.

O minimapa/HUD não faz parte desta implementação.
