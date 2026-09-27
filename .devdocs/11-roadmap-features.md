# 11 — Roadmap de features e correções (backlog com IDs)

> Documento de planejamento do fork. Cada item tem um ID estável usado nos
> commits (`BUG-01a`, `FEAT-02`, ...). Antes de implementar um item, preencha o
> checklist R5 do `10-padrao-desenvolvimento.md` e consulte o
> `09-mapa-economia-taxas.md` quando o item tocar economia.
> Estado: `proposto` → `aprovado` → `em-andamento` → `feito` (com commit).
> Ordem sugerida por risco/valor; itens marcados ⭐ foram pedidos pelo usuário.

---

## P0 — Infraestrutura de testes (pré-requisito de TUDO)

| ID | Item | Estado | Notas |
|---|---|---|---|
| TST-01 | `enable_testing()` + `tests/` no CMake + harness headless commitado (sucessor do `simtest` perdido) | **feito** (`d42a9889`) | 3 camadas em `ctest` (unit/sim/save); harness em `tests/headless_env.*`, dados transformados em `build/test-install` (target `test-appdata`). |
| TST-02 | Camada 1: extrair e testar fórmulas puras (`compute_income_tax`, `compute_goods_tax`, `compute_export_discount`, `compute_interest`, `compute_cost`, `maintenance_cost`, `clamp_money`, `num_to_ansi`, `Stat`) | **feito** (`debc0a1b`) | Fórmulas puras em `src/lincity/economy.hpp` (novo); unit tests em `tests/unit/test_economy.cpp` e `test_util.cpp`. |
| TST-03 | Camada 2: invariante de conservação (dinheiro + mercadorias) com seed fixa, ações aleatórias, ASan | **feito** (`1ffabf20`, `f40a2c8f`, `dcdae425`) | Dinheiro: exato (base = caixa + contas acumuladas), valida ações aleatórias (500 build/demolish) + 300 dias, roda limpo sob ASan/UBSan. **Mercadorias**: ledger `World::commodityLedger` (produce/consume/level/destrutor) + checagem diária em `run_days` — soma dos inventários = base + ledger, para **todas** as rotas (cidade nova, 6 cenários, A/B de elasticidade, ações aleatórias). Prova negativa: hook com vazamento de +3 food/dia foi detectado no dia 1. |
| TST-04 | Camada 3: round-trip de save para os 6 cenários + saves de teste | **feito** (`debc0a1b`/`d42a9889`) | `tests/save/savetest.cpp`: round-trip dos 6 cenários + cidade nova via `cmake --install` de test-appdata. |
| TST-05 | CI no GitHub Actions: `ctest` (3 camadas) em push/PR, variantes regular + ASan | **feito** (`89169252`, PR #2) | `.github/workflows/ci.yml`: matriz 2 jobs (`build` RelWithDebInfo ~2m43s, `build-asan` Debug+ASan/UBSan ~3m56s com `detect_leaks=1`). Container `ubuntu:26.04`; fmt 11.1.4 e SDL3_mixer 3.2.4 buildados do source (não em apt). LSAN suppressions herdadas do CMake `ENVIRONMENT`. **CI verde**: https://github.com/maicomferre/lincity-ng/actions/runs/32062202943 |
| TST-06 | Modo de debugging em desenvolvimento: facility `util/debuglog` (áreas/níveis/env/sink) + driver `contrib/devlog.sh` (dirs por data, `latest`, `run.json`, gdb, retenção) + filtro/timestamps no `tiny_test` | **feito** | `util/debuglog.{hpp,cpp}`; `LNG_LOG(area, level, fmt...)`; determinismo preservado (R9). Instrumentação inicial: `sim` (do_time_step), `econ` (income/expense + resumo ano), `save` (load/save/ldsv), `vehicles` (spawn/destroy/move_frame), `frames` (removeFrame/moveFrameTo). Testes camada 1 em `tests/unit/test_debuglog.cpp` (defaults, env, sink, out-of-range). **Subárea `gui` futura** reservada (não inflar enum). |
| TST-09 | Gate de warnings (`-Wall/-Wextra` ou equivalente) e análise estática gradual no CMake/CI | **feito** (`33a769a5`, PR #22, merge `ebd42a77`) | `-Wall -Wextra -Werror` compila por padrão (`LINCITYNG_WARNINGS_AS_ERRORS=ON`); baseline reduzida com commits granulares, remoção de código morto comprovado e death tests forkados compatíveis com ASan. CI regular/ASan verde no workflow `32579379105`. |

**Regra**: NENHUMA feature de gameplay entra antes do TST-01+TST-03 existirem
(ver `10` §3 "Regra de ouro").

**Regra vigente da auditoria**: conteúdo econômico, commodity, transporte ou
algoritmo novo também espera `TST-07`, `BUG-14/15`, `BUG-18`, `REF-06` e
`TST-08`; `TST-09` é o gate de qualidade progressivo e deve acompanhar a fase
de infraestrutura.

---

## P1 — Bugs do usuário (fazer primeiro)

| ID | Item | Estado | Resumo da correção |
|---|---|---|---|
| ⭐ BUG-01a | "Novo Jogo" mostra a cidade anterior (MiniMap/texturas velhas + autosave desligado no jogo novo) | **feito** (`7470e028`) | `Game::setWorld` faz reset completo: fecha diálogos, marca MiniMap/GameView sujos, reinicia `lastAutosaveDay`/`Tick`. |
| ⭐ BUG-01b | Separar slot do autosave do slot do Continue (decisão de design) | **feito** (`7eae2cfe`) | Autosave → `autosave.scn.gz` (throttle); `9_currentGameNG.scn.gz` só ao sair para o menu/QUIT; Continue = `9_currentGameNG` → fallback `autosave` → cidade nova. |
| BUG-01c | Limpar seleção de cenário ao abrir "Novo Jogo" | **feito** (`7eae2cfe`) | `newGameButtonClicked` desmarca a seleção anterior. |
| BUG-01d | Feedback de origem do jogo ("Novo jogo criado" vs "Continue") | **feito** (`7eae2cfe`) | `printStatusMessage`: "Continue: game loaded." / "New game started." |
| ⭐ BUG-02 | Tela de carregamento (barreira até texturas prontas + fix da race H6) | **feito** (`03950e1c`) | Overlay "Loading…" em `Game::run` (Document+Paragraph, descarta input, trata QUIT/PIXEL_SIZE_CHANGED); `remaining_images`/`textures_ready` viraram `std::atomic`. |
| BUG-03a | Windmill: `static bool paid` compartilhado | **feito** (`7bb16565`) | `paid` virou membro de instância (dtor sem estado compartilhado). |
| BUG-03b | Escola e eólica não debitam manutenção do caixa | **feito** (`1ae9f923`) | Escola: `compute_school_cost(busy,…)` regressivo por lotação; eólica: `expense(1)` a cada `WIND_POWER_RCOST` dias **só quando gera**. Flags `schoolRunningCost`/`windPowerMaintenance`. |
| BUG-03c | Meses não re-traduzem ao trocar idioma | **feito** (`3db29915`) | `retranslate_months()` chamado no `reloadGUI()` (`util.cpp`). |
| BUG-04 | Ponte: exibida ×500 mas cobrada ×1 | **feito** (`a92e0d13`) | `transport_group_at()` resolve track/road/rail↔ponte e `World::buildConstruction` cobra o grupo resolvido quando `bridgeRealCost` (flag, default on). |
| BUG-05 | Crash ao abrir MPS finance (overflow de parágrafos) | **feito** (`e88a6e9e`) | `refreshCashFlow` escrevia 14 linhas vs 13 `mps_textN`; `PARAGRAPH_COUNT` 14 + `addText` guarda índice inválido. |
| BUG-07 | SEGV intermitente na borda do mapa (veículo lendo fora) | **feito** (`22e2ea43`) | `Vehicle::acceptable_heading` passa a checar `world.map.is_inside(dest)` antes de acessar. |
| BUG-08 | Race da thread de carregamento de música/efeitos do som | **feito** (`d7274d19`) | Ctor dá join na loader thread depois do `loadMusicTheme`; dtor guarda `loaderThread`. |
| ⭐ BUG-13 | Carros somem <0,5s durante curvas e reaparecem | **feito** | `Vehicle::walk` mantém `ExtraFrame` em `prev` ou `point`, nunca no vértice diagonal que pode ser prédio; `GameView` ordena frames por camada/profundidade, com transporte antes de veículos e construções. Invariante no simtest: `framePt` pertence ao segmento lógico. Detalhes em `.devdocs/12` §BUG-13. |

---

## P2 — Features ⭐ do usuário (na ordem de implementação)

### FEAT-01 — Custo de estrada em tempo real perto do mouse + texto negativo ao concluir

**Estado: FEITO** (`a92e0d13` BUG-04 + `c62b023d` FEAT-01). Custo itemizado
flutuante perto do cursor durante o drag (`Estrada N×$X + Ponte M×$Y = $Z`,
grupo resolvido via `transport_group_at` quando o drag passa por água); ao
soltar, texto flutuante negativo `-{:n}$` com o total debitado (paths de drag
e clique único). Ver testes: `bridge_charges_resolved_cost` no sim suite.

**Pedido**: durante o drag da estrada, mostrar o custo estimado em tempo real
perto do cursor; ao soltar, um texto negativo flutuante com o custo total.

**Referência SimCity 2013** (pesquisa `drafts/04-upstream-e-web.md` §3): o
SimCity documenta custo **por tile no tooltip do tipo de estrada** antes de
construir (Dirt $1, Low $2, Med $4, High $7, Avenues $7/$9 — [IGN Road
Types](https://www.ign.com/wikis/simcity/Road_Types)), construção por
arrastar-e-soltar ([IGN Laying
Roads](https://www.ign.com/wikis/simcity/Laying_Roads)), ponte/túnel entrando
no custo automaticamente ([GameFAQs](https://gamefaqs.gamespot.com/pc/663025-simcity/faqs/66674))
e o gasto saindo do saldo ao construir. **Não existe fonte citável** de
"readout flutuante perto do cursor" nem de "texto negativo ao concluir" no
SimCity 2013 — esse efeito é especificação nova do fork; a parte com
precedente é o custo por unidade visível durante a escolha.

**Estado do código (verificado)**: o cálculo já existe — `GameView::draw`
(`GameView.cpp:1543-1638`) soma `buildCost()`/`bulldozeCost()` por tile do drag
e hoje manda o total para a **barra de status**. Falta: (1) desenhar o total
perto do cursor (padrão `TooltipManager.cpp:90-118`, `Document`+`Paragraph`),
(2) no `MOUSEBUTTONUP` (`GameView.cpp:755-828`), montar o texto flutuante
negativo com expiração (`SDL_GetTicks` no draw; posição via
`getScreenPoint`), formato `fmt::format("{:n}")` (padrão já usado no fork).

**⚠️ Pré-requisito — bug latente de custo de ponte (BUG-04, DECIDIDO)**: a
ponte é **exibida** ×500 (`GameView.cpp:1736`) mas **cobrada** ×1
(`world.cpp:407` cobra antes do swap do grupo em `track_road_rail.cpp:376-388`)
e **demolida** ×500 (`lintypes.cpp:312`). Decisão: **cobrar o valor real**
(usar o grupo resolvido/`is_water()` em `world.cpp:401-415`) — mudança de
gameplay → flag de config (R4) + entrada no `09` + teste unit.

**Valores reais (confirmados em `transport.hpp`)**: 500 **não é o preço** — é o
multiplicador `BRIDGE_FACTOR` sobre a fórmula da estrada, que varia com o
tech (`custo = cost × (1 + cost_mul × tech/1e6)`, `lintypes.cpp:899-902`):

| Tipo | cost base | cost_mul | Custo em tech 0 | Custo em tech 1e6 | Bulldoze |
|---|---|---|---|---|---|
| Estrada | 100 | 25 | $100 | $2.600 | $50 |
| Ponte de estrada | 50.000 | 2 | $50.000 | $150.000 | $25.000 |
| Trilha (track) | 1 | 25 | $1 | $26 | $10 |
| Ponte de trilha | 500 | 2 | $500 | $1.500 | $5.000 |
| Ferrovia | 500 | 10 | $500 | $5.500 | $1.000 |
| Ponte de ferrovia | 250.000 | 2 | $250.000 | $750.000 | $500.000 |

**Exibição do custo em tempo real (DECIDIDO — itemizado)**: durante o drag,
contar separadamente tiles de terra e de água e mostrar linhas quando houver
ponte: `Estrada 3×$100 + Ponte 1×$50.000 = $50.300` (valores dinâmicos por
tech, formato `fmt::format("{:n}")`). Ao soltar, o texto negativo flutuante
mostra o **TOTAL** debitado (a soma das linhas). Isso dá transparência sobre a
criação dinâmica de pontes (qualquer tile de água no trajeto vira ponte).

**Risco**: baixo/médio. Não mexe em save (custos não persistem), não mexe em
simulação; mexe em `GameView` (draw/eventos) e, com o BUG-04, em
`World::buildConstruction` (economia — consultar `09` §5.11).

---

### FEAT-02 — Painel de informações do mapa na lista de cenários

**Estado: FEITO** (`485c34b7`). Painel com **thumbnail dinâmico** (widget
`MapThumbnail`, 1px/tile via `minimapColorNormal` extraída do MiniMap) +
**texto sempre** (nome, descrição traduzida, dinheiro/tech/ano/população/
tamanho; população só quando > 0); sem seleção → painel oculto; worlds
carregados uma vez e cacheados (`infoWorlds`). Ver teste
`scenario_thumbnail_and_panel_data`.

**Pedido**: à direita da lista de mapas (Novo Jogo), ao passar o mouse/clicar,
mostrar características do mapa (construções iniciais, população, época,
recursos, detalhes) e, idealmente, um pequeno screenshot; sem seleção → nada.

**Referência SimCity 2013**: cada "city site" da região lista seus recursos e
acessos ao ser escolhido ("Wessex Bend – Slight crude oil and raw ore
deposits… access to trading routes along the River Stuart" — [IGN Viridian
Woods](https://www.ign.com/wikis/simcity/Viridian_Woods); [EA Multi-City
Scale](https://www.ea.com/en-gb/news/simcity-multi-city-scale)).

**Estado do código**: `data/gui/newgame.xml` tem a **coluna 3 (rel 15) vazia** —
encaixe natural; lista populada em `MainMenu.cpp:135-162` (nomes hardcoded,
sem descrição). Dados disponíveis **sem carregar o mapa**: bloco `<globals>` do
`.scn.gz` (dinheiro, tech, tempo/época, população, `money_rates`, clima) via
mini-parser que para antes do `<map>` (`xmlloadsave.cpp:446-565`). Contagem de
construções exige load completo (`stats.groupCount` não é persistido).
**Thumbnails não existem** — gerar reusando a lógica de pintura do
`MiniMap::refreshTexture` (`MiniMap.cpp:652-679`) ou criar imagens estáticas.

**Plano**: (1) painel de texto (nome, descrição traduzida local
`given_scene → descrição`, dinheiro inicial, tech, ano, população, tamanho,
taxas) ligado à seleção (`newGameSelection.selected`) — hover opcional via
hit-test no `MainMenu::run` (`Component::relative2Global`); (2) **DECIDIDO:
thumbnail gerado dinamicamente quando disponível** (pré-carregar os 6 Worlds
em background é viável, dezenas de ms cada), com **texto sempre presente**
como base — se a geração falhar/estiver indisponível, o painel continua só
com texto.

**Risco**: baixo. Só GUI (`newgame.xml`, `MainMenu.cpp`, `src/gui` widgets);
zero impacto em save/simulação. Tradução: novas strings → PO pt_BR.

---

### FEAT-03 — Extração de petróleo + gestão de porto + visão de governador (projeto grande, em fases)

**Pedido**: clarear a mineração, adicionar petróleo, planejar melhor o porto e
a taxa do governo, com a visão do jogador como governador — com a exigência de
**planejamento total antes de codar** (proibido "lençol curto").

**Base obrigatória de leitura**: `09-mapa-economia-taxas.md` (inteiro) —
mineração §5.4-5.5/§8, porto §5.6, checklist de commodity nova §8.2/§12,
métricas de gestão §9, invariantes §10.

**Fases propostas (cada fase = feature fechada e testável, NUNCA parcial):**

| Fase | ID | Conteúdo | Risco | Pré-requisito |
|---|---|---|---|---|
| 0 | FEAT-03a | Documentar/definir "gerir bem/mal" como KPIs testáveis (fluxo de caixa por categoria, dole, fome, tech, reservas, estoques) e expor no MPS/GameStats | **feito** (`1861274a`, PR #18 merged em `6bff04a9`) | snapshot compartilhado entre MPS/GameStats; sem fórmula nova, save ou simulação; teste headless da UI |
| 1 | FEAT-03b | **Corrigir a contabilidade antes de criar nada** ✅ DECIDIDO: escola com custo regressivo por lotação (`busy`), eólica debitando quando gera (por instância), MPS "Net" batendo com Δmoney, `export_tax` (unused) decidido, ponte BUG-04 | **feito** (`1ae9f923` + `a92e0d13`; MPS "Net": ver `09` §10.4 — continua documentado, não é Δmoney exato) | TST-01/03 |
| 2 | FEAT-03c | **Painel do governador** ✅ DECIDIDO: alíquotas EDITÁVEIS (`money_rates.*`, clamps 0–2×, efeito imediato) + Etapa 2 com elasticidade (impostos altos → indústria produz menos, residências migram → menos arrecadação, possível prejuízo) | **feito** (`4ee08c12`, `d009d5d7`, `5e5e521c`) | Etapa 1: `Dialog` tipo GOVERNOR + `governor.xml` (6 alíquotas, clamps 0–2× default, efeito imediato). Etapa 2: `taxElasticity` (default on) — `compute_tax_burden_labor` no gate de produção (light/heavy industry) e `compute_tax_discomfort` na `desireability` da residência; inerte nas alíquotas padrão. Testes: `tax_elasticity_ab_reduces_activity` + `game_flow` abre o governador. |
| 3 | FEAT-03d | **Petróleo** como commodity nova seguindo o checklist do `09` §8.2 (enum no fim, jazidas no setup, poço/refinaria, tributação na entrega como coal/ore, porto, transporte, save `ldsv_version`) | alto (formato de save, mapa, GUI, sprites, help) | TST-01/02/03/04; `09` atualizado junto |
|     | FEAT-03d-a | Plumbing: enum STUFF_PETROLEUM, nomes, port rates, maxload=500 | **feito** (`e556fbb0`) | sem impacto save, CTest+ASan 4/4 |
|     | FEAT-03d-b | Jazidas petroleum no mapa + save/load + ldsv_version 2144 | **feito** (`6e1907be`) | round-trip save test, ASan 4/4 |
|     | FEAT-03d-c | Poço + refinaria + GUI (sprites, help, MPS, images.xml) | **feito** (`b9dfd960` + fix `83cc4552`) | CTest+ASan 4/4; docs 08/09 atualizados |
| 4 | FEAT-03e | **Porto 2.0**: revisão das regras (taxação dupla de exportação, `PORT_*_RATE` FIXME, frações, comércio sorteado) com base no nerf upstream (#87/#139) | alto (economia central) | FEAT-03b/c; simulação comparativa A/B |
| **FEAT-03e** | **Porto 2.0** | **feito** (`88126ed3` + tests) | Option B (porto isento de taxable) + rates ORE 1→50, STEEL 100→30, PETROLEUM_RATE=40 benchmarkado + testes camadas 1-2 | `09` §5.6/§10.4/§14.2; `13` |

**Checklist R5 — FEAT-03a antes da implementação**:

- simulação/economia: **sem impacto**; somente leitura de `World::stats`,
  `total_money`, `tech_level` e reservas já existentes;
- taxas: o fluxo anual por categoria continua na página Cash Flow; não há
  imposto, tarifa, fee ou arredondamento novo;
- stats/inventário: impacto apenas de apresentação; população, desemprego,
  fome, poluição, reservas e estoques usam acumuladores existentes;
- minimapa/MPS: adiciona a página Gestão ao `GlobalMPS`, mantendo Cash Flow,
  Costs e Population;
- save/load: **sem impacto**, nenhum campo novo e nenhum bump de
  `ldsv_version`;
- tradução/help: adiciona rótulo da página e tradução pt-BR; sem help novo;
- GUI/config: **sem flag**, pois não há mudança de gameplay;
- determinismo/RNG: **sem impacto**, nenhuma ordem de simulação ou sorteio
  muda.

**Regras anti-"lençol curto" específicas deste projeto**: (1) cada fase tem
matriz de interação própria (R5) listando TODAS as taxas/commodities afetadas
(`09` §5); (2) simulação A/B com a mesma seed: cidade idêntica com flag
on/off, diffs de caixa/estoques explicados; (3) invariantes rodando em todas
as fases; (4) nenhuma fase entra sem a anterior concluída e testada.

---

## P3 — Quick wins (baixo risco, alto valor percebido)

| ID | Item | Origem |
|---|---|---|
| QW-01 | Sons de foguete (assets já existem; 2 linhas) | **feito** (`3498da4f`) |
| QW-02 | Scroll speed configurável | **feito** (`593eac43`) |
| QW-03 | Typos #367 e bugs baratos upstream #360/#363/#364/#366 | **feito** (`ed38da84`, `1be0cab1`, `4982a7c4`, `7567c26f`) — #360 display do tamanho jogável; #366 clamp 50–10000 no menu; #364 tecla 'p' do lago; #367 typos de help (market/oremine/query). #363 e "Congradulations"/"yourcredit" já estavam corrigidos no upstream (`7e4d03dd`). |
| QW-04 | Tooltip do build menu com custo + manutenção (padrão SimCity/IGN) | **feito** (`e705767c`, PR #20, merge `41b2f2e6`) — custo de construção, custo operacional estimado, periodicidade e flags existentes; sem nova cobrança |
| QW-05 | `Sound.cpp:376` — entrada da música não atualiza no options | **feito** (`d4eb067f`, PR #19, merge `74eedff3`) |
| QW-06 | `MainMenu.cpp:521` — ordenar resoluções | **feito** (`a642041e`, PR #21, merge `0dd35455`) — opções filtradas, normalizadas, ordenadas e sem duplicatas; guarda para lista SDL nula. |

## P4 — Dívida estrutural (agendar com calma)

| ID | Item | Origem |
|---|---|---|
| REF-01 | Desacoplar engine→GUI (`getConfig`/`simDelay`/`getSound`/`Mps.hpp`) | `10` §1, `drafts/02` §2a — destrava testes unitários puros |
| REF-02 | `tech_level` numa camada única (`World::addTech`) | `drafts/02` §2b |
| REF-03 | `framesptr` → `unique_ptr<list>` + métodos `MapTile::addFrame/removeFrame/moveFrameTo` | `10` §1.5/R8, `drafts/02` §2d — **análise profunda em `.devdocs/13-framesptr-ciclo-de-vida.md`** (ponto crítico nº 1 de memória; causa estrutural do crash histórico #52). Fases: **REF-03a** (migrar duplicações `school.cpp:82-90` e `Vehicles.cpp:329-342` para métodos existentes — **feito**, PR #1 mergedo `7b7d4bff`) → **REF-03b** (encapsular `framesptr` privado + accessors + asserts debug de ownership — **feito**, PR #3 mergedo `b3207a28`; camada 1: `tests/unit/test_maptile_frames.cpp` com 5 invariantes incl. prova do assert via fork) → **REF-03d** (canary/invariantes em debug — **feito**, PR #4: registry global de frames vivos + `Map::assertFramesConsistent()` no fim de cada `do_time_step`; matriz sim+save inteira passou limpa) → **REF-03c** (`unique_ptr<list>` — **feito**, PR #5: MapTile não-copiável + move ctor, Map move-assign para o load path, últimos `delete framesptr` eliminados; **junto veio BUG-12** — set de constructions ordenado por ponto, sim agora bit-reprodutível entre runs/builds/máquinas, ver `.devdocs/12` §BUG-12) → **REF-03e** (`Vehicle::delete this` → remoção tardia — **feito**, PR #6: `dead` flag + reaping no `do_animate`, dtor sem unlist, 2 statics mortos removidos, invariante de sweep no simtest). **Plano REF-03 completo (fases 0–4): zero `delete` manual de frames, zero `delete this` no engine, ciclo de vida de ExtraFrame fechado** (ver `.devdocs/13` §10). **Zero impacto em save** (frames não persistidos — R3 não se aplica) nem em gameplay/determinismo. Ordem 0→1→3→2→4. |
| REF-04 | Penalidade de falência (3 TODOs `simulate.cpp`) | 08 P4 — decide com #220 upstream |
| REF-05 | Revisar cenários (#187 upstream: good_times com substations desconectadas; #30 Bad Times impossível) | `02` |

## Auditoria de qualidade 2026-08-20 — sequência obrigatória antes de novas features

Esta seção é a fila vigente para estabilidade. O documento `14` contém a
análise, estimativas e critérios de decisão; este roadmap contém os IDs que
devem aparecer nos commits e no diário.

### P0 — Falhas que podem invalidar o estado do jogo

| ID | Item | Estado | Critério de saída |
|---|---|---|---|
| TST-07 | Corrigir `contrib/devlog.sh`: propagar retorno real do CTest e tornar `logs/latest` seguro em concorrência | **feito** (`0204082c`, PR #9 merged em `fa9da6cc`) | teste negativo e duas execuções simultâneas passam; CTest local/CI 4/4 |
| BUG-14 | Encerrar o tick imediatamente após exceção de simulação; não executar save limpo/autosave com mundo inválido | **feito** (`71aaec79`, aguardando PR/CI) | os dois caminhos de exceção retornam de `Game::run` após crashsave/status, sem `backToMainMenu` nem pós-tick; regular e ASan/UBSan locais 4/4 |
| BUG-15 | Tornar resultado de save verificável e remover `assert(false)` como mecanismo de recuperação | **feito** (`31fdf897`, aguardando PR/CI) | `saveCityNG`/`autoSaveCityNG` retornam bool, capturam `std::exception` por referência, escrevem temporário e só substituem após rename; teste de I/O confirma retorno, temp limpo e slot anterior preservado |

### P1 — Corrupção silenciosa, compatibilidade e contabilidade

| ID | Item | Estado | Critério de saída |
|---|---|---|---|
| BUG-16 | Persistir o resto fracionário do porto ou remover a dependência dele | **feito** (`4af6a43f`, PR #13 merged em `6bff5e35`; CI regular/ASan verde) | `pence` persiste como tag do `Port`, `ldsv_version` 2140→2141; saves antigos carregam zero por default e round-trip dedicado passa |
| BUG-17 | Fechar ciclo de vida do loader de texturas: `stopThread` sincronizado e contador só concluído após sucesso | **feito** (`bbe83e9c`, PR #14 merged em `480930f4`; CI regular/ASan verde) | `stopThread` é atômico; falhas de leitura/conversão encerram a barreira; contador só decrementa após textura criada; fluxo headless e ASan local passam |
| BUG-18 | Fazer `income/expense` e clamps com aritmética segura (`int64_t`) antes da conversão | **feito** (`3115952a`, PR #16 merged em `33f9cf3f`) | `income/expense` usam intermediário largo; juros/clamp aceitam limites largos; unit de limites, sim de conservação 4/4 e CI regular/ASan verde |
| BUG-19 | Revisar registro/destruição de `ResourceGroup` e ruído de teardown | **feito** (`d2a34eea`, PR #18 merged em `6bff04a9`) | `resetGraphics()` libera estado por `Game`; destrutor reutiliza a rotina e não emite falso erro quando o harness troca o registro para teardown |
| BUG-20 | Estado de câmera `static` em `GameView::viewportUpdated` vazava entre instâncias | **feito** (`8df8a5b1`, PR #18 merged em `6bff04a9`) | centro/zoom viraram estado da instância; caminho sequencial de jogos passa no sim regular e ASan/UBSan |
| BUG-21 | Singleton `ButtonPanel` e vetores gráficos globais sobreviviam ao jogo anterior | **feito** (`ed11dac1`, PR #18 merged em `6bff04a9`) | destrutor limpa singleton; `GameView` reseta gráficos/imagens dos grupos; dois jogos consecutivos passam sem assert/duplo estado |
| BUG-22 | `GameStats` referenciava `dialogbutton` sem definir o estilo localmente | **feito** (`07cc81be`, PR #18 merged em `6bff04a9`) | estilo declarado em `gamestats.xml`; abertura headless coberta pelo teste de KPIs |
| REF-06 | Centralizar mutações de `tech_level` e `taxable` em APIs de `World` | **feito** (`7610985e`, PR #17 merged em `e00a14b8`; CI regular/ASan verde) | grep não encontra mutações econômicas fora das APIs/load; teste de limites, CTest regular 4/4 e CI regular/ASan verdes |
| TST-08 | Persistir estado do RNG para continuação determinística, ou registrar decisão contrária | **feito** (`dc0e31c7`, PR #15 merged em `65e1d411`) | `BasicUrbg` e estado de simulação relevante são preservados; teste compara 200 dias pós-load; CTest regular/ASan 4/4; `ldsv_version` 2142 |

### P2/P3 — Evolução de gameplay com custo controlado

| ID | Item | Estado | Dependências e decisão necessária |
|---|---|---|---|
| FEAT-04 | Ledger por construção: compra, capital investido, manutenção, operação e receita separados | **feito** (`a3f2c1d0`) | 5 campos em `Construction`; save 2145; MPS LEDGER page; `recordExpense`/`recordRevenue` hooks; tests camadas 1-3 |
| FEAT-05 | Sea farm inspirada no upstream #339 | **feito** (`d1dc6a88`) | Módulo em sea_farm.hpp/cpp; water-tile can_build_here; constantes em all_buildings.hpp; sprites/images.xml; help + pt_BR; save/load; tests camadas 1-3 |
| FEAT-06 | Poluição hídrica inspirada no upstream #338 | **proposto** | modelo bounded, custo de CPU medido e A/B com ecologia atual |
| REF-07 | Scheduler/eventos determinísticos para transporte, ecologia e RNG | **feito** — Fases 0–6 concluídas | RNG v1 pertencente ao `World`, domínios keyed e cosméticos isolados; save 2143 com protocolo/seed/versão e lacunas de trajetória persistidas; Legacy golden regenerado (`64f6bd4b…`); plano em `15`, relatórios em `16`–`21`; Fase 6: extração transacional do transporte (snapshotTransport → resolveTransport → applyTransport) |

---

## Decisões tomadas (2026-08-16 — usuário)

1. **BUG-01b — SEPARAR os slots** ✅: autosave em `autosave.scn.gz` (slot
   próprio); Continue lê `9_currentGameNG.scn.gz` (escrito só ao sair para o
   menu/QUIT). Cadeia do Continue: `9_currentGameNG` → fallback `autosave`
   (se existir) → senão cidade nova (comportamento atual).
2. **BUG-04/ponte — cobrar o valor real e exibir itemizado** ✅: a ponte passa
   a ser cobrada pelo custo de verdade (500× a estrada, ver tabela no
   FEAT-01) e o custo em tempo real mostra **linhas separadas** quando o drag
   inclui água: `Estrada +x | Ponte +y | TOTAL z` — transparência de como o
   custo é composto (o multiplicador de ponte é dinâmico: criada sempre que o
   drag passa sobre água).
3. **BUG-03b — escola e eólica** ✅:
   - **Eólica**: quando **gera** (tem trabalho e espaço de estoque — condição
     real em `windpower.cpp:87-88`), debita manutenção do caixa
     (`World::expense`, por instância — sem `static`); quando **não gera**, sem
     custo na v1 ("parada = 0"). O lucro/prejuízo líquido da geração entra na
     contabilidade do estado (fase 1: só o débito; fase 2/governador: receita
     de energia consumida — ver FEAT-03c).
   - **Escola**: custo **fixo regressivo pela lotação**: lotação = `busy`
     (dias operados nos últimos 100, `school.cpp:101-110`). Fórmula proposta:
     `custo = SCHOOL_RUNNING_COST × (2 − min(1, busy/50))` → lotação 0% = teto
     2× ($4/dia); 50% = 1× ($2/dia); 100% = piso 0,5× ($1/dia). Menos alunos,
     mais custo por aluno para o estado. Flag de config (R4) + função pura
     `compute_school_cost(busy)` com unit test.
4. **FEAT-02 — thumbnail** ✅: painel mostra o **thumbnail quando houver**
   (gerado dinamicamente com a lógica do MiniMap) e **texto sempre**; sem
   seleção → painel oculto.
5. **FEAT-03c — alíquotas EDITÁVEIS** ✅: UI de governador permite editar
   `money_rates.*` (income/coal/ore/goods/dole/transport) com clamps (0–2× o
   default) e efeito imediato nas fórmulas anuais (`simulate.cpp:183-235`).
   Em etapas:
   - **Etapa 1 (arrecadação)**: só a fórmula muda — baixo risco, sem loops.
   - **Etapa 2 (elasticidade — o "prejudicar")**: impostos altos afetam o
     comportamento: (a) indústrias reduzem produção/empregam menos
     (custo tributário entra no gate de produção de light/heavy industry);
     (b) residências migram: termo de imposto entra na `desireability`
     (`residence.cpp:372-390` good−bad) → menos moradores → menos trabalho →
     menos base de income tax → **menos arrecadação do estado, podendo virar
     prejuízo** (loop completo e testável: simulação A/B mesma seed,
     imposto 2× → prever Δpopulação/Δreceita e conferir).
   - Regra de conservação: a elasticidade **reduz fluxos tributáveis**; não
     cria nem destrói dinheiro fora dos pontos do `09` §10 — invariantes
     continuam válidos em qualquer alíquota.


---

## Implementado (sessão 2026-08-16) — resumo de commits

Todos na `master` local (base `0b508d42` intocada; nada enviado ao upstream).
Mensagens descrevem apenas as alterações.

| Commit | Item | Conteúdo |
|---|---|---|
| `d42a9889` | TST-01 | infra de testes (ctest, harness headless, test-appdata) |
| `7470e028` | BUG-01a | reset completo em `Game::setWorld` |
| `03950e1c` | BUG-02 | barreira de loading + atomics |
| `e88a6e9e` | BUG-05 | fix do overflow de parágrafos do MPS finance |
| `22e2ea43` | BUG-07 | veículo lendo fora do mapa na borda |
| `7bb16565` | BUG-03a | windmill `paid` por instância |
| `d7274d19` | BUG-08 | join da thread de som |
| `7eae2cfe` | BUG-01b/c/d | slots separados, limpar seleção, mensagens de origem |
| `3db29915` | BUG-03c | meses re-traduzem no reloadGUI |
| `debc0a1b` | TST-02/TST-04 | `economy.hpp` + unit tests + round-trip de saves |
| `1ae9f923` | BUG-03b/FEAT-03b | escola regressiva + eólica debitando ao gerar (flags) |
| `a92e0d13` | BUG-04 | ponte cobrada pelo valor real (flag) |
| `c62b023d` | FEAT-01 | custo itemizado em tempo real + texto negativo |
| `485c34b7` | FEAT-02 | painel do cenário (thumbnail + texto) |
| `4ee08c12` | FEAT-03c | appdata transformado para o harness |
| `d009d5d7` | FEAT-03c | elasticidade tributária (flag) |
| `5e5e521c` | FEAT-03c | painel do governador (UI) |
| `ca2a7c9d` | TST-03 | sanitização de scancodes SDL (UBSan) + NSDMI em `Event` |
| `1ffabf20` | TST-03 | teste de ações aleatórias + base do invariante |
| `2b0dc7b3` | i18n | correção do pipeline de POT (dialogs, UTF-8, msgcat) |
| `f98ec66b` | i18n | traduções pt-BR das strings novas (36 msgids) |
| `3498da4f` | QW-01 | sons de foguete (takeoff/explosion) |
| `593eac43` | QW-02 | scroll speed configurável (config + menu Options + pt-BR) |
| `ed38da84` | QW-03 | #360 — menu mostra o tamanho jogável (worldSize−2) |
| `1be0cab1` | QW-03 | #366 — clamp do tamanho do mapa em 50–10000 no menu |
| `4982a7c4` | QW-03 | #364 — tecla do lago de parque 'k'→'p' + help en/pt_BR |
| `7567c26f` | QW-03 | #367 — typos no help (market 2%→20%, oremine coal→ore, query) |
| `f40a2c8f` | TST-03 | ledger de mercadorias no World (produce/consume/level/destrutor) |
| `dcdae425` | TST-03 | checagem diária do invariante de goods no `run_days` |

Pendências registradas: auditoria 2026-08-20 (`REF-07`,
`FEAT-04`…`FEAT-06`), FEAT-03d (petróleo),
FEAT-03e (porto 2.0), tradução de POs de outros idiomas (fallback inglês).
Os documentos canônicos desta sessão são `08`, `09`, `10`, `11`, `14` e `06`.

---

## Avaliação de viabilidade (2026-08-16) — itens pendentes

> Análise feita a pedido do usuário; **nenhum destes itens foi iniciado**.
> Ver também `.devdocs/04-traducao-ptbr.md` (POs) e `.devdocs/09` §8.2/§12
> (checklist de petróleo).

| Item | Esforço | Risco | Observações |
|---|---|---|---|
| FEAT-03a (KPIs de gestão) | **baixo** | baixo (só leitura/stats) | **Feito nesta sessão**: snapshot compartilhado no MPS/GameStats, página Gestão, estoques/reservas normalizados e teste headless. Sem impacto em save/simulação. |
| FEAT-03d (petróleo) | **alto** (multi-sessão) | alto | Checklist completo em `09` §8.2/§12: enum antes de `STUFF_COUNT`, bump `ldsv_version` 2140→2141, `MapTile` com jazida + save, `NUM_OF_GROUPS` 51→52+, sprites/help/GUI, maxloads (transporte/porto/mercado), linha de receita nova. Maior item da lista. |
| FEAT-03e (porto 2.0) | **médio-alto** | alto (economia central) | Decisões antes de codar: taxação dupla (`09` §10.4), `PORT_*_RATE` FIXME (`port.hpp:57`), resto fracionário (`port.cpp:132`), comércio sorteado. Exige simulação A/B. Pedaços baratos isoláveis (ex. `port.cpp:132`). |
| POs de outros idiomas | **baixo (mecânico)** / **alto (qualidade)** | baixo | 18 POs sem os 36 msgids novos → fallback inglês. `msgmerge` contra POT marca os ausentes; tradução de qualidade exige nativos (upstream aceita PRs, cf. #361 alemão). |
| QW-04 (tooltip custo+manutenção) | **baixo** | baixo (GUI-only) | **Feito nesta sessão**: adapter de apresentação reaproveita fórmulas/constantes e taxas já cobradas nos módulos; mostra período, faixa escolar, geração e flags desativadas; sem impacto em simulação/save. |
| QW-05 (música no options) | **baixo** | baixo (GUI-only) | **Feito nesta sessão**: `musicHalted` agora só sinaliza atomicamente; `MainMenu` consome a solicitação na thread principal e atualiza `musicParagraph`, enquanto `Game` mantém a troca automática. Sem impacto em save/simulação/economia/RNG. |

**Ordem recomendada vigente**: FEAT-03a (KPIs) →
QW-05/QW-04 → FEAT-03e (porto 2.0) → FEAT-05/FEAT-06 → FEAT-03d

---

## Modernização upstream (estudo 2026-09-27)

> Análise do upstream `lincity-ng/lincity-ng` master (59cb5312,
> versão 2.15.0) comparada com o fork atual (ldsv 2145).

### O que o upstream modernizou

| Área | Upstream | Fork atual | Esforço de migração |
|---|---|---|---|
| SDL | SDL3 3.2.0 (2.14.0+) | SDL2 (implícito) | **alto** — API de rendering, eventos, janela |
| GUI XML | libxml++ (2.14+) | libxml++ | compatível |
| Traduções | gettext/libintl, .mo binário, PO unificado | tinygettext, POs separados | **médio** — novo sistema de locale |
| Rendering | Painter, TextureManager, zoom | legacy SDL blit | **alto** — refatorar GameView |
| Config | LINCITYNG_CONFIG_FILE env vars | cfgpath + Config.hpp | baixo — adicionar env vars |
| Build | CMake 3.21, CMAKE_BUILD_TYPE=None, relocatable | CMake 3.x | baixo |
| Ajuda | help_templates/ regenerado | data/help/en/ manual | baixo — adotar templates |

### Itens gráficos visíveis do upstream

1. **Zoom** (GameView::zoomIn/zoomOut/zoomMouse) — fator 0.0312× a 4×
2. **SDL3 textures** — TextureManager, drawTile/drawTexture/drawOverlay
3. **Component refactor** — Painter-based, resize/Event handling
4. **Idioma sem reiniciar** — parcial (componentes já traduzidos não re-traduzem)
5. **AppStream metainfo** — desktop file, icon 256×256
6. **Ajuda regenerada** — help_templates/ → data/help/en/

### Decisão necessária

Migrar para SDL3 é o passo mais impactante. Alternativa: manter SDL2
e portar só as melhorias sem impacto em rendering (gettext, config,
help templates, zoom logic). Recomendado: fazer em fases.

Fase 1 (baixo risco): gettext, config env vars, help templates
Fase 2 (médio): zoom, component refactor, Painter
Fase 3 (alto): SDL3 completo, TextureManager

(petróleo). REF-07 entra antes de qualquer otimização estrutural de
transporte. FEAT-04 só entra depois de uma decisão explícita sobre o formato
de save.
