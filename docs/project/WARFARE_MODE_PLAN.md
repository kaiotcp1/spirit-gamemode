# SPT Warfare — arquitetura atual

Data de referencia: 2026-06-29.

Este documento descreve o estado atual do sistema Warfare depois da mudanca para configuracao 100% manual por `SPT_WarfarePoint`.

## Decisao atual

O Warfare nao cria mais pontos automaticamente a partir de descritores do mapa.

Todo objetivo precisa ser colocado no World Editor usando:

```text
Prefabs/Warfare/SPT_WarfarePoint.et
```

Nao existem mais:

- modo `AUTOMATIC`;
- modo `HYBRID`;
- descoberta de objetivos Warfare por `MapDescriptorComponent`;
- vinculo automatico de ponto Warfare com descritor por distancia;
- lista manual de vizinhos por string;
- IDs instaveis do tipo `MANUAL_n`.

O World Garrison ainda mantem suporte interno a descritores para uso isolado, mas quando o `SPT_WarfareGameModeComponent` existe no GameMode, a descoberta por descritores fica desativada para o fluxo Warfare.

Log esperado:

```text
[SPT_WorldGarrison] Descoberta por descritores desativada; aguardando SPT_WarfarePoint manuais.
```

## Componentes principais

| Arquivo | Responsabilidade |
|---|---|
| `Prefabs/Warfare/SPT_WarfarePoint.et` | Prefab colocavel no World Editor para cada objetivo Warfare |
| `Scripts/Game/Components/SPT_WarfarePointComponent.c` | Atributos do ponto: ID, nome, tipo, raio, ordem de captura e vitoria |
| `Scripts/Game/GameMode/SPT_WarfareGameModeComponent.c` | Autoridade do Warfare, validacao, estados, captura, preview, RPC e JIP |
| `Scripts/Game/GameMode/SPT_WarfareTypes.c` | Enums, dados e snapshots Warfare |
| `Scripts/Game/GameMode/SPT_WorldGarrisonManagerComponent.c` | Streaming, guarnicoes, batalhas, SAFE e localizacoes manuais do Warfare |
| `Scripts/Game/UI/SPT_WarfareMapRenderer.c` | Icones no mapa tatico nativo |

## Configuracao de pontos

Cada `SPT_WarfarePoint` deve ter:

- `Point ID`: obrigatorio, estavel e unico;
- `Display Name`: opcional; se vazio, o ID e usado;
- `Point Type`: tipo visual/logico do objetivo;
- `Radius`: raio de ataque/captura;
- `Capture Order`: ordem numerica da progressao;
- `Counts For Victory`: se entra na condicao de vitoria.

### Ordem de captura

A progressao territorial e derivada somente do atributo `Capture Order`.

| Ordem | Significado |
|---|---|
| `0` | HQ inicial do jogador; sempre SAFE e ja capturada |
| `1` | Primeira etapa inimiga atacavel |
| `2` | Segunda etapa; libera quando todos os pontos de ordem `1` forem capturados |
| `3+` | Etapas seguintes, seguindo a mesma regra |

Regras:

- precisa existir pelo menos um ponto de ordem `0`;
- ordens negativas sao invalidas;
- a sequencia precisa comecar em `0`;
- nao pode haver lacunas entre ordens;
- pontos com a mesma ordem sao objetivos paralelos;
- todos os pontos da ordem anterior precisam ser capturados para liberar a proxima ordem.

Exemplo:

```text
SPT_WarfarePoint_HQ       Capture Order = 0
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

## HQ SAFE

Todo ponto com `Capture Order = 0` e tratado como HQ.

Durante a inicializacao, o Warfare registra uma localizacao de guarnicao manual para cada ponto e marca as localizacoes de HQ como SAFE antes do primeiro ciclo de streaming.

Uma localizacao SAFE:

- nao spawna CQB;
- nao spawna patrulha;
- nao entra em monitoramento de baixa;
- nao inicia batalha;
- cancela filas e batalhas pendentes;
- remove grupos ativos e cacheados;
- nao emite eventos de captura.

Isso evita o problema anterior em que a area inicial do jogador acionava CQB, patrulha ou reforcos inimigos.

## Guarnicoes manuais

Cada ponto Warfare cria sua propria localizacao de guarnicao no centro do ponto.

O ID da localizacao de guarnicao e o mesmo `Point ID` do Warfare. Por isso, o ponto nao depende mais de:

- descritor do mapa;
- distancia ate cidade/vila/base;
- nome de localidade do mapa;
- componente visual de mapa externo.

Fluxo de inicializacao no servidor:

1. coletar todos os `SPT_WarfarePointComponent`;
2. validar IDs e ordens;
3. construir grafo por ordem de captura;
4. registrar localizacoes manuais no `SPT_WorldGarrisonManagerComponent`;
5. marcar HQs como SAFE;
6. calcular frente inicial;
7. replicar snapshot para clientes.

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
- ordem negativa;
- ausencia de HQ (`Capture Order = 0`);
- lacuna na sequencia de ordens;
- falha ao registrar localizacao manual de guarnicao.

Avisos esperados:

- pontos inalcançaveis/desconectados no preview;
- mapa aberto antes do snapshot do cliente, mostrando temporariamente zero pontos.

## Como configurar no editor

1. Coloque um `SPT_WarfarePoint.et` para o HQ do jogador.
2. Configure ID unico e `Capture Order = 0`.
3. Coloque os pontos inimigos.
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
- log registra cada localizacao manual Warfare;
- HQ nao spawna CQB/patrulha/batalha;
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
