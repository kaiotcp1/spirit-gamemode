# Proximos passos — SPT Warfare

Data de referencia: 2026-06-29.

Este documento lista o que ainda faz sentido depois da mudanca para pontos Warfare 100% manuais.

## Concluido / implementado

- HQs aliados usam `Prefabs/Warfare/SPT_WarfareHQ.et`.
- Objetivos inimigos usam `Prefabs/Warfare/SPT_WarfarePoint.et`.
- Todos os objetivos inimigos iniciam atacaveis e podem ser conquistados em qualquer sequencia.
- HQs sao aliados por tipo e podem existir em varios locais.
- O Warfare registra localizacoes de guarnicao somente para objetivos inimigos.
- HQ nao possui configuracao nem localizacao hostil.
- `SPT_WarfarePoint` configura streaming, cache, prefabs, budget e reforcos por area.
- Nao existem fallbacks globais de gameplay de area no `WorldGarrisonManager`.
- Veiculos de comboio tambem sao configurados por `SPT_WarfarePoint`.
- Preview dos pontos aparece ao selecionar o GameMode no World Editor.
- Icones aparecem no mapa tatico nativo usando `SCR_MapEntity.WorldToScreen()`.
- Snapshot JIP usa `RplSave`/`RplLoad`.
- Mudancas de estado usam RPC confiavel.

## Prioridade alta

### 1. Validar compilacao e runtime do fluxo manual-only

Objetivo:

- confirmar que a remocao do fluxo automatico nao deixou referencias quebradas;
- validar que todos os `SPT_WarfarePoint` inimigos iniciam como `FRONTLINE`.

Checklist:

- compilar scripts sem erro;
- abrir world com GameMode selecionado;
- verificar preview;
- salvar layer com os pontos;
- rodar Play Mode;
- confirmar logs de registro manual e configuracao efetiva por area.

Logs esperados:

```text
[SPT_WorldGarrison] Descoberta automatica desativada; aguardando configuracoes dos SPT_WarfarePoint.
[SPT_WorldGarrison] Localizacao Warfare manual registrada | id=...
[SPT_WorldGarrison] Config local | id=...
[SPT_Warfare] Inicializacao concluida
```

### 2. Melhorar mensagens de validacao no editor

Cenario atual:

- a validacao existe, mas a maior parte do diagnostico ainda depende do console;
- pontos invalidos ficam magenta no preview, mas a causa precisa ficar mais evidente.

Melhoria proposta:

- exibir motivo do erro perto do ponto no preview;
- separar erro bloqueante de aviso;
- destacar IDs duplicados com o mesmo texto/indice.

Erros importantes:

- ID vazio;
- ID duplicado;
- ausencia de HQ;
- ponto sem localizacao registrada.

### 3. Documentar procedimento definitivo de setup no editor

Criar um guia curto com:

- como adicionar `SPT_WarfareHQ.et` e `SPT_WarfarePoint.et`;
- como preencher ID;
- como configurar HQ e guarnicoes inimigas;
- como interpretar cores do preview;
- como testar no mapa.

Pode ficar em um novo documento `docs/project/WARFARE_EDITOR_SETUP.md`.

### 4. Criar presets opcionais de area

Agora cada `SPT_WarfarePoint` possui muitos atributos de guarnicao. Para mapas maiores, presets podem reduzir repeticao:

- vila pequena;
- cidade media;
- base militar;
- aeroporto;
- area leve sem reforco;
- area pesada com reforco/convoy.

## Prioridade media

### 5. API `ForceStartAttack(string pointId)`

Objetivo:

- permitir scripts/missoes iniciarem ataque em um ponto especifico;
- validar se o ponto existe, ainda nao foi capturado e possui guarnicao configurada;
- iniciar batalha sem depender da primeira baixa, se desejado.

### 6. Invokers publicos Warfare

Adicionar eventos para integracoes futuras:

- `OnPointCaptured`;
- `OnPointStateChanged`;
- `OnPointAttackStarted`;
- `OnWarfareVictory` ja existe/permanece.

### 7. Melhorar notificacoes de reforco

Hoje o sistema informa transicoes importantes, mas ainda pode melhorar:

- ETA da primeira onda;
- aviso de nova onda agendada;
- aviso de batalha encerrada;
- evitar duplicidade em JIP.

Como os delays agora podem variar por area, o tooltip/notificacao deve mostrar
o ETA real da localizacao quando essa informacao estiver disponivel.

### 8. Configuracao visual dos icones

Hoje as formas e cores sao procedurais/hardcoded.

Melhoria futura:

- atributos de cor por estado;
- escala do icone;
- configuracao externa por tipo;
- opcao de esconder tipos especificos.

## Prioridade baixa

### 9. Persistencia entre reinicios

Fora do escopo atual. Futuramente salvar:

- pontos capturados;
- estados de guarnicao;
- batalhas ativas;
- budget restante;
- cache de grupos.

### 10. Recaptura inimiga

Fora do escopo atual.

Se for implementada, precisa de regras claras para:

- quando o inimigo pode contra-atacar;
- como ownership muda;
- como evitar frustracao por recaptura invisivel;
- como o mapa comunica risco.

### 11. Encerramento automatico da sessao

Atualmente a vitoria e evento/mensagem. Encerrar a sessao automaticamente deve ser configuravel e desativado por padrao.

## Itens removidos do roadmap

Nao fazem mais parte do plano atual:

- modo `AUTOMATIC` de descoberta Warfare;
- modo `HYBRID`;
- deduplicacao de descritores para objetivos Warfare;
- vinculo manual de ponto a guarnicao por ID externo;
- vizinhos configurados por string;
- action de quadro para abrir mapa Warfare;
- prefab proxy do mapa.
