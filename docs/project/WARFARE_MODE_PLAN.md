# Plano de Implementação — Modo SPT Warfare

## Resumo

Criar `SPT_WarfareGameModeComponent` como coordenador autoritativo no servidor
para uma campanha de conquista territorial.

Cada área estratégica começa sob controle inimigo, aparece no mapa e participa
de um grafo territorial. A primeira baixa da guarnição inicia os reforços da
área. Eliminar somente os grupos locais CQB e de patrulha permite a captura;
ondas de reforço nunca bloqueiam nem revertem a propriedade do território.

O objetivo da primeira versão é oferecer uma campanha simples de entender,
desafiadora e compatível com os sistemas atuais de caching, budget e batalhas.

---

## 1. Arquitetura

### `SPT_WarfareGameModeComponent`

Componente central anexado ao GameMode, responsável por:

- registrar e validar todos os pontos Warfare;
- manter ownership e estado de cada área;
- calcular a frente territorial;
- observar baixas e limpeza das guarnições;
- iniciar batalhas pela API pública de reforços;
- confirmar capturas;
- replicar estado para clientes e jogadores que entrarem depois;
- controlar marcadores, notificações e condição de vitória.

O servidor será a única autoridade para captura, ownership, progressão e início
de reforços.

### `SPT_WarfarePointComponent`

Componente colocado no World Editor em cada área participante. Deve expor:

- ID estável e único;
- nome opcional;
- tipo da área;
- raio de ataque/captura;
- IDs dos pontos vizinhos;
- opção de marcar o ponto como HQ inicial;
- opção de herdar nome, tipo e posição do `MapDescriptorComponent` mais próximo;
- opção de habilitar ou excluir o ponto da condição global de vitória.

Tipos iniciais:

- cidade;
- vila;
- base ou área militar;
- aeroporto;
- ruína;
- porto;
- floresta;
- montanha;
- campo;
- personalizado.

### Descoberta híbrida

- Cidades, vilas, portos, aeroportos e outros descritores suportados podem ser
  detectados automaticamente.
- Componentes manuais adicionam florestas, montanhas, campos e áreas sem
  descritor.
- Um componente manual próximo a um descritor automático sobrescreve seus
  metadados e fornece ID, links e raio.
- Pontos automáticos sem links permanecem visíveis, mas bloqueados. A validação
  deve informar claramente quais configurações estão ausentes.

---

## 2. Grafo territorial e progressão

O avanço usa uma frente conectada.

- Um ou mais pontos começam como HQ azul.
- Um ponto inimigo é atacável quando é vizinho de um ponto capturado.
- A frente é recalculada por busca em largura (BFS) após cada captura.
- Pontos desconectados permanecem bloqueados.
- Todos os links são configurados explicitamente por IDs nos componentes.

Validações obrigatórias ao iniciar:

- IDs duplicados;
- IDs vazios;
- vizinhos inexistentes;
- links para o próprio ponto;
- ausência de HQ;
- pontos inalcançáveis a partir de qualquer HQ;
- descritores duplicados representando a mesma área.

### Ataques profundos

Jogadores ainda podem entrar fisicamente em uma área bloqueada.

Se eliminarem a guarnição:

- os defensores não respawnam;
- a limpeza fica registrada;
- o ponto continua vermelho e isolado;
- a captura fica pendente;
- quando a conexão territorial chegar, a captura é concluída se a presença já
  tiver sido confirmada durante a limpeza;
- caso contrário, um jogador precisa retornar ao raio para confirmar.

Isso preserva o caching finito e evita recriar inimigos artificialmente.

---

## 3. Estados das áreas

Criar enum de estado Warfare:

| Estado | Mapa | Significado |
|---|---|---|
| `LOCKED` | Vermelho escuro | Área inimiga desconectada da frente |
| `FRONTLINE` | Vermelho | Área inimiga disponível para ataque |
| `UNDER_ATTACK` | Laranja | Primeira baixa registrada e reforços autorizados |
| `CLEARED_WAITING` | Amarelo | Guarnição eliminada, aguardando presença ou conexão |
| `CAPTURED_DEFENDING` | Azul pulsante | Capturada, mas reforços inimigos ainda estão chegando |
| `CAPTURED` | Azul | Área conquistada e batalha encerrada |

Ownership e batalha são estados independentes:

- um ponto pode ser azul enquanto ainda há reforços inimigos;
- reforços não capturam nem recapturam;
- eliminar reforços não é requisito para mudar o ownership;
- recaptura inimiga fica fora da primeira versão.

---

## 4. Integração com guarnições

Expandir `SPT_WorldGarrisonManagerComponent` com uma interface pública estável
para o Warfare.

### Estado público por localização

Expor:

- ID estável;
- centro e nome;
- manpower atual da guarnição;
- manpower-alvo/inicial;
- quantidade de spawns locais pendentes;
- estado `cleared`;
- budget restante;
- batalha ativa;
- tempo estimado para a próxima onda.

### Eventos

Adicionar invokers para:

- primeira baixa da guarnição;
- guarnição local eliminada;
- batalha iniciada;
- onda agendada;
- onda materializada;
- batalha encerrada.

As contagens precisam continuar separadas:

- grupos CQB e patrulha contam para captura;
- battle groups, comboios, tripulações e sobreviventes de ondas não contam.

### Primeira baixa

O primeiro momento em que o manpower local fica abaixo do manpower inicial:

1. marca a área como `UNDER_ATTACK`;
2. registra que o gatilho já foi usado;
3. chama `StartLocationBattle(position)` exatamente uma vez;
4. informa aos jogadores a faixa de chegada da primeira onda.

Entrar ou atravessar a área sem causar baixas não inicia reforços.

---

## 5. Regra de captura

Uma área pode ser capturada quando:

1. o manpower da guarnição CQB/patrulha é zero;
2. não existe spawn de guarnição pendente;
3. existe ao menos um jogador vivo da facção configurada dentro do raio;
4. a área está conectada à frente azul.

Se a guarnição zerar sem jogador presente, usar `CLEARED_WAITING`.

Se a área estiver desconectada, guardar:

- limpeza confirmada;
- presença confirmada durante a limpeza;
- captura pendente por conexão.

Quando todos os requisitos forem satisfeitos:

- ownership muda para jogador;
- marcador fica azul;
- novos vizinhos são desbloqueados;
- se houver batalha ativa, usar `CAPTURED_DEFENDING`;
- caso contrário, usar `CAPTURED`.

---

## 6. Reforços e defesa após captura

As ondas usam exclusivamente a API pública já implementada:

```c
SPT_WorldGarrisonManagerComponent.GetInstance().StartLocationBattle(position);
```

Comportamento:

- primeira baixa autoriza as ondas;
- a primeira onda respeita o atraso configurado;
- ondas seguintes respeitam tempo ou limite de sobreviventes;
- captura não cancela ondas já planejadas;
- jogadores recebem aviso para preparar a defesa;
- o ponto continua azul durante a contraofensiva;
- ao terminar a batalha, muda de `CAPTURED_DEFENDING` para `CAPTURED`.

Se o dataset viário estiver ausente ou inválido:

- o Warfare continua funcionando;
- `CONVOY` usa fallback `SPREADED`;
- nenhuma captura ou progressão é bloqueada.

---

## 7. Marcadores de mapa

Cada ponto deve possuir marcador próprio, independente do descritor visual
original.

### Apresentação

- ícone específico por tipo;
- cor correspondente ao estado;
- atualização imediata ao atacar, limpar, capturar ou encerrar a defesa;
- tooltip com nome, categoria, estado territorial e situação dos reforços.

### Replicação

- servidor mantém o estado canônico;
- mudanças são enviadas por RPC confiável;
- `RplSave`/`RplLoad` fornece snapshot completo para JIP;
- clientes criam e atualizam seus marcadores localmente;
- não replicar `IEntity` nem `EntityID` como estado persistente.

---

## 8. HUD e notificações

Notificar somente transições importantes:

- “Área sob ataque” após a primeira baixa;
- “Reforços inimigos chegando em 30–90 segundos”;
- aviso de nova onda;
- “Área limpa — permaneça no local para confirmar”;
- “Área capturada — prepare a defesa”;
- “Área segura” quando a batalha terminar;
- mensagem de vitória global.

Cada transição ou onda gera no máximo uma notificação, evitando spam periódico.

---

## 9. Facções e configuração

Adicionar atributos ao componente Warfare:

- chave da facção dos jogadores;
- chave da facção inimiga;
- intervalo de atualização;
- prefab/configuração dos ícones por tipo;
- cores dos estados;
- habilitar notificações HUD;
- habilitar descoberta automática;
- distância máxima para vincular componente a descritor;
- exigir presença para captura;
- habilitar condição global de vitória.

Defaults devem ser compatíveis com o `FactionManager_USxUSSR` atual, mas todas
as chaves precisam ser configuráveis para compatibilidade com mods.

---

## 10. APIs Warfare

Expor métodos públicos:

```c
SPT_WarfarePointState GetPointState(string pointId);
bool IsPointCaptured(string pointId);
bool IsPointAttackable(string pointId);
bool ForceStartAttack(string pointId);
float GetCampaignProgress();
int GetCapturedPointCount();
int GetTotalPointCount();
```

Também disponibilizar invokers para captura, desbloqueio, mudança de estado e
vitória, permitindo integração futura com missões, economia e UI.

---

## 11. Condição de vitória

O servidor verifica vitória após cada captura.

Critério:

- todos os pontos habilitados para vitória estão capturados.

Resultado inicial:

- mensagem global;
- bloqueio de novos ataques;
- cancelamento de timers de Warfare;
- evento público `OnWarfareVictory`;
- encerramento automático da sessão fica configurável e desativado por padrão.

---

## 12. Testes manuais

1. Todos os pontos configurados aparecem vermelhos; HQ inicial aparece azul.
2. Somente vizinhos de território azul ficam `FRONTLINE`.
3. Entrar sem causar baixa não inicia reforços.
4. Primeira baixa inicia exatamente uma batalha e um aviso.
5. Baixas posteriores não reiniciam nem duplicam ondas.
6. Eliminar CQB/patrulha com jogador presente captura o ponto.
7. Battle groups e veículos não bloqueiam a captura.
8. Limpar sem presença mantém `CLEARED_WAITING`.
9. Limpar ponto desconectado não respawna defensores.
10. Ponto desconectado captura quando conexão e presença forem satisfeitas.
11. Captura desbloqueia os vizinhos corretos.
12. Marcadores atualizam para todos os clientes.
13. Jogador JIP recebe ownership e marcadores atuais.
14. Ondas continuam após captura e o ponto fica `CAPTURED_DEFENDING`.
15. Fim da batalha muda o ponto para `CAPTURED`.
16. Dataset viário ausente usa fallback sem quebrar a campanha.
17. Capturar todos os pontos dispara vitória uma única vez.
18. Logs confirmam transições, manpower, gatilho, ondas, BFS e ownership.

---

## 13. Entrega em fases

### Fase A — Contrato da guarnição

- IDs estáveis;
- consultas públicas;
- distinção formal entre guarnição e battle groups;
- eventos de primeira baixa, limpeza e ondas.

### Fase B — Núcleo Warfare

- componentes;
- registro e validação;
- estados;
- BFS;
- captura;
- integração com reforços;
- vitória.

### Fase C — Mapa e rede

- marcadores;
- replicação;
- JIP;
- HUD e notificações.

### Fase D — Configuração do mundo

- colocar/configurar pontos;
- definir HQ;
- montar links;
- validar todos os pontos e categorias.

### Fase E — Validação

- testes incrementais no Workbench;
- análise de logs;
- correções;
- documentação do procedimento final.

Commits semânticos separados:

- `feat(guarnicao): adiciona eventos e estado público para warfare`
- `feat(warfare): implementa conquista territorial conectada`
- `feat(warfare): adiciona marcadores e notificações`
- `chore(mapa): configura pontos e ligações warfare`
- `docs(warfare): documenta implementação e testes`

---

## Fora do escopo inicial

- persistência entre reinícios;
- recaptura inimiga;
- economia e recompensas;
- construção de bases;
- novos pontos de respawn;
- comandante estratégico;
- encerramento obrigatório da sessão;
- balanceamento dinâmico avançado por quantidade de jogadores.

Esses sistemas devem usar as APIs e eventos do Warfare em fases posteriores.
