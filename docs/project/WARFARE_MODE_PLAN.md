# SPT Warfare — arquitetura atual

Data de referencia: 2026-06-29.

Este documento descreve o sistema Warfare manual com HQs aliados dedicados e
objetivos inimigos configurados por `SPT_WarfarePoint`.

## Decisao atual

O Warfare nao cria mais pontos automaticamente a partir de descritores do mapa.

Toda missao precisa usar os dois tipos de prefab:

```text
Prefabs/Warfare/SPT_WarfareHQ.et
Prefabs/Warfare/SPT_WarfarePoint.et
```

Nao existem mais:

- modo `AUTOMATIC`;
- modo `HYBRID`;
- descoberta de objetivos Warfare por `MapDescriptorComponent`;
- vinculo automatico de ponto Warfare com descritor por distancia;
- lista manual de vizinhos por string;
- IDs instaveis do tipo `MANUAL_n`.

O World Garrison nao executa descoberta por descritores; ele aguarda o registro
manual dos objetivos inimigos pelo Warfare.

Log esperado:

```text
[SPT_WorldGarrison] Descoberta automatica desativada; aguardando configuracoes dos SPT_WarfarePoint.
```

## Componentes principais

| Arquivo | Responsabilidade |
|---|---|
| `Prefabs/Warfare/SPT_WarfareHQ.et` | HQ aliado, sem atributos ou localizacao de guarnicao hostil |
| `Prefabs/Warfare/SPT_WarfarePoint.et` | Objetivo inimigo com configuracao completa de guarnicao |
| `SPT_WarfareNodeComponent` | Dados territoriais compartilhados: ID, nome, tipo e raio |
| `Scripts/Game/GameMode/SPT_WarfareGameModeComponent.c` | Autoridade do Warfare, validacao, estados, captura, preview, RPC e JIP |
| `Scripts/Game/GameMode/SPT_WarfareTypes.c` | Enums, dados e snapshots Warfare |
| `Scripts/Game/GameMode/SPT_WorldGarrisonManagerComponent.c` | Streaming, guarnicoes, batalhas, SAFE e localizacoes manuais do Warfare |
| `Scripts/Game/UI/SPT_WarfareMapRenderer.c` | Icones no mapa tatico nativo |

## Configuracao de pontos

Cada HQ e objetivo inimigo possui ID, nome, tipo visual e raio. Somente
`SPT_WarfarePoint` possui:

- `Capture Order`: obrigatoriamente `1` ou maior;
- `Counts For Victory`: se entra na condicao de vitoria.
- configuracao de guarnicao, streaming, cache, prefabs e reforcos por area.

`SPT_WarfareHQ` e sempre aliado, capturado, ordem territorial `0` e nao conta
para a condicao de vitoria. Varios HQs sao permitidos.

### Ordem de captura

A progressao territorial e derivada somente do atributo `Capture Order`.

| Ordem | Significado |
|---|---|
| `0` | Reservada internamente aos prefabs `SPT_WarfareHQ` |
| `1` | Primeira etapa inimiga atacavel |
| `2` | Segunda etapa; libera quando todos os pontos de ordem `1` forem capturados |
| `3+` | Etapas seguintes, seguindo a mesma regra |

Regras:

- precisa existir pelo menos um `SPT_WarfareHQ`;
- objetivos inimigos com ordem menor que `1` sao invalidos;
- a sequencia precisa comecar em `0`;
- nao pode haver lacunas entre ordens;
- pontos com a mesma ordem sao objetivos paralelos;
- todos os pontos da ordem anterior precisam ser capturados para liberar a proxima ordem.

Exemplo:

```text
SPT_WarfareHQ_Base
SPT_WarfarePoint_Vila_A   Capture Order = 1
SPT_WarfarePoint_Vila_B   Capture Order = 1
SPT_WarfarePoint_Base_C   Capture Order = 2
```

Nesse exemplo, a base de ordem `2` so vira frente de batalha quando `Vila_A` e `Vila_B` estiverem capturadas.

## Grafo territorial

O grafo e construido automaticamente pelo GameMode:

- todo ponto de ordem `N` se conecta aos pontos de ordem `N - 1`;
- os links sao bidirecionais para preview e alcance;
- nao ha mais configuracao manual de vizinhos.

Isso simplifica a configuracao no editor e reduz erro humano. A ordem numerica passa a ser a fonte unica da progressao.

A ordem de captura nao bloqueia streaming fisico. Um ponto `LOCKED` ainda pode
spawnar guarnicao se o jogador chegar dentro da distancia de spawn configurada
para aquela area. A ordem controla captura/frente, nao presenca de IA.

## Configuracao de guarnicao por ponto

Cada `SPT_WarfarePoint` e a fonte unica da configuracao gameplay da propria
area. O `SPT_WorldGarrisonManagerComponent` apenas executa streaming, cache,
spawn e batalha; ele nao fornece defaults globais de area.

Os atributos do prefab possuem valores iniciais concretos. Valores invalidos
sao normalizados localmente durante o uso (por exemplo, despawn deve ser maior
que spawn), nunca herdados do manager.

Configuracoes por area:

- distancia de spawn/despawn e histerese;
- raio de busca de construcoes CQB;
- raio de patrulha;
- budget da localizacao;
- cache ligado/desligado;
- intervalo de spawn assincrono;
- prefabs CQB e patrol;
- waypoint, formacao e movimento de patrulha;
- batalha ligada/desligada;
- delays e tamanho das ondas;
- threshold de sobreviventes para antecipar onda;
- distancia min/max de spawn dos reforcos;
- pesos das estrategias `CONCENTRATED`, `SPREADED` e `CONVOY`;
- prefabs especificos de reforco;
- veiculos e tripulacoes de comboio.

Fallback de prefabs de reforco:

1. usa `Battle Group Prefabs` do ponto;
2. se vazio, usa `Patrol Group Prefabs` do ponto;
3. se ambos estiverem vazios, a area nao gera reforcos de infantaria.

Listas CQB e patrol vazias desativam somente aquele tipo de presenca na area.
Cache e batalha sao flags booleanas do proprio ponto.

O prefab base inclui um preset de teste dificil e imediatamente jogavel:

- streaming em `1200/1600 m`, cache ativo e budget `100`;
- tres grupos vanilla para CQB, patrol e reforcos;
- patrulhas em coluna e `WALK`;
- reforcos em `RUN`, ondas agendadas sem delay e grupos materializados a cada `1000 ms`;
- comboio vanilla configurado e peso `0.4` para a estrategia `CONVOY`.

Esses valores sao apenas o ponto de partida. Cada instancia deve ser ajustada
pela missao quando precisar de composicao, dificuldade ou faccao diferente.

## HQ aliado

O HQ nao e registrado no `SPT_WorldGarrisonManagerComponent`. Portanto nao
possui CQB, patrulha, cache, budget, batalha ou reforcos e e seguro por
construcao. O componente dedicado fica preparado para recursos futuros de HQ,
como respawn, arsenal e economia, sem misturar configuracao inimiga.

## Guarnicoes manuais

Cada objetivo inimigo cria sua propria localizacao de guarnicao no centro. HQs
nao criam localizacao de guarnicao.

O ID da localizacao de guarnicao e o mesmo `Point ID` do Warfare. Por isso, o ponto nao depende mais de:

- descritor do mapa;
- distancia ate cidade/vila/base;
- nome de localidade do mapa;
- componente visual de mapa externo.

Fluxo de inicializacao no servidor:

1. coletar `SPT_WarfareHQComponent` e `SPT_WarfarePointComponent`;
2. validar IDs e ordens;
3. construir grafo por ordem de captura;
4. registrar somente objetivos inimigos no `SPT_WorldGarrisonManagerComponent`;
5. calcular frente inicial;
6. replicar snapshot para clientes.

Logs esperados para pontos validos:

```text
[SPT_WorldGarrison] Localizacao Warfare manual registrada | id=...
[SPT_Warfare] Inicializacao concluida
```

## Estados dos pontos

| Estado | Cor no mapa | Significado |
|---|---|---|
| `LOCKED` | Vermelho escuro | Inimigo, mas ainda bloqueado pela ordem de captura |
| `FRONTLINE` | Vermelho | Atacavel agora |
| `UNDER_ATTACK` | Laranja | Primeira baixa registrada; reforcos autorizados |
| `CLEARED_WAITING` | Amarelo | Guarnicao local eliminada, aguardando presenca/captura |
| `CAPTURED_DEFENDING` | Azul claro pulsante | Capturado, mas batalha/reforcos ainda ativos |
| `CAPTURED` | Azul | Capturado e seguro |

Captura e reforcos sao independentes:

- reforcos inimigos nao bloqueiam a mudanca de ownership;
- o ponto pode ficar azul enquanto a batalha continua;
- ao terminar a batalha, `CAPTURED_DEFENDING` muda para `CAPTURED`;
- recaptura inimiga esta fora do escopo atual.

## Regra de ataque e captura

Um ponto inimigo fica atacavel quando:

- esta em `FRONTLINE`;
- todos os pontos da ordem anterior foram capturados.

A primeira baixa da guarnicao:

1. muda `FRONTLINE` para `UNDER_ATTACK`;
2. dispara `StartLocationBattle(position)` uma unica vez;
3. agenda reforcos usando o sistema de batalha do `SPT_WorldGarrisonManagerComponent`.

Um ponto captura quando:

1. a guarnicao local CQB/patrulha esta zerada;
2. nao ha spawn local pendente;
3. existe jogador vivo da faccao configurada dentro do raio, se `Require Player Presence` estiver ativo;
4. a ordem anterior ja esta completamente capturada.

Se a guarnicao zerar sem presenca, o ponto fica em `CLEARED_WAITING`.

## Preview no World Editor

Ao selecionar o GameMode no World Editor, o componente desenha um preview com `Shape` e callbacks `_WB_*`.

O preview usa a mesma resolucao do runtime:

- HQ em azul;
- frente inicial em laranja;
- pontos alcancaveis por BFS em verde;
- pontos bloqueados/desconectados em vermelho;
- pontos invalidos em magenta;
- linhas entre ordens consecutivas.

O preview tambem mostra raio, ID e nome dos pontos. Ao mover entidades ou alterar atributos, ele recalcula o grafo na atualizacao do editor.

## Mapa tatico nativo

Os icones aparecem no mapa padrao do jogador, nao em um Canvas separado.

O renderer cliente usa:

- `SCR_MapEntity.GetOnMapOpen()`;
- `SCR_MapEntity.GetOnMapClose()`;
- `SCR_MapEntity.WorldToScreen()`;
- overlay transparente `UI/Layouts/WarfareMap.layout`.

O cliente recebe dados por RPC confiavel e por snapshot `RplSave`/`RplLoad` para JIP.

## Validacoes

Erros que bloqueiam a inicializacao Warfare:

- nenhum ponto manual encontrado;
- ID vazio;
- ID duplicado;
- objetivo inimigo com ordem menor que `1`;
- ausencia de `SPT_WarfareHQ`;
- lacuna na sequencia de ordens;
- falha ao registrar localizacao manual de guarnicao.

Avisos esperados:

- pontos inalcançaveis/desconectados no preview;
- mapa aberto antes do snapshot do cliente, mostrando temporariamente zero pontos.

## Como configurar no editor

1. Coloque um `SPT_WarfareHQ.et` e configure um ID unico.
2. Adicione outros HQs se a missao tiver varias origens aliadas.
3. Coloque os `SPT_WarfarePoint.et` inimigos.
4. Configure IDs unicos e ordens `1`, `2`, `3` etc.
5. Selecione o GameMode para visualizar o grafo.
6. Corrija pontos magenta ou lacunas de ordem.
7. Salve a layer.
8. Compile scripts.
9. Rode Play Mode.
10. Abra o mapa tatico nativo e valide os icones.

## Checklist de teste

### Editor

- GameMode selecionado mostra preview.
- HQ aparece azul.
- Ordem `1` aparece como frente inicial.
- Ordens seguintes ficam conectadas por linhas.
- ID duplicado ou vazio fica invalido.
- Lacuna de ordem fica invalida.

### Runtime local

- log mostra descoberta por descritores desativada;
- log registra uma localizacao somente para cada objetivo inimigo;
- log imprime configuracao efetiva por localizacao;
- HQ nao aparece nos logs do gerenciador de guarnicao;
- ponto de ordem `1` spawna inimigos ao aproximar;
- primeira baixa muda para `UNDER_ATTACK`;
- eliminar a guarnicao muda para `CLEARED_WAITING` ou captura;
- entrar no raio captura o ponto;
- proxima ordem so libera quando a ordem anterior inteira foi capturada.

### Mapa

- abrir mapa mostra todos os pontos;
- pan/zoom mantem posicoes corretas;
- tooltip mostra nome, categoria, estado, manpower e onda;
- abrir/fechar repetidamente nao duplica icones;
- JIP recebe snapshot atual.

## Fora do escopo atual

- descoberta automatica de objetivos Warfare;
- modo hibrido;
- recaptura inimiga;
- persistencia entre reinicios;
- economia/recompensas;
- novos respawns de jogador;
- encerramento automatico de sessao.
