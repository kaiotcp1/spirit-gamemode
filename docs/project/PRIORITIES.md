# SPT_GAME — prioridades atuais

Data de referencia: 2026-06-29.

Este documento substitui a lista antiga de fases. O foco atual e manter o modo Warfare manual, finito e validavel no editor.

## Estado atual resumido

| Sistema | Estado |
|---|---|
| Streaming de guarnicoes | Implementado com spawn/despawn, cache de sobreviventes e histerese |
| Budget por localizacao | Implementado |
| Spawn assincrono | Implementado |
| Batalhas/ondas explicitas | Implementado |
| Warfare territorial | Implementado com `SPT_WarfarePoint` manual |
| Mapa tatico Warfare | Implementado no mapa nativo |
| Preview no editor | Implementado no GameMode selecionado |
| Persistencia entre reinicios | Pendente |
| Recaptura inimiga | Fora do escopo atual |

## Prioridade 1 — estabilizar configuracao manual Warfare

Objetivo: garantir que qualquer mapa configurado com `SPT_WarfarePoint` funcione sem depender de descritores automaticos.

Tarefas:

- validar compile limpo no Workbench;
- confirmar que pontos antigos foram recriados/reconfigurados com `Capture Order`;
- confirmar que a layer salva contem todos os pontos;
- melhorar mensagens de erro no preview;
- adicionar guia de setup no editor.

Critérios de aceite:

- HQ ordem `0` sempre SAFE;
- ordem `1` sempre vira frente inicial;
- ordem `2+` so libera quando a ordem anterior inteira foi capturada;
- mapa tatico mostra todos os pontos;
- JIP recebe snapshot correto.

## Prioridade 2 — polimento de validacao e debug

Tarefas:

- mostrar motivo do erro no preview, nao apenas cor magenta;
- logar resumo da sequencia de captura encontrada;
- logar pontos por ordem;
- criar comando/API de dump do estado Warfare;
- revisar atributos orfaos ou duplicados no GameMode.

## Prioridade 3 — APIs publicas para integracao

Adicionar ou completar:

- `ForceStartAttack(string pointId)`;
- `OnPointCaptured`;
- `OnPointUnlocked`;
- `OnPointStateChanged`;
- `OnPointAttackStarted`;
- getters seguros para progresso e estado por ponto.

Essas APIs devem permitir futuras missoes, economia, respawn e UI sem acessar mapas internos do componente.

## Prioridade 4 — notificacoes e UX

Tarefas:

- mostrar ETA de reforcos;
- avisar nova onda;
- avisar fim da defesa;
- evitar notificacoes antigas em JIP;
- configurar escala/cores dos icones do mapa.

## Prioridade 5 — persistencia

Fora do fluxo atual, mas importante para campanha longa.

Salvar:

- ownership dos pontos;
- estado Warfare;
- budget restante;
- cache de guarnicoes;
- batalhas ativas ou encerradas.

## Fora do escopo atual

- descoberta automatica de objetivos Warfare;
- modo hibrido;
- deduplicacao de descritores para pontos Warfare;
- vizinhos manuais por string;
- recaptura inimiga;
- economia;
- respawn dinamico do jogador;
- encerramento automatico obrigatorio.
