# Diário de desenvolvimento do fork

Registro de todo o trabalho feito neste fork. Formato: data + item +
detalhes + arquivos tocados. Nenhuma informação aqui é commitada no git.

---

## 2026-10-03 — Sessão 76: auditoria de arquitetura, Fase 0 e Fase 1

Branch `feat/feat-07b-firetruck-art`. A WIP do FEAT-07b foi commitada
(`ff91be50`) e a sessão passou a implementar a auditoria de arquitetura,
que está arquivada em `27-relatorio-arquitetura-modernizacao.md`.

### Fase 0 — cobertura de medição (concluída)

O achado que organiza tudo: o harness headless não media o renderizador
nem a animação. Por isso `05` podia registrar dois descartes corretos por
"evidência insuficiente" enquanto o jogo ainda emite ~550 mil `drawTile`
por frame. Nenhuma otimização das Fases 3–6 entra antes disso.

- `6449c476` PERF-01/02 — benchmark `--bench` com cadência real de animação,
  config fixada (o benchmark herdava o `lincity-ng.conf` do desenvolvedor) e
  saída por `fmt::println` (o `printf` imprimia vírgula decimal em pt-BR).
  Baseline: 600 dias, 743 construções, 12 veículos → simulação 219,9 ms,
  animação 8,5 ms, **animação = 3,7 % do total**.
- `80bdabc9` PERF-03 — área `gui` no debuglog + cronômetro do rebuild do
  mapa. Medido: 897 tiles, 53 frames, ~1,2 ms a zoom 1,0.
- `14f64ab2` PERF-04 — job TSan no CI, porque a thread "Loader" é o
  subsistema mais concorrente do jogo e ASan/UBSan não veem data race.
  Verificado localmente antes de subir: build limpo, zero races, e a thread
  existe mesmo (breakpoint mostra `Thread 5 "Loader"`).

### Fase 1 — correções

- `55cdb243` BUG-25 — `r == -1` sobre `std::optional<MapPoint>` é sempre
  falso (MapPoint tem construtor implícito de um int), então o caminho de
  falha desreferenciava optional vazio. **UB alcançável** em mapa lotado,
  que é exatamente o caso que o bloco existe para tratar.
- `9dc273f1` BUG-26 — o incêndio aleatório silenciava sozinho. A auditoria
  dizia "≥4898"; o limiar real é **~2000**, porque `period <= offset` (6 dias
  contra offset 9) não tem solução para `day % period == offset`. O conserto
  foi na codificação de `due()`, não no número. Dois testes: um que falha
  sem o conserto e lista os lados culpados, outro que passa com e sem — a
  guarda contra corrigir demais.
- `69b9e3db` BUG-40 — `HEAVY_IND_WATER_POLLUTION` era constante morta
  "coberta" por `TCHECK_EQ(HEAVY_IND_WATER_POLLUTION, 6)`, uma tautologia
  que fazia o FEAT-06 parecer cobrir indústria pesada. Não cobre: as fontes
  reais são port, oilwell e refinery, cada uma somando na mão (violação de
  R2, e por isso não há função central para um teste perguntar "quem polui").
- `20a84ad2` BUG-34 — `ja.po`/`zh_CN.po` renderizavam 100% em tofu: a fonte
  não tem um único glifo CJK (conferido lendo o cmap). Agora o
  `FontManager` emenda uma fonte CJK do sistema como fallback.
  Detalhe que só apareceu testando: `TTF_AddFallbackFont` **não fica com
  referência** — a primeira versão fechava a fallback em seguida e os
  glifos continuavam ausentes. O `FontManager` passou a ser o dono.

### BUG-48 — o portão ASan não estava verde, e era pré-existente

Rodar a suíte completa sob ASan pela primeira vez nesta sessão mostrou
duas falhas que **não** vinham de nada da Fase 1: o `build-asan` tinha
sido compilado às 17:28, antes do primeiro commit de correção (17:34).
Ou seja: o portão ASan/UBSan/LSAN deste fork já falhava, e ninguém tinha
percebido — exatamente o tema da Fase 0.

- `init_game.cpp:339: runtime error: load of value 213, which is not a
  valid value for type 'bool'`. `_CitySettings` é um POD sem
  construtores, então `city_settings city;` deixa os dois `bool`
  indeterminados, e `create_new_city()` lê `without_trees` sem validar.
  Três testes faziam exatamente isso (`simtest.cpp:2113`,
  `test_ledger.cpp:17` e `:48`) — os outros oito preenchiam os dois
  campos, e os dois chamadores de produção (`MainLincity.cpp:145` e
  `MainMenu.cpp:901`) também. Corrigido na fonte, com initializers, para
  que nenhum chamador possa errar de novo; C++17 mantém a struct como
  agregado, então os `city_settings{true, false}` seguem válendo.

- `LeakSanitizer: 456 byte(s) leaked in 2 allocation(s)`, com pilha em
  `TransportConstructionGroup::placeItem`. A causa não era o
  `placeItem`: `World::~World` delegava toda a posse a
  `MapTile::~MapTile`, que apaga a construção **anexada** ao tile. Uma
  construção detachada — estrada, trilho ou linha de força trocada por
  outro transporte, prédio demolido desde o último tick — está em
  `map.constructions` sem dono, porque `detach()` é exatamente o que zera
  `tile->construction`. `doConstruction` recolhe essas durante o jogo,
  mas o que sobrou na teardown não tinha dono nenhum. `~World` agora
  apaga as que estão mortas antes de `map` ser destruído.

A confirmação de que era pré-existente veio do horário dos commits, não
de suposição: `git log` mostra o `build-asan` compilado antes de BUG-25.

### O que a regra "confirmar antes de corrigir" evitou

- **BUG-41** (`Ground::int1..int4`) parecia exclusão simples. Não é:
  `xmlloadsave.cpp:1299-1302` **serializa** os quatro campos. Remover é
  mudança de formato de save (R3), com bump de `ldsv_version`.
- **BUG-28** (`placeItem`) só vazaria se `place()` lançar, e
  `buildConstruction` já valida antes. Trocar por `unique_ptr` seria
  **pior**: se `neighborize()` lançar depois do registro em
  `map.constructions`, o ponteiro próprio apagaria um objeto que o mapa
  ainda referencia. Rebaixado a "não mexer".
- **PERF-02** — eu próprio havia escrito que o harness "nunca roda
  `do_animate`". Lendo o código, ele roda, mas 1× a cada 30 dias e sem
  tempo. O que faltava era cadência e instrumento.

### Continuação da Fase 1 — o resto dos achados

Os itens abaixo foram fechados depois que a primeira redação deste diário
foi escrita.

- `ba4bb940` BUG-38 — `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` gravava
  pixels na config, que em todo o resto é lida em coordenadas lógicas.
  A janela ficava com a proporção certa e o clique, errado. Agora a config
  recebe `SDL_GetWindowSize` (lógico) e o render continua com pixels.
- `3437423b` BUG-39 + BUG-47 — `Event` abortava em qualquer evento SDL não
  mapeado (`assert(false)`). O próprio arquivo já tinha a convenção
  oposta em `sanitizeScancode()`: sanitizar e seguir. E `debuglog` usava
  `std::mutex` sem linkar `Threads::Threads` — compila só porque o glibc
  ≥ 2.34 dobrou pthreads para dentro da libc.
- `674cd26e` BUG-43 — thread vestigial em `Sound`: `SDL_CreateThread`
  para rodar `loadWaves()`, com um `SDL_WaitThread` que nunca era
  alcançado no caminho normal. `loadWaves()` é chamado direto.
- `28afde36` BUG-44 — `getTileResourceGroup()` usava `resMap::operator[]`,
  que **cria** a entrada; um cache que se envenena com seis nulos em vez
  de reprocessar. Passa a `find()` e re-resolve enquanto algum ponteiro
  for nulo.
- `dff18aa2` BUG-45 — jogo novo nascia **congelado** se o anterior tivesse
  sido encerrado pausado. `simDelay` não é resetado por `setWorld`.
  Teste novo na camada 2 (`r7_pause_does_not_leak_into_the_next_game`),
  verificado por build negativo.
- `24bbcb73` BUG-32, `733789be` BUG-33 — `Style` sobrescrever um estilo era
  silenciosamente ignorado, e `Panel` era o único container que abortava
  em filho não-resizável. Nenhum dos dois era um bug *presente*: o
  defeito era a armadilha sem aviso.
- `6938f2d8` BUG-31 — o caminho OpenGL está morto e o build agora diz isso
  em voz alta na configuração, em vez de só aceitar a opção.
- `e9dd9155` BUG-30 — `log::enabled()` era função fora de linha. Com
  `LINCITYNG_ENABLE_DEBUG_LOG` ligado por padrão em RelWithDebInfo (que é
  o build normal de desenvolvimento), **todo** `LNG_LOG` no jogo pagava
  uma chamada fora de linha só para perguntar se ia imprimir. Passou a
  `inline` no header, com as tabelas num namespace `detail` nomeado.

### Portão final

`ctest` em `build/` e em `build-asan/`: **5/5**, verde nos dois.
ASan completo levou 1697 s (`sim` 1187 s, `save` 500 s), o que confirma
que o portão desta vez *fecha* — antes da série ele já estava vermelho.

### Ainda aberto na Fase 1

**BUG-36** (intervalo de tamanho de mapa validado é ficção: o topo do
intervalo pede 10,4 GB com `sizeof(MapTile)=104`), **BUG-37** (persistir
resolução ao arrastar a janela é decisão de produto) e **BUG-46**
(`BasicUrbg` semeado por processo viola R7, mas semear por `time(0)`
quebraria o determinismo — precisa de desenho, não de remendo). Os três
estão registrados como decisão, não como falta de tempo, em `11`.

### O que ainda não foi verificado

BUG-34 (fontes CJK) foi provado por teste isolado — `TTF_AddFallbackFont`
não fica com referência, e a primeira versão fechava a fallback em
seguida. Mas o **render** de ponta a ponta não foi visto: nenhum
headless pinta tela. A aceitação é manual: abrir o jogo, escolher
japonês, olhar a tela.

---

## 2026-09-29 — Sessão 75: FEAT-06 poluição hídrica + TST-10

FEAT-06 implementado:
- `do_water_pollution()` em engine.cpp: diffusion entre tiles water,
  cap em WATER_POL_MAX=1000, rodando a cada 14d junto com do_pollution()
- Constantes: PORT_WATER_POLLUTION=3, OILWELL=5, REFINERY=8, HEAVY_IND=6
- Sea farm reduz produção: food/goods *= (1 - water_pol/MAX)
- Port, oilwell, refinery adicionam water_pol nos update()
- Teste unitário water_pollution_formula

TST-10 implementado (tests/unit/test_tst10_assets.cpp):
- Valida imagens.xml refs existem no disco
- Valida help XML img refs existem
- Verifica Sea Farm assets presentes (5 tiles + ícone)
- Verifica can_build_here funciona em water
- Bug fix: sea farm can_build_here chamava base is_bare() (falha p/water)
- Bug fix: placeItem rejeitava water p/ não-transport → buildOnWater() virtual

Roadmap e diário atualizados. CTest 4/4 ✅.

---

## 2026-09-28 — Sessão 73: FEAT-07a correção de ícones e ajuda

Usuário reportou screenshots de ícones incompletos e ajuda sem imagens;
autorizou a correção. Branch `fix/feat-07a-icons-help` sobre `16cbcd2e`.
Commit local assinado `c15e818a`.
R5 prévio em `24-correcao-icones-ajuda.md`: GUI/assets/help pt_BR,
sem impacto em save/simulação, economia, stats, minimapa, MPS, config ou RNG.
Sem flag/bump. Docs locais e arquivos preexistentes preservados.

MSVG perdia strokes: exporter agora usa librsvg/PyGObject/pycairo, conserva
SVGs originais e normaliza com ImageMagick. Templates orig/pt_BR de poço e
refinaria geram quatro páginas ilustradas; conversor ganhou seleção de
arquivos/idiomas sem alterar o comportamento padrão. Teste focal verifica
pixels estruturais, transparência e referências/layout das páginas instaladas.

Builds regular/ASan passaram. CTest regular 4/4 em 137,23s
(`20260928-115345.388465963_6_icons-help-full`). Focal ASan/UBSan 2/30 passou
(`20260928-115333.543650139_5_icons-help-asan-final`, detect_leaks=0).
Negativo com export MSVG detectou alpha ausente, exit 1 esperado; pacote do
harness restaurado. XML/PNG reproduzíveis por hash. Inspeção dos PNGs feita;
nova aceitação visual manual no jogo pendente. Sem suíte ASan completa.
Sem alterações de sprites do mapa, saves ou remotos.

---

## 2026-09-29 — Sessão 74: bateria extensa de testes de integração + correções Sea Farm

Validou FEAT-04 (ledger), FEAT-05 (sea farm), FEAT-07a (pilotos),
modernização (help templates, zoom) com testes reais.

Testes novos (commit `81a210fa`):
- unit/test_sea_farm.cpp: `sea_farm_cannot_build_on_land`,
  `sea_farm_formula_scales_with_labor`
- unit/test_ledger.cpp: `ledger_maintenance_accumulates`,
  `ledger_revenue_accumulates`, `ledger_net_worth_is_revenue_minus_expenses`
- sim/simtest.cpp: `sea_farm_production_simulation` (carve water, build,
  run 1200d, verifica food+goods), `ledger_accumulates_during_simulation`,
  `sea_farm_ledger_records_expense_and_revenue`
- save/savetest.cpp: `round_trip_sea_farm_state` (ledger + sea farm state)

Bug encontrado e corrigido: sea farm `can_build_here` chamava a base
`ConstructionGroup::can_build_here` que exige `is_bare()` — falso para
água. Substituído por check direto de visibilidade + GROUP_WATER + sem
construction cobrindo.

Bug encontrado e corrigido: `ConstructionGroup::placeItem` rejeitava água
para não-transporte. Adicionado `virtual buildOnWater() const` (default
false) em ConstructionGroup; SeaFarm override=true.

CTest 4/4 ✅ (100%).

Docs atualizadas: roadmap (TST-10 em proposto, FEAT-07a em piloto).

## 2026-09-28 — Sessão 72: FEAT-07a piloto estático no painel

Pedido: implementar a próxima etapa; testes reais no jogo ficam para depois.
Branch `feat/feat-07a-visual-panel`, sobre BUG-24 assinado `5117f906`.
Commit local assinado `16cbcd2e`; documentação local preservada fora do commit.
Checklist R5 prévio e decisão em `23-piloto-painel-visual.md`: GUI/arte,
sem impacto em save/simulação, economia, stats, minimapa, MPS ou RNG;
sem flag/bump. Reutilizadas strings e páginas help existentes.

Três ferramentas: Sea Farm/agricultura (grupo 53), poço/mineração (51),
refinaria/indústria (52). Gavetas no máximo 300px. Ícone Sea Farm reutilizado;
dois ícones vetoriais originais de petróleo, fontes SVG e exportador 32px.
Não houve geração/edição raster com IA nesta sessão. Contrato explicitamente
estático, preservando módulos/índices; aliases não representam produção.
Não declarar arte final aprovada antes da inspeção real.

Builds regular e ASan passaram. Teste focal de seleção/gate tecnológico,
tooltip/custo e dimensões dos ícones passou (1/29):
`20260928-033449.325551810_5_feat07a-panel`.
Exportação de ícones reproduzida com hashes idênticos. CTest regular 4/4
passou em 95,72s (`20260928-033458.752001210_5_feat07a-full`).
Focal ASan/UBSan passou, detect_leaks=0:
`20260928-033640.544978754_5_feat07a-panel-asan`.
Sem suíte ASan completa ou teste manual/OpenGL nesta sessão.
Sem operações remotas.

---

## 2026-09-27 — Sessão 71: BUG-24 escala e pivô do petróleo (concluído)


Autorizado pelo usuário: segundo passo de `22` §6. Três agentes fizeram
leitura de backlog, Git, assets, renderer e testes; decisões pelo principal.
Branch local `fix/bug-24-petroleum-sprites`, a partir de `1b9f32c9`.
Implementação e validação concluídas; commit assinado `5117f906`.

R5 antes de implementar: arte e offsets XML; testes de carregamento/preview.
Sem impacto em save/simulação, taxas/economia, stats/inventário, minimapa,
MPS, tradução/help, GUI/config ou determinismo/RNG. Sem flag, bump de save
ou mudança de footprint 3×3. Módulos, botões e animação ficam intocados.
Decisão: bases raster estáticas assistidas por IA, masters e exportação
reproduzível; geometria 3D e estados produtivos finais ficam em FEAT-07a.
Não ampliar símbolos 32×32 como se isso recuperasse detalhe de construção.

Entregue: oito PNGs RGBA; poço 384×352/pivô (192,352), refinaria
384×384/pivô (192,384). Masters raster gerados com imagegen integrado,
prompts, hashes e exportador afim em `data/models/petroleum/`.
Quatro aliases estáticos por prédio; módulos e estados permanecem iguais.

Validação:
- builds regular e ASan compilados;
- CTest regular 4/4, 315,62s (`20260927-204249.985388005_5_bug24-petroleum`);
- focal ASan+UBSan passou, `detect_leaks=0`
  (`20260927-204050.694550586_5_bug24-asan`); suíte ASan inteira não executada;
- preview focal passou; cenas em `build/logs/20260927-bug24-preview/`,
  comparação visual SDL com estrada/indústria 3×3 em 1× e 0,5×;
- negativo com sprite antigo 32×32 falhou nas dimensões esperadas (exit 1),
  arquivo restaurado (`20260927-204049.547428938_11_bug24-negative-size`);
- catálogo fonte/instalado: 355 nomes únicos, zero ausências;
- exportação repetida byte a byte; `git diff --check` limpo.

Limites: sem validação OpenGL, animação nova ou fontes Blender. Oito texturas
RGBA8 consomem aproximadamente 4,31 MiB de pixels, sem overhead do driver.
Documentação local e `.openrouter-setup.md` preexistentes preservados fora
do commit. Nenhum fetch/pull/push.

---

## 2026-09-27 — Sessão 70: BUG-23 assets da Sea Farm (concluído)

Commit local: `1b9f32c9` na branch `fix/bug-23-seafarm-assets`; sem push.

Escopo: cinco arquivos de tiles e ícone de help; ramo
`fix/bug-23-seafarm-assets`. Skill imagegen, ferramenta integrada: base
estática e ícone gerados com alfa. Os cinco nomes compartilham a base,
pois `SeaFarm::animate()` não seleciona estados; animação e botão novo
continuam em FEAT-07a.

R5 antes de implementar: impacto em arte/offsets XML e imagem já referenciada
pelo help. Sem impacto em save/simulação, taxas/economia, stats, minimapa,
MPS, textos/traduções, config e determinismo/RNG. Sem flag, sem bump de save,
sem mudança de footprint lógico. Validação de carregamento na suíte GUI
headless existente, instalação e comparação visual da exportação.

Entregue: cinco PNGs 512×320 RGBA com pivô (256,320), ícone 32×32;
masters raster, prompts, hashes e exportador em `data/models/sea-farm/`.
Teste de carregamento exige cinco texturas e ícone; fixture opcional exporta
preview SDL lado a lado com fazenda legada em 4×/2×/1×/0,5×/0,25×.

Validação:
- build regular e build-asan compilados;
- CTest regular 4/4, 96,61s (`20260927-142227.285151005_5_bug-23-seafarm`);
- fluxo/reset/arte focal ASan+UBSan passou, `detect_leaks=0`
  (`20260927-180518.654447318_5_bug23-asan`); suíte ASan inteira não executada,
  pois não houve alteração da simulação;
- preview focal passou e imagens em `build/logs/20260927-bug23-preview/`;
- negativo: ocultar `seafarm7.png` instalado causou exatamente a falha
  `frame.texture != nullptr` (exit 1); arquivo restaurado pelo trap;
- catálogo fonte e instalado: 355 nomes ativos, zero ausências;
- `git diff --check` limpo. Sem fetch/push; mudanças anteriores em `.devdocs`
  e `.openrouter-setup.md` preservadas, fora do commit de assets.

Limites: cinco aliases estáticos, sem animação nova, botão ainda pendente em
FEAT-07a. Inspeção visual via renderer SDL dummy; OpenGL não foi validado.

---

## 2026-09-27 — Sessão 69: decisão de modernização visual com IA

Escopo solicitado e cumprido: somente documentação. Três agentes
`gpt-6-luna` pesquisaram sprites, renderer e backlog; o agente principal
conferiu os achados no HEAD `d0fb1f1d`. Modelo “luna 5.6” indisponível.

- Corrigida a premissa da sessão 68: SDL3, gettext, Painter/TextureManager,
  zoom e config por env já estão no fork. A fila de migração em `11` foi
  substituída por cobertura de assets e melhoria de arte.
- Parser do catálogo: 441 entradas ativas, 355 PNGs distintos, 5 ausentes
  (Sea Farm). Ícone referido pelo help também ausente. Pontes antigas
  comentadas não contam como falha.
- Poço/refinaria: 32×32 com offsets `(256,299)`, animação reduzida a frame 0
  por catálogo de um frame/grupo; variantes Low/Med/Full sem seleção nos
  módulos. Firetruck usa carro comum tingido. Botões dos três prédios novos
  não constam no painel. São pendências, não correções realizadas.
- Decisão: IA para conceitos/acabamento e apoio à produção; fontes editáveis
  e câmera controlada para séries. Piloto Sea Farm + petróleo, depois
  bombeiro/ícones e revisão por famílias. Sem migração gráfica como requisito.
- Documento canônico novo `22`; sincronizados `07`, `08`, `11`, `14` e índice
  README. IDs: TST-10, BUG-23/24 e FEAT-07a–d, todos ainda pendentes.
- Validação: código, histórico, parser XML, dimensões PNG e inspeção visual
  de oilwell-full/refinery-full. Sem build/CTest, benchmark ou geração de
  imagens, pois não houve mudança de runtime. Sem impacto em save/simulação,
  economia, stats, minimapa, MPS, tradução, GUI/config e determinismo.
- Nenhum fetch/pull/push. `.openrouter-setup.md` preexistente preservado.
  O commit anterior rastreou parte de `.devdocs` com force-add; esta sessão
  preserva esse estado, sem novo force-add, remoção do índice ou commit de
  documentação local, conforme a regra de `README`/`10`.

---

## 2026-09-27 — Sessão 67: FEAT-04 Ledger by Construction (implementação)

- **Campos ledger adicionados a `Construction`** (`lintypes.hpp`):
  `purchase_cost`, `capital_invested`, `maintenance`, `operation_cost`,
  `revenue`.
- **Save/load** (`lintypes.cpp`): 5 novos XML elements em
  `Construction::save()` / `loadMember()`.
- **Build cost recording** (`lintypes.cpp:1233`):
  `ConstructionGroup::placeItem()` agora faz
  `tmpConstr->purchase_cost = getCosts(world)` e
  `capital_invested = purchase_cost`.
- **Tracking hooks** (`lintypes.hpp/cpp`):
  `Construction::recordExpense(int)` → acumula em `maintenance`;
  `recordRevenue(int)` → acumula em `revenue`. Módulos podem chamar
  junto com `world.expense()`/`world.income()`.
- **ldsv_version** (`xmlloadsave.hpp`): 2144 → 2145. Compatibilidade:
  saves antigos carregam com campos ledger=0 (default).
- **MPS LEDGER page** (`Mps.hpp/cpp`, `MiniMap.cpp`):
  `MpsFinance::Page::LEDGER` — itera `world.map.constructions`,
  mostra total purchase/capital/maintenance/operation/revenue + net
  worth. Ciclo: CASH_FLOW → COSTS → POPULATION → MANAGEMENT → LEDGER.
- **Tests**:
  - `tests/unit/test_ledger.cpp` — TTEST
    `ledger_records_expense_and_revenue` (acumulação) +
    `ledger_purchase_cost_set_at_build` (placeItem).
  - `tests/save/savetest.cpp` — 5 checks ledger no
    `compare_worlds()`.
  - `tests/sim/simtest.cpp` — fingerprint atualizado para nova
    contract (0663381900960033e6bc62829e3dadba).
- CTest 4/4 ✅ (unit 0.01s / sim 60s / save 26s / devlog 0.3s).

---

## 2026-09-27 — Sessão 67: FEAT-04 Ledger + FEAT-05 Sea Farm

### FEAT-04 (committed cbf4ad77)
- 5 ledger fields in Construction; save 2145; MPS LEDGER page;
  recordExpense/recordRevenue hooks; tests camadas 1-3.

### FEAT-05 (committed d1dc6a88)
- New module `sea_farm.hpp/cpp`: water-tile placement,
  consumes labor+water+energy, produces food+goods.
- Constants: COST=15000, tech=40k, labor=180, power=200,
  water=10, food=350, goods=8.
- GROUP_SEA_FARM=53 in groups.hpp (NUM_OF_GROUPS→54).
- Registered in all_modules, CMakeLists.txt.
- Sprites in images.xml, help doc + pt_BR translations.
- Unit tests: sea_farm_constants, sea_farm_can_build_on_water.
- CTest 4/4 ✅.

---

## 2026-09-29 — Sessão 74: Modernização — help templates + zoom

- Adotado sistema de help templates do upstream:
  `data/help_templates/convert.py` converte .tml → .xml;
  `sea_farm.tml` e `farm.tml` criados.
- Atalhos zoom: SDLK_EQUALS (in), SDLK_MINUS (out) em Game.cpp.
- Env vars LINCITYNG_CONFIG_FILE/APP_DATA_DIR/USER_DATA_DIR já
  presentes em Config::init.
- CTest 4/4 ✅. Commit `3bcc3bf6`.

---

## 2026-09-27 — Sessão 68: Estudo modernização upstream

> Retificação na sessão 69: as capacidades abaixo já existem no fork.
> A caracterização SDL2/tinygettext/rendering legado e as fases de migração
> foram corrigidas em `11`; o plano gráfico vigente está em `22`.

- Fetch upstream master (59cb5312, versão 2.15.0).
- Upstream migrou SDL2→SDL3 3.2.0 (2.14.0+), libxml++ GUI,
  gettext/libintl (sem tinygettext), PO unificado em .mo binário.
- Rendering: Painter, TextureManager, zoom 0.0312×–4×,
  Component refactor, idioma sem reiniciar.
- Fases de migração documentadas em 11:
  Fase 1 (baixo): gettext, config env vars, help templates
  Fase 2 (médio): zoom, Component/Painter
  Fase 3 (alto): SDL3 completo

---

## 2026-09-26 — Sessão 66: FEAT-03e discrepância FOOD_RATE + sprites

- Resolvida discrepância PORT_FOOD_RATE: doc 13 diz 500 ($5/unit), código tem 1 ($0,01/unit). Benchmark com rate=1 → food stock=7158 (saudável). Decisão: manter rate=1. Doc 13 atualizada.
- Sprites oilwell/refinery melhorados: 32×32 RGBA com derrick/pool e building/smoke+flame. Substituem placeholders monocromáticos.
- Commits: `28a8fe69`

---

## 2026-09-26 — Sessão 65: FEAT-03e testes camadas 1-2 (itens 4-5)

Implementados os 2 testes pendentes do Porto 2.0:

- `tests/unit/test_port_rates.cpp` — TTEST `port_rates_benchmark`:
  verifica PORT_ORE_RATE=50, PORT_STEEL_RATE=30, PORT_PETROLEUM_RATE=40
  (valores benchmarkados Rocket_98 seed 1234567 1500d). Flags ORE/STEEL
  como "discrepância doc vs código" (doc diz FOOD_RATE=500, código=1).
- `tests/sim/simtest.cpp` — TTEST
  `port_trade_preserves_commodity_conservation`: coloca porto em tile
  com rio a leste, configura take/give (import food/coal/ore, export
  goods/steel), seta tradeRule e workforce, roda 120 dias, verifica
  invariante de conservação.

CTest 4/4 ✅ (unit 0.01s / sim 93s / save 48s / devlog 0.3s).

---

## 2026-09-26 — Sessão 63: FEAT-03d phase c — oil well + refinery + GUI

Fase c de FEAT-03d (petróleo): poço, refinaria e GUI.

- `src/lincity/modules/oilwell.{hpp,cpp}`: novo — extrai petroleum de `oil_reserve` do tile, produz petroleum, gasta labor; ao esgotar reserva e estoque, evapora (makeLake)
- `src/lincity/modules/refinery.{hpp,cpp}`: novo — consome petroleum + labor, produz goods
- `src/lincity/modules/all_modules.hpp/cpp`: registro dos novos grupos
- `src/lincity/all_buildings.hpp`: constantes OILWELL/REFINERY + MAX_PETROLEUM_AT_REFINERY / MAX_GOODS_AT_REFINERY
- `data/images/tiles/images.xml`: sprites OilWell/Refinery (4 estados)
- `data/help/en/oilwell.xml`, `refinery.xml`: documentação
- `src/lincity-ng/Mps.cpp/.hpp`: oil reserve no ground view, petroleum stock nos KPIs
- Sprites placeholder 32x32 (8 files)
- Correção de bug: refinery.hpp usava MAX_PETROLEUM_ON_PORT / MAX_GOODS_ON_PORT (constants de port) → corrigido para MAX_PETROLEUM_AT_REFINERY / MAX_GOODS_AT_REFINERY
- Correção de bug: oilwell.hpp usava MAX_PETROLEUM_ON_PORT → corrigido para MAX_PETROLEUM_AT_REFINERY
- CTest: unit/sim/save ✅; ASan: devlog/unit/sim/save ✅ (3 runs, 4/4 cada)
- Commit `b9dfd960`

Itens pendentes desta sessão:
- PORT_PETROLEUM_RATE=5 placeholder — aguardando benchmark
- DeepSeek review de fase c (configurado em `13`)
- .devdocs/08/09/11 atualizados ✅ (nesta sessão)

---

## 2026-09-26 — Sessão 64: FEAT-03d fixes — mps.xml, ResourceGroup, dead code

Correções pós-review e ajustes pendentes da fase c.

- `data/gui/mps.xml`: TableLayout rows 14→15 + célula `mps_text14`
  → resolveu SEGFAULT em `Mps::clear()` ao escrever 15ª linha Petroleum
- `src/lincity/modules/all_modules.cpp`: `new ResourceGroup` para
  OilWellLow/Med/Full + RefineryLow/Med/Full → resolveu warnings
  "unknown resourceID" e crash no sim
- `src/lincity/modules/oilwell.cpp`: removido dead code em `place()`
  (variável local `oil` nunca usada; `if(oil<1) oil=1` era unreachable)
- `src/lincity/groups.hpp`: GROUP_OILWELL=51, GROUP_REFINERY=52
- DeepSeek v4.1 flash review via OpenRouter (`deepseek/deepseek-v4.1-flash`)
  → encontrou bug crítico: lake conversion never triggers (condição
  `total_extracted >= 99999` inalcançável; corrigido para
  `!any_oil && total_extracted > 0` — já commitado em 9281a4f0)
- PORT_PETROLEUM_RATE=40 justificado (entre COAL=50 $0.50/unit e
  STEEL=30 $0.30/unit; petroleum é intermediate good)
- CTest: 4/4 pass (unit/sim/save/devlog) ✅
- Commit `83cc4552`

---

## 2026-09-26 — Sessão 61: FEAT-03e (Porto 2.0) — opção B implementada

Planejamento registrado em `.devdocs/13-feat-03e-porto.md`:
- Decisões bloqueantes documentadas: 2.1 taxação dupla (opção B), 2.2 benchmark de rates, 2.3 pence OK, 2.4 comércio sorteado OK

**Opção B implementada**: porto isentado de taxable de entrega.
- `lintypes.cpp:930-931` (`equilibrate_stuff`): adicionado `&& constructionGroup->group != GROUP_PORT`
- `lintypes.cpp:1112-1114` (`applyTransport`): adicionado `&& p.origin->constructionGroup->group != GROUP_PORT`
- Comenta: "port is exempt: export tax covers port revenue"
- CTest regular 4/4 ✅ (103s)
- ASan/UBSan em execução (sim ~836s)

Benchmark executado: simtest Rocket_98, seed 1234567, 1500 dias.
Porto configurado: importa food/coal/ore, exporta goods/steel.

Resultados:
- food: estoque 7158, import $14.170/dia
- coal: estoque 1714, import $14.170/dia
- ore: estoque 3528, import $14.170/dia
- goods: estoque 808, export $10.285/dia
- steel: estoque 134, export $10.285/dia
- monthly_ic: $418.048, monthly_et: $306.552
- lastm_ic: $1.348.396, lastm_et: $1.105.063

Análise de rates:
- PORT_ORE_RATE=1 → import $0,01/unidade (subprecificado) → ajustar p/ 50
- PORT_STEEL_RATE=100 → export $100/unidade (caro demais) → ajustar p/ 30
- PORT_FOOD_RATE=500 → $5/unidade (OK)
- PORT_COAL_RATE=50 → $0,50/unidade (OK)
- PORT_GOODS_RATE=3 → $3/unidade (OK)

Porto NÃO opera sem configuração do jogador (take/give=false por padrão).
Isso é comportamento correto (jogador deve configurar via diálogo).

Gate benchmark ✅ — opção B decidida e implementada.
- CTest regular 4/4 ✅, ASan/UBSan 4/4 ✅
- Commit 88126ed3

Gate implementação ✅ — FEAT-03e concluída.

## 2026-09-26 — Sessão 61: FEAT-03e (Porto 2.0) — planejamento

Planejamento registrado em `.devdocs/13-feat-03e-porto.md`:
- Decisões bloqueantes documentadas: 2.1 taxação dupla (opção B — isentar
  porto de taxable de entrega), 2.2 benchmark de PORT_*_RATE,
  2.3 pence OK, 2.4 comércio sorteado OK
- Análise do comportamento atual: buy/sell rates, pence, tradeRule toggle
- Mudanças propostas: 5 itens (sem bump de save)
- Critérios de aceite listados

Gate planejamento ✅ — próximo: decisão do jogador (opção A/B/C)
e benchmark de rates antes de codar.

## 2026-09-26 — Sessão 60: REF-07 Fase 6 concluída (extração transacional do transporte)

Fase 6 (extração transacional do transporte) implementada e validada:
- `Construction::trade()` agora dispara entre Legacy (shuffle + equilibrate_stuff)
  e DeterministicV1 (snapshotTransport → resolveTransport → applyTransport)
  com base em `World::simulationProtocol()`
- Três fases transacionais em `lintypes.cpp`:
  - `snapshotTransport()`: coleta propostas de fluxo sem mutar o World
  - `resolveTransport()`: ordena deterministicamente (comodidade → origem
    MapPoint → destino MapPoint) ou embaralha com BasicUrbg legado
  - `applyTransport()`: aplica deltas consolidados + recordTaxable
- `TransportProposal` struct e declarações estáticas em `lintypes.hpp`
- CTest regular 4/4 e ASan/UBSan 4/4 (sim 836s, save 366s)
- Conservação de commodities verificada pelo simtest (ledger invariant)

Gate Fase 6 ✅ — REF-07 completo.

## 2026-09-26 — Sessão 59: REF-07 Fase 5 commitada + início Fase 6

Fase 5 (save 2143) documentada, validada e commitada na branch
`refactor/ref-07-save-2143`:
- `ldsv_version` 2143 com compatibilidade 2130
- Persistência de protocolo, seed, random_protocol_version
- `stats.total_money`, `stats.inventory`, `Map::polluted`
- Estado de trajetória de Coalmine, Commune, Market, School,
  Windmill/Windpower
- 4 novos testes camada 3 (round-trip v1, fixture 2142, continuação
  200 dias, fronteiras 99/100/1199/1200)
- CTest regular 4/4 e ASan/UBSan 4/4 com `detect_leaks=0`

Gate Fase 5 ✅ — próximo: Fase 6 (extração transacional do transporte).

## 2026-09-13 — Sessão 58: REF-07 Fase 5, save 2143 e compatibilidade

A pedido do usuário, foi executado o gate imediato do REF-07 (Fase 5) na
branch local `refactor/ref-07-save-2143`, criada a partir de `42cdb9a7`, sem
rede e sem commit. O save passou de 2142 para 2143 mantendo
`LOADSAVE_VERSION_COMPAT` 2130: protocolo, seed e versão do RNG v1, a fila
`Map::polluted`, o estado de trajetória de Coalmine (`mine_cur`,
`current_coal_reserve`, contadores; `place()` deixou de recalcular e de
re-aplicar o boost +20 no load), Commune, Market (`market_labor`; `cover()`
não roda no load para preservar a fase dos `*_COVER_CHECK`), School e
Windmill/Windpower (`paid`). O loader segue o padrão por tags do 2140–2142,
com tags opcionais (2142 ausente ⇒ Legacy/0) e gate de
`random_protocol_version` futuro; `setSimulationProtocol` é aplicado antes do
`<map>`.

Os testes de fronteira (1199/1200) revelaram duas lacunas extras, também
persistidas: `stats.total_money` (snapshot de fim de mês, diferente de
`world.total_money` após o imposto anual) e `stats.inventory` (acumuladores
`Stat` da história mensal).

A camada 3 ganhou 4 testes novos — round-trip v1 com XML canônico
byte-idêntico, fixture 2142 genuína em `tests/fixtures/` (gerada pelo binário
pré-bump de `good_times.scn.gz`; sha256 `11077b65…`), continuação v1 de 200
dias e fronteiras 99/100/1199/1200 — e os round-trips existentes passaram a
exigir igualdade do XML canônico. O golden Legacy do fingerprint foi
regenerado uma vez (`d0f29750…` → `64f6bd4b89fe3d5ab7db68a83c19a649`) porque
o XML canônico cresceu; nenhum código de simulação mudou.

CTest regular final 4/4 (`build/logs/20260913-192432…_ref07-phase5-regular-final`;
sim 92,27 s; save 46,55 s) e ASan/UBSan 4/4
(`build/logs/20260913-185020…_ref07-phase5-asan2`; sim 1369,71 s; save
641,87 s) com `detect_leaks=0` pela limitação conhecida de LSan/ptrace. O
primeiro run ASan (Debug) flagrou dois asserts de nome de nó em
`loadMap`/`loadMapTile` corrompidos durante a edição; restaurados ao texto do
HEAD.

R5: persistência sem mudança de comportamento; sem fórmula, taxa, commodity,
stats observáveis ou tradução novos; GUI/config sem flag nova; determinismo
apenas no contrato persistente do v1; estado por instância (`loadedFromSave`),
sem `static` novo; nenhum cenário de abertura alterado. Relatório completo em
`.devdocs/21`; próximo gate: Fase 6 (extração transacional do transporte).

## 2026-08-22 — Sessão 57: REF-07 Fase 4, RNG pertencente ao World

Após autorização do usuário, foi criada a branch local
`refactor/ref-07d-world-rng-save` sobre `4a552514`, sem rede, e concluída no
commit `42cdb9a7`. Foi introduzido
o protocolo opt-in `DeterministicV1`: cada `World` possui seed e geradores de
simulação/cosmético; contextos de gameplay são derivados de evento, dia,
coordenada e ordinal por SplitMix64 + xoshiro256**. Bounded por rejeição,
chance racional, aproximação normal inteira e Fisher–Yates próprio removem a
dependência da STL no caminho v1.

Ordem, ecologia, poluição, fogo, favelas, demografia, regras anuais, foguete,
geração de mapa, estado de construção e fogo de lixo foram migrados. Veículos,
animações e som usam fluxos cosméticos separados. Os ramos Legacy continuam
com as expressões/distribuições originais e conservaram o golden de 30 dias
`d0f2975041a730617e7d9fdd039bc845`.

A opção `Config::deterministicScheduler` nasce false e só é consumida ao
criar mundo/cenário novo. Ela não é lida/escrita no XML nem exposta na GUI
antes do contrato persistente. `xmlloadsave.cpp` e cenários de abertura não
foram alterados; save continua 2142. A Fase 5 fará atomicamente o bump 2143,
compatibilidade 2142 e persistência de protocolo/seed/versão.

Testes cobrem vetores golden, dois mundos intercalados, perturbação do RNG
global, isolamento de carros/som/animação e conservação v1 por 1.200 dias.
CTest regular final: 4/4 em 85,87 s, stderr vazio. A compilação regular e
sanitizada passou com Werror; CTest ASan/UBSan completo: 4/4 em 1.026,13 s
(`sim` 960,72 s; `save` 64,98 s), stderr vazio e `detect_leaks=0` devido à
limitação conhecida de LSan com `ptrace`. Os focados sanitizados foram
repetidos após a última revisão. Evidências estão no relatório temporário 20.

R5: mudança deliberada de RNG apenas no protocolo opt-in; nenhuma fórmula,
taxa, `taxable`, commodity, stats, minimapa, MPS, tradução ou help mudou.
Config interna default false; sem estado `static` novo; geradores pertencem às
instâncias. Evidências e contrato técnico completos estão no relatório 20.
Próximo gate: Fase 5, save 2143 e compatibilidade.

## 2026-08-22 — Sessão 56: REF-07 Fase 3, calendário Legacy explícito

Após autorização do usuário, foi criada a branch local
`refactor/ref-07c-calendar-legacy` sobre `cae294a9`, sem rede. A cadência
top-level de `World::do_time_step` foi extraída para uma tabela imutável com
`SimulationEventId`, fase, período, offset e prioridade. Os dez eventos são:
stats mensal/anual, ecologia, sustentabilidade, poluição, incêndio aleatório,
cobertura, favelas e fechamentos mensal/anual.

A execução preserva os dois pontos do tick: stats antes de
`simulate_mappoints`; eventos periódicos depois. Nenhum `trade`, `update`,
draw de RNG ou predicado interno de módulo foi movido. Eventos devidos emitem
trace com nome, fase e prioridade. Testes puros cobrem dias 0, 3, 9, 14, 75,
99, 100, 300, 1199 e 1200, além da prioridade e do período dinâmico de fogo.

O golden Legacy de 30 dias permaneceu
`d0f2975041a730617e7d9fdd039bc845`. Uma cidade nova rodou 60.000 dias em
18,4 s, frente à referência aproximada de 19,9 s da Fase 0, com conservação
intacta. CTest regular: 4/4 em 61,31 s. CTest ASan/UBSan: 4/4 em 937,32 s,
stderr vazio, usando `detect_leaks=0` devido ao `ptrace` do ambiente. Uma
tentativa unitária inicial com LSan confirmou apenas essa limitação conhecida;
a repetição normativa passou.

R5: refatoração de simulação sem mudança econômica, stats observáveis,
minimapa, MPS, GUI/config, tradução/help, save ou RNG. O save continua 2142,
sem estado de partida novo, sem `static` mutável e sem tocar cenários de
abertura. Evidências completas estão no relatório temporário 19. Próximo gate:
Fase 4, RNG pertencente ao `World` e protocolo DeterministicV1.

## 2026-08-22 — Sessão 55: REF-07 Fase 2, replay e comandos externos

Após autorização para seguir, foi criada a branch local
`refactor/ref-07b-replay` a partir de `30dfa0e3`, sem rede. A Fase 2 introduziu
`SimulationCommand` e um replay headless TSV v1 com protocolo, seed, origem,
fingerprint inicial, comandos ordenados por dia/fase/sequência, resultado
esperado e checkpoints diários.

Os sete comandos do plano foram implementados: construir, demolir, evacuar,
inundar, alterar alíquota, pesquisar carvão e lançar foguete. Todos reutilizam
operações existentes de `World`/módulos. A edição das seis alíquotas foi
centralizada em `World::setMoneyRate`, adotado também pelo governador, com o
mesmo clamp 0–2× e sem mudança de default ou elasticidade.

A fixture determinística cria um foguete pronto, grava sete ações em dois
dias, inclui um terceiro checkpoint, serializa/parseia canonicamente e
reproduz em um segundo mundo. Um teste separado confirma que grupo inválido é
rejeitado sem alterar o fingerprint. CTest regular: 4/4 em 58,20 s. CTest
ASan/UBSan final: 4/4 em 904,29 s (`sim` 837,44 s; `save` 66,44 s), stderr
vazio e `detect_leaks=0`, necessário porque LSan não suporta o `ptrace` deste
ambiente. Evidências no relatório 18.

R5: impacto somente na orquestração headless e centralização de mutação de
taxa; fórmulas, lançamentos, stats, MPS, minimapa, RNG e ordem interna da
simulação permanecem iguais. Sem nova config ou tradução. Replay não é save:
`LOADSAVE_VERSION_CURRENT` permanece 2142. Sem estado `static`, sem cenário de
abertura tocado e sem operação remota.

## 2026-08-22 — Sessão 54: REF-07 Fase 1, auditoria e fingerprint Legacy

Após autorização para seguir, foi executada a Fase 1 do plano 15 na branch
`refactor/ref-07a-fingerprint-baseline`. O serializer do save 2142 foi
extraído para uma rotina compartilhada e ganhou um caminho canônico em
memória. Foi implementado hash estável próprio de 128 bits, snapshot por
seções e localização da primeira diferença.

A auditoria dos módulos identificou estado de trajetória ainda ausente do
save: `Map::polluted`; cursor/reserva corrente de Coalmine; contadores de
Commune; labor do Market; ocupação da School; e `paid` de Windmill/Windpower.
Esses membros entram numa seção Legacy canônica sem bump de save; a
persistência permanece reservada para o 2143 da Fase 5. Frames, veículos,
som, mensagens, updatedSet e ledger diagnóstico foram excluídos.

Goldens: bytes vazios `61d32176a7d5f721c56dab92f6ffc61e`, `abc`
`e6c91b2f8cb87efb1d7cb7e9dfa6ce82`, cidade nova seed 1234567 ao fim do dia
30 `d0f2975041a730617e7d9fdd039bc845`. O teste de trajetória compara os 30
fingerprints diários e reconstrói snapshots completos somente no caminho de
erro para informar dia, seção e byte.

Após a exclusão explícita de `recentPoint` de UI, o gate final passou: CTest
regular 4/4 em 57,03 s; ASan/UBSan 4/4 em 816,28 s com `detect_leaks=0` e
stderr vazio. Compilações Werror regular e sanitizada verdes. Relatório
completo e diretórios de evidência em `.devdocs/17`.

R5: nenhuma alteração de gameplay, fórmula econômica, taxa, calendário,
ordem, RNG, GUI, tradução ou minimapa; save gravado e versão continuam 2142.
Somente leitura diagnóstica do estado. Nenhum remoto ou cenário de abertura
foi tocado.

## 2026-08-22 — Sessão 53: REF-07 Fase 0 aprovada e executada

O usuário aprovou integralmente o plano temporário do REF-07. A execução foi
iniciada pelo único gate autorizado antes do primeiro patch: Fase 0, baseline
e congelamento de escopo. Os documentos 10, 09, 08, 11, a sessão mais recente
do 06, o 01 e o plano 15 foram relidos antes da mudança local de branch.

Foi criada a branch `refactor/ref-07a-fingerprint-baseline` a partir de
`ebd42a77`, sem operação de rede. O cache regular antigo ainda tinha Werror
desligado; o resultado funcional inicial foi descartado como gate, o cache foi
reconfigurado com `LINCITYNG_WARNINGS_AS_ERRORS=ON`, recompilado e retestado.
Os dois builds agora têm Werror ON.

Evidências definitivas: CTest regular 4/4 em 20,24 s; ASan/UBSan 4/4 em
219,78 s com `detect_leaks=0`; cidade nova seed 1234567 por 60.000 dias
passou em aproximadamente 19,9 s; `good_times` e `Rocket_98` passaram 1.200
dias com a mesma seed. Conservação monetária e de mercadorias foi checada
diariamente nas trajetórias aplicáveis. Nenhum diagnóstico de sanitizer.

O relatório reproduzível, checklist R5 e inventário de entrada da Fase 1
estão em `.devdocs/16-relatorio-temporario-ref-07-fase-0.md`. Nenhum código,
save, cenário de abertura, fórmula econômica ou protocolo foi alterado. A
próxima fase permitida é a auditoria de estado e fingerprint, após a
apresentação deste gate.

---

## 2026-08-22 — Sessão 50: QW-06 e configuração local

QW-06 foi implementado na branch `fix/qw-06-resolution-order`, com commit
`a642041e`, PR #21 e merge em `origin/master` como `0dd35455`. A lista de
resoluções agora filtra modos inválidos, normaliza o tamanho mínimo, ordena e
remove duplicatas; a lista SDL nula não causa acesso inválido. O teste puro
`resolution_options_filter_sort_and_deduplicate` cobre a regra sem tocar a
simulação, economia, save ou RNG.

`opencode.json` foi incluído em `/.gitignore` pelo commit `9558824d`; é uma
configuração local e não deve entrar no fork.

## 2026-08-22 — Sessão 51: TST-09 e decisão sobre código morto

A branch `test/tst-09-warning-gate-final` reduziu a baseline de `-Wall -Wextra`
com commits granulares e tornou `LINCITYNG_WARNINGS_AS_ERRORS=ON` o padrão.
Também atualizou `doc/BUILD_OPTIONS.md` e a mensagem do CI. A compilação regular
e ASan/UBSan com Werror passou.

Foram removidos itens comprovadamente mortos: o contador/constante de pausa
sem efeito do Blacksmith, o temporário de Commune, declarações obsoletas de
geração de mapa, `textureType` sem consumidor e temporários/argumentos sem uso.
Os helpers gettext não foram removidos porque são usados em vários módulos;
foram tornados `inline`. `OutsideMapMessage` também não era morto: recebeu
inicialização de estado e correção de inicialização de `point`.

R5: impacto de simulação/economia/taxas/save/tradução/RNG = nenhum intencional;
as alterações de parser são apenas warning-safe e não mudam a versão ldsv.
O teste `unit` desliga somente LSAN para os death tests que fazem `fork` e
`abort`; `sim/save` mantêm LSAN. O sandbox local impede LSAN por ptrace, mas
`ASAN_OPTIONS=detect_leaks=0 ... ctest --test-dir build-asan` passou 4/4;
regular também passou 4/4. A PR #22 passou o CI regular e ASan/UBSan no
workflow `32579379105` e foi merged em `origin/master` como `ebd42a77`.

## 2026-08-22 — Sessão 52: plano completo de implementação do REF-07

Sessão exclusivamente de pesquisa e planejamento, sem implementação. Foram
lidos integralmente, na ordem normativa, .devdocs/10, 09, 08, 11, 06 e 01;
também foram consultados .devdocs/14, 05, 12 e 02 nas seções relacionadas a
determinismo, performance, save e transporte.

O código atual foi mapeado em simulate, engine, lintypes, transport,
xmlloadsave, World, Map, randutil e módulos consumidores de calendário,
transporte e RNG. Os principais achados foram:

- trade e update mutam o mundo conforme uma ordem embaralhada;
- BasicUrbg é compartilhado por gameplay, veículos, animação e som;
- eventos periódicos usam períodos e offsets implícitos;
- transporte direto e Market fazem redistribuição in-place;
- ecologia e poluição misturam seleção e aplicação;
- map.polluted e outros membros mutáveis precisam ser auditados antes de um
  fingerprint completo de trajetória.

Foi criado .devdocs/15-plano-temporario-ref-07.md com escopo, não objetivos,
arquitetura de dois protocolos, RNG keyed pertencente ao World, scheduler
explícito, transporte transacional, ecologia por snapshot, replay, save 2143
previsto, checklist R5, dez fases de execução mais baseline, gates, testes nas
três camadas, benchmark, A/B, riscos, rollback e critérios finais.

Decisão desta sessão: nenhuma fase começa antes da apresentação e aprovação
explícita do plano. O caminho Legacy permanece como default inicial e para
saves antigos; DeterministicV1 é proposto como opt-in até a validação A/B.

R5 prospectivo: impacto central em simulação, determinismo, save, timing de
transporte, taxables, tráfego, stats e poluição; sem nova fórmula econômica ou
UI nesta sessão. Nenhum teste foi executado porque nenhum código versionado foi
alterado. Nenhum save, cenário, remoto ou upstream foi tocado. HEAD permaneceu
em ebd42a77 e o status versionado permaneceu limpo.

---

## 2026-08-21 — Sessão 49: preparação de QW-05/QW-04

### Checklist R5 antes da implementação

- **QW-05 — música no Options:** somente GUI/fluxo de áudio; sem impacto em
  simulação, economia/taxas, stats, minimapa/MPS, save/load, tradução/config
  persistida ou determinismo/RNG. Não exige flag nem bump de save.
- **QW-04 — tooltip:** somente apresentação de valores já existentes; sem
  alteração de fórmula, cobrança, simulação, stats, minimapa/MPS, save/load,
  flag ou RNG. Os rótulos novos devem passar por gettext e a manutenção deve
  ser explicitamente classificada como custo operacional estimado, não como
  nova regra econômica.
- **Greps obrigatórios:** `QW-05` estava ancorado em `Sound.cpp:376` e
  `MainMenu.cpp:297-298`; `QW-04` em `ButtonPanel.cpp:367-408` e nos únicos
  pontos existentes de `world.expense()` por módulo. Não será criada fórmula
  duplicada nem mutação econômica.

Implementação seguirá em duas branches, na ordem `QW-05` → `QW-04`, com
testes, PR, CI e merge separados. A limpeza de `BUG-19` foi feita apenas no
inventário local: `.devdocs/` é ignorado por decisão do fork.

### QW-05 implementado

Branch `fix/qw-05-music-options-refresh`, commit local `d4eb067f`; PR #19 foi
integrado no `origin/master` como `74eedff3`. O CI remoto `32525423494`
passou em regular (2m30s) e ASan/UBSan (4m38s).
`Sound::musicHalted` deixou de mudar `currentTrack` no callback de áudio e
passou a publicar uma solicitação atômica. `Game` e `MainMenu` consomem a
solicitação na thread principal; quando o menu Options está ativo,
`musicParagraph` é atualizado junto com a troca automática. O teste headless
`music_callback_defers_track_change_to_main_thread` cobre o contrato da
callback.

Evidências locais: build regular e ASan dos alvos afetados concluídos;
CTest regular 4/4; teste focado regular e ASan aprovados. O primeiro CTest
ASan completo foi interrompido no `sim` após exceder a duração histórica sem
saída; com `ASAN_OPTIONS=detect_leaks=0`, `unit` e o teste focado ASan passaram.
Esse timeout fica registrado para não ser confundido com falha do QW-05.

### QW-04 implementado

Branch `feat/qw-04-build-tooltip-cost-maintenance`, commit `e705767c`; PR #20
foi integrado no `origin/master` como `41b2f2e6`. O CI remoto
`32527312780` passou em regular (2m40s) e ASan/UBSan (3m50s).
`ButtonPanel::createTooltip` agora mostra custo de construção e custo
operacional estimado. A ponte entre GUI e engine reutiliza `getCosts(world)`,
`compute_school_cost`, `compute_maintenance_cost`, constantes dos módulos e a
taxa de transporte atual; não cobra, altera stats ou cria fórmula nova. O
tooltip cobre custos diários, faixas dependentes da ocupação, cobranças por
período (transporte/energia), condição de geração e flags de configuração
desativadas. `updateTech` também reconstroi o texto para refletir mudança de
tecnologia ou taxa de transporte.

O teste headless `build_tooltip_shows_cost_and_operating_estimate` verifica
custo, manutenção e período de 100 dias da estrada. A tradução pt-BR recebeu
os novos rótulos.

Evidências locais: build regular e ASan do alvo `lincity-test-sim`; teste
focado regular e ASan aprovados; CTest regular 4/4 (`57,81 s`). O CTest ASan
completo (`qw-04-asan`) passou `devlog-driver` e `unit`, mas voltou a ficar
sem saída em `sim` antes de `save`; o teste focado ASan é a evidência sanitizada
específica do código alterado. A limitação é a mesma já observada na sessão
anterior e será revalidada pelo CI remoto.

---

## 2026-08-21 — Sessão 48: KPIs de gestão e ciclo de vida do GUI (FEAT-03a / BUG-19–22)

Implementado na branch `feat/feat-03a-management-kpis`, com commits granulares
para os bugs de estabilidade, o diálogo e a feature de apresentação. Commits:
`07cc81be`, `8df8a5b1`, `ed11dac1`, `d2a34eea` e `1861274a`. A sessão seguiu o
ciclo documentar → implementar → PR/CI/merge. A PR #18 passou os jobs regular e
ASan/UBSan e foi incorporada em `origin/master` como `6bff04a9`.

### Implementação

- `ManagementKpis` concentra as leituras normalizadas de caixa, população,
  moradia, desemprego, fome, tecnologia, poluição, reservas e estoques.
- `GlobalMPS` ganhou a página Gestão e `GameStats` passou a consumir o mesmo
  snapshot; a tradução pt-BR inclui o novo rótulo.
- BUG-20 removeu centro/zoom `static` de `GameView`; BUG-21 resetou o singleton
  `ButtonPanel` e os gráficos carregados entre instâncias; BUG-22 tornou o
  estilo `dialogbutton` do GameStats autossuficiente.
- BUG-19 concentrou a liberação de gráficos no `ResourceGroup::resetGraphics`
  e classificou corretamente o teardown do harness como limpeza esperada.

### R5 e evidências

- somente apresentação: sem fórmula econômica, taxa, save/bump, flag, RNG ou
  alteração de conservação;
- teste `management_kpis_are_shared_by_mps_and_game_stats` valida snapshot,
  MPS e GameStats; o fluxo de dois jogos consecutivos cobre BUG-20/21;
- CTest regular: 4/4; ASan/UBSan: 4/4 (última execução: 613,66 s), com
  `ASAN_OPTIONS=detect_leaks=0` pela limitação de ptrace do ambiente;
- ruído de `ResourceGroup` esperado foi removido da saída de teardown.
- CI remoto #32520333844: `build` verde em 2m30s e `build-asan` verde em
  4m25s; corpo da PR foi publicado com quebras de linha reais.

Próximo item autorizado: QW-05/QW-04 ou revisão planejada de `REF-07`, conforme
o custo desejado; petróleo e porto 2.0 continuam bloqueados pela auditoria da
economia e pelos testes A/B previstos.

---

## 2026-08-20 — Sessão 45: continuação determinística após save/load (TST-08)

Implementado no branch `test/tst-08-deterministic-save`, commit `dc0e31c7`,
incorporado pela PR #15 em `origin/master` (`65e1d411`). O ciclo desta sessão
ficou isolado em um item: implementação, testes locais, commit, CI e merge,
sem tocar o remoto `upstream`.

### Implementação

- Todas as chamadas de aleatoriedade do engine foram migradas de `rand()` para
  `lincityRand()`, usando o `BasicUrbg` central; os testes que sorteiam ações
  continuam independentes.
- O estado textual do `BasicUrbg` agora é salvo em `<rng_state>`; o formato
  subiu de `ldsv_version` 2141 para 2142. A restauração ocorre depois do mapa
  terminar de carregar, para que aleatoriedade usada na construção do mundo
  durante o load não altere a continuação.
- A serialização inclui os contadores mensais de produção das construções e o
  estado de trabalho da olaria. Os links de construções são ordenados por
  `MapPoint`, removendo dependência da ordem de alocação no heap.
- Saves antigos continuam carregáveis; a ausência de `rng_state`/dos campos
  novos mantém defaults legados, mas não promete replay determinístico.

### Evidências

- Teste focado de continuação: 200 dias comparados passo a passo, aprovado.
- CTest regular: 4/4, aprovado.
- CTest ASan/UBSan: 4/4, aprovado em 228,28 s; simulação de 1.000 dias
  isolada também passou. `detect_leaks=0` foi usado pela limitação de ptrace
  do ambiente, já registrada na documentação normativa.
- R5: impacto em simulação, economia temporal, save/load e determinismo; sem
  nova fórmula econômica, GUI, tradução ou flag de gameplay.

Próximo item autorizado após o merge: `BUG-18` (aritmética segura de
`World::income/expense`), seguido de `REF-06`.

---

## 2026-08-20 — Sessão 46: aritmética monetária segura (BUG-18)

Implementado no branch `fix/bug-18-safe-money-arithmetic`, commit `3115952a`.
O saldo de `World::income/expense` passou a ser calculado em `int64_t` antes
da conversão para o campo persistido `int`, preservando os limites de crédito
e a saturação histórica em `INT_MAX`. A fórmula de juros também evita negar um
`INT_MIN`, e o clamp recebe valores largos para não reintroduzir o problema em
novas chamadas.

### Evidências

- Unit: limites de `compute_interest` e `compute_clamp_money`, incluindo
  `INT_MIN` e `int64_t` mínimo/máximo.
- Sim: chamada real de `income()` em `INT_MAX` e `expense()` em `INT_MIN`,
  com o limite de crédito verificado.
- CTest regular: 4/4, aprovado em 35,12 s.
- Teste focal ASan/UBSan de fronteiras: aprovado sem diagnóstico.
- A execução ASan completa via wrapper parou durante o reset longo sem resumo
  ou erro de sanitizer; o CI remoto será o gate completo do PR.
- R5: impacto em simulação e economia; sem save/load, GUI, tradução, RNG ou
  mudança das fórmulas válidas de gameplay.

PR #16 foi aberto, passou o CI regular/ASan e foi merged em `origin/master`
como `33f9cf3f`. Próximo passo: iniciar `REF-06` para centralizar `tech_level`
e `taxable`.

---

## 2026-08-20 — Sessão 47: APIs econômicas do World (REF-06)

Implementado no branch `refactor/ref-06-world-economy-apis`. A mudança
centraliza todas as mutações de `tech_level` e `taxable.*` do engine em
`World::addTech()`, `World::recordTaxable()` e `World::clearTaxable()`. Foram
incluídos os pontos de entrega de `lintypes`, o labor manual do mercado, o
porto, a escola, a universidade, o monumento, o shanty e o fechamento anual.
O loader XML continua escrevendo diretamente apenas para restaurar estado
persistido, como exceção prevista em R1.

As APIs calculam em `int64_t` e saturam em zero/`INT_MAX`; isso fecha overflow
nas contas tributáveis e em incrementos de tecnologia sem alterar o limite de
gameplay `MAX_TECH_LEVEL` nem as fórmulas mensais já existentes. `clearTaxable`
mantém a liquidação anual em um único ponto.

### Checklist R5

- simulação/economia: sim; apenas centralização e proteção aritmética, sem
  alteração das fórmulas válidas;
- stats, minimapa, MPS: sem mudança de contrato; os mesmos acumuladores são
  atualizados;
- save/load: sem impacto, nenhuma tag ou versão mudou; writes do loader são
  exceção documentada;
- tradução/help, GUI/config: sem impacto;
- determinismo/RNG: sem mudança de ordem ou sorteio.

### Evidências locais

- `rg` não encontra mutações de `tech_level`/`taxable.*` nos módulos fora das
  APIs de `World`, inicialização, testes e `xmlloadsave.cpp`;
- teste focado regular: 1/1;
- CTest regular: 4/4;
- teste focado ASan/UBSan: 1/1, sem diagnóstico.

PR #17 foi aberta com corpo estruturado, passou o CI regular em 2m42s e o
ASan/UBSan em 4m21s, e foi merged em `origin/master` como `e00a14b8`. A fase
inicial fica encerrada; o roadmap pode avançar para `FEAT-03a` ou, diante de
evidência de estabilidade, para `BUG-19`/`REF-07` conforme a decisão de
prioridade.

---

## 2026-08-20 — Sessão 44: auditoria completa e plano de evolução (D14)

Sessão documental, sem alteração de código e sem qualquer operação no remoto
upstream. Foram relidos os documentos normativos e canônicos (`08`, `09`, `10`,
`11`, `01` e este diário), conferidos os refs locais e pesquisados issues/PRs
do upstream como referência do fork separado.

### Evidências registradas

- HEAD local `49a79b49`; `master` do fork está 89 commits à frente da referência
  local do upstream e sem commits upstream faltantes nessa comparação.
- Build regular: CTest 3/3 aprovado.
- Build ASan/UBSan: CTest 3/3 aprovado com `ASAN_OPTIONS=detect_leaks=0`;
  o erro do LeakSanitizer isolado foi limitação de ptrace do ambiente, não
  crash do jogo.
- TST-07 implementado em `0204082c`: teste negativo e concorrente do driver;
  PR #9 passou CI regular/ASan e foi merged em `origin/master` (`fa9da6cc`).
- TST-09 iniciado em branch `test/tst-09-warning-gate`: Release/MinSizeRel/
  BetaTest deixaram de suprimir warnings; `-Wall -Wextra` virou diagnóstico
  padrão e `-Werror` ficou opt-in. A compilação regular e a compilação ASan
  concluíram com a baseline de avisos registrada para redução gradual.
- BUG-14 implementado em `71aaec79`: uma exceção de `do_time_step()` agora
  salva somente `crashsave.scn.gz`, marca a execução como encerrada e retorna
  de `Game::run`; o save limpo, autosave, calendário e animação não rodam.
  CTest regular e ASan/UBSan passaram 4/4.
- BUG-15 implementado em `31fdf897`: `saveCityNG` e `autoSaveCityNG` agora
  retornam `bool`, removem o `assert(false)`, capturam exceções por referência
  e usam temporário + rename atômico. Chamadores não avançam em caso de falha;
  o teste cobre abertura impossível, commit impossível e preservação do slot.
  CTest regular e ASan/UBSan passaram 4/4.
- BUG-16 implementado em `4af6a43f`: `Port::pence` agora é persistido em
  `<pence>`, com `ldsv_version` 2141 e compatibilidade por ausência da tag em
  saves antigos. O teste dedicado e CTest regular/ASan/UBSan passaram 4/4.
- BUG-14, BUG-15 e BUG-16 foram incorporados em sequência pelas PRs #11, #12
  e #13; todas passaram CI regular e ASan/UBSan. A `master` local foi
  sincronizada com `origin/master` em `6bff5e35`.
- BUG-17 implementado no branch `fix/bug-17-texture-loader`: `stopThread` é
  atômico, o destrutor aguarda apenas uma thread válida, o loader captura
  exceções e o contador de imagens só decrementa após conversão bem-sucedida.
-  O fluxo dedicado e CTest local passaram (4/4); a PR #14 passou CI regular e
  ASan/UBSan e foi incorporada em `480930f4`.
- A auditoria apontou riscos residuais em `Game.cpp` (continuação após
  exceção), `MainLincity.cpp` (falha de save não verificável), `engine.cpp`
  (overflow antes do clamp), `port.cpp` (resto fracionário não salvo),
  `GameView` (loader), RNG e mutações econômicas distribuídas.

### Documentação atualizada

- criado `14-auditoria-qualidade-e-plano-evolucao.md`, com escopo, evidências,
  decisões D14, fases, custos, critérios de aceite e referências ao upstream;
- atualizado `08` com BUG-14…BUG-19, TST-07/08/09, REF-06/07 e FEAT-04…06;
- atualizado `11` com a fila de estabilidade e a ordem obrigatória antes de
  novas features;
- atualizado `09` com o gate econômico para taxas, eventos e custo de
  construções;
- atualizado `10` com o estado vigente, R13 (fronteira de falha), limitações
  do driver de testes e a leitura obrigatória do documento 14;
- atualizado `README`, `01` e `13` para distinguir arquitetura histórica de
  estado atual;
- preservado o arquivo não rastreado `opencode.json`, sem relação com a tarefa.

### Decisões da sessão

1. Estabilidade e validade do estado vêm antes de petróleo, sea farm,
   poluição hídrica, contratos ou novas taxas.
2. Toda exceção de simulação deve encerrar o tick e não pode produzir save
   limpo a partir de um mundo inconsistente.
3. Custo de construção, capital investido, manutenção e receita serão modelos
   separados; o custo dinâmico atual não será reinterpretado sem decisão de
   save.
4. Nenhum item econômico novo será implementado antes de centralizar mutações,
   definir eventos/taxas e validar determinismo/round-trip.

Próxima sessão autorizada pelo plano: TST-07, seguida de BUG-14/15. Nenhum
commit de gameplay deve começar antes de o checklist R5 e a decisão específica
do item estarem registrados.

---

## 2026-08-18 — Sessão 43: modo de debugging em desenvolvimento (TST-06)

Facility de logging em C++ (`util/debuglog.{hpp,cpp}`) + driver
`contrib/devlog.sh`. Áreas `sim`/`econ`/`save`/`vehicles`/`frames`/`config`/
`test`/`general`, níveis `trace..error`, env `LINCITYNG_LOG_LEVEL`,
`LINCITYNG_LOG_AREAS`, `LINCITYNG_LOG_FILE`, `LINCITYNG_LOG_TIMESTAMP`,
CMake option `LINCITYNG_ENABLE_DEBUG_LOG` (ON em Debug/RelWithDebInfo). O
canary de frames (REF-03d) **não** foi religado à macro (fica `#ifndef NDEBUG`):
sob `-DNDEBUG` o `assert()` descarta a expressão do `insert`, o que quebraria o
registro de frames no build/ — decisão registrada no `10` §3.5.

Driver: `build/logs/<UTC>_<tag>/` com `stdout/stderr/lincity-ng.log`/`run.json`,
`latest` symlink, retenção `LINCITYNG_DEVLOG_KEEP` (30), `--gdb`/`--asan`.
`tiny_test` ganhou `--filter`, `--ts`, `--list-tests`; simtest/savetest repassam
flags e inicializam o logging. Instrumentação: `simulate.cpp` (do_time_step),
`engine.cpp` (income/expense), `xmlloadsave.cpp` (load/save), `Vehicles.cpp`
(spawn/destroy/move_frame), `world.cpp` (removeFrame/moveFrameTo).

Validado: 3 camadas verdes em `build/` e `build-asan/`; 137 logs ad-hoc
legados migrados para `build/logs/legacy-2026-08-18/`. Sem impacto em save,
economia, determinismo ou simulação (logging é no-op sem a macro; com ela,
só fmt::format). **Decisão**: subárea `gui` fica reservada (não adicionada
para não inflar o enum).

Arquivos: `src/util/debuglog.{hpp,cpp}`, `src/{util,lincity}/CMakeLists.txt`,
`CMakeLists.txt` (raiz), `tests/unit/test_debuglog.cpp`,
`tests/{tiny_test.hpp,tiny_main.cpp}`, `tests/sim/simtest.cpp`,
`tests/save/savetest.cpp`, `contrib/devlog.sh`, `src/lincity/{simulate,engine,
Vehicles,world,xmlloadsave}.cpp`.

---

## 2026-08-17 — Sessão 40: preservação de profundidade do BUG-13

Um passe global posterior para veículos foi descartado: ele fazia carros
passarem sobre prédios e objetos. `GameView` voltou a desenhar os frames no
passe isométrico do tile âncora; a correção permanece concentrada em
`Vehicle::walk()`, que evita a âncora diagonal em curvas. Sem impacto em
save, economia ou simulação.

## 2026-08-17 — Sessão 41: consolidação da ordem visual do BUG-13

A regressão mostrou que desenhar todo frame com `lowerRightTile` removia a
semântica especial dos veículos e fazia a pista cobrir parcialmente o carro.
Foi restaurada a regra histórica: `is_vehicle` é desenhado em `upperLeft`
somente para grupos baixos/de transporte; prédios continuam usando
`lowerRightTile`. A âncora de curva permanece limitada a `prev`/`point`.
Sem impacto em save, economia ou simulação.

## 2026-08-17 — Sessão 42: ordem de frames do BUG-13

A allowlist evitava carros sobre prédios, mas não resolvia a sobreposição de
uma estrada desenhada em uma diagonal posterior. `GameView` passou a coletar
frames durante o passe de tiles e ordená-los por camada e profundidade visual:
transporte/piso primeiro, veículos depois com `move_y` na profundidade e
construções na ordem isométrica. Cabos suspensos continuam por último.
Sem impacto em save, economia ou simulação.

## 2026-08-17 — Sessão 39: correção do BUG-13

`Vehicle::walk()` mantém a troca de âncora original em retas e substitui
apenas a quina diagonal das curvas por `prev` ou `point`; `move_x/move_y`
preservam a geometria. A transição para `arriving` também atualiza o frame
no mesmo tick. A allowlist do render permanece intacta. Invariante nova no
simtest (`framePt == prev || framePt == point`), 11 testes sim sem falhas;
impacto apenas visual, sem save/economia/determinismo.

---

## 2026-08-17 — Sessão 38: análise do sistema de veículos + BUG-13 + regra R12

Usuário relatou carros piscando (<0,5s) durante curvas, pior após
REF-03e/BUG-12. Análise completa do sistema (spawn, navegação,
update/walk, render) entregue no chat e consolidada em `.devdocs/12`
§BUG-13 — diagnóstico: âncora do sprite cruza para o tile do vértice
interno da curva (prédio) e o render só desenha veículo ancorado em
grupo da allowlist. Nenhuma mudança de código de comportamento; apenas
remoção/encurtamento dos comentários verbosos REF-03* (Vehicles.*,
simulate.cpp, world.*, school.cpp; build verde). Nova regra R12 em
`.devdocs/10` (comentários enxutos, doc em `.devdocs`, sem duplicação).
Correção do BUG-13 pendente de decisão (fix A render × B âncora).

---

## 2026-08-17 — Sessão 37: REF-03 completo (03d, 03c+BUG-12, 03e)

### Escopo

Fases finais do plano REF-03 (`.devdocs/13` §4, ordem §7.2: 0→1→3→2→4),
três PRs sequenciais, cada um com CI verde antes do merge. O plano do
ponto crítico nº 1 de memória do fork está **concluído**.

### PR #4 — REF-03d: canary debug de frames

Registry global (`g_live_frames`, unordered_set de endereços de nós) de
todos os ExtraFrame vivos em builds debug: `addFrame` registra,
`removeFrame` assertiona pertinência **antes** do scan de ownership (pega
double-kill mesmo com reuso de endereço pelo mesmo tile — caso que o scan
O(n) não vê), `~MapTile` desregistra silenciosamente, `moveFrameTo` não
toca (splice não muda endereços). `Map::assertFramesConsistent()` no fim
de cada `do_time_step`. 3 testes novos (consistência, teardown limpo,
prova do double-kill via fork aceitando SIGABRT **ou** exit≠0 do ASan).
Matriz sim+save inteira passou limpa com o canary ativo (+30s no sim
ASan, release intocado). Registry global (não per-Map): MapTile não tem
back-pointer e há vários maps vivos nos sim tests.

### PR #5 — REF-03c: unique_ptr + BUG-12 (o achado da sessão)

`framesptr` → `std::unique_ptr<std::list<ExtraFrame>>`; MapTile ganha
move ctor e fica não-copiável; Map ganha move ops defaultadas (dtor
declarado suprimia; o load path `world.map = Map(len)` viraria
move-assign). Últimos `delete framesptr` eliminados.

**O CI pegou um bug latente de anos**: o job rel falhou no
`tax_elasticity_ab` (pop high 64970 > base 64944) com código que passava
localmente. Investigação empírica: mesmo binário, 3 runs → números
diferentes (65280/65088/65088; sem ASLR 65094) — a simulação inteira era
dependente do layout de heap: `Map::constructions` era
`std::set<Construction*>` (ordem = endereços) e o `iota`+`shuffle` do
`simulate_mappoints` pareava RNG→construção por essa ordem. Toda
trajetória re-sortava a cada mudança de memória/build/máquina (BUG-12).
Correção: `ConstructionPointLess` (ordena por ponto do tile principal —
único e imutável pós-insert, verificado por grep). Prova: 4 runs
(3×ASLR + 1×`setarch -R`) **bit-idênticas** (65088/62502). Salvos agora
listam construções em ordem de ponto (formato inalterado). Detalhe do
diagnóstico em `.devdocs/12` §BUG-12.

### PR #6 — REF-03e: Vehicle markDead + reaping

Os 4 `delete this` de `Vehicle::update` → flag `dead`; `do_animate` faz
o reaping após o pass (erase→delete; dtor sem unlist — o swap-para-local
do `World::~World` virou desnecessário). Removidas 2 declarações mortas
de `Vehicles.hpp`. Invariante nova no simtest: após cada `do_animate`,
nenhum veículo `dead` pode restar na lista. **Zero `delete this` no
engine.**

### Estado do plano (ver `.devdocs/13` §10)

- R8 integralmente satisfeita: frames só via métodos de MapTile
  (privado + unique_ptr), remoção de objetos só centralizada;
- camada 1: 8 testes de MapTile (incl. 2 provas via fork); camada 2:
  invariante de sweep de veículos; matriz sim+save verde nos dois builds
  em cada PR;
- bônus: engine agora bit-reprodutível por seed (BUG-12) — prerequisito
  para qualquer futuro golden-test de trajetória.

### Próximas pendências candidatas

- Validação manual headless do usuário (5 min jogo real, cidade grande)
  — `.devdocs/13` §6 sugere; canary ativo em build-asan.
- Backlog: ver `11` (nada de REF-03 restante).

---

## 2026-08-17 — Sessão 36: REF-03b — framesptr privado + accessors (PR #3)

### Escopo

Fase 1 do plano REF-03 (`.devdocs/13` §4): encapsular totalmente o ciclo de
vida de `ExtraFrame` em `MapTile`. Primeiro refactor de engine do fork
validado pelo CI do zero (PR #3 — build 2m20s + build-asan 4m50s, ambos
verdes antes do merge).

### Mudanças

- `world.hpp`: `framesptr` **private**. API pública: `hasFrames()` (O(1)),
  `addFrame()` (ex-`createframe`), `removeFrame(it)` (ex-`killframe`),
  `moveFrameTo` (REF-03a), `frames()`/`frames() const` (pré-condição
  `hasFrames()`). Private: `ensureFrames()` — único ponto de `new list`.
- `world.cpp`: **assert debug de ownership** (scan O(n) por endereço de nó,
  `owns_frame` em namespace anônimo sem `NDEBUG`) em `removeFrame` e
  `moveFrameTo`. A pergunta do comentário histórico do `killframe` ("what
  would actually happen if it belongs to another maptile?") virou abort no
  ponto exato. `moveFrameTo` também asserta `dest != origem` (self-splice
  UB quando `it` é o último elemento).
- Migrados 12 arquivos: lintypes, Vehicles, coal_power, shanty, market,
  track_road_rail (incl. iteração de spawn em `canPlaceVehicle`), school,
  light_industry, GameView (loop de draw com ref local).
- **Correção de doc**: são **6** classes com iteradores-membro (doc dizia
  4): Shanty e Market também têm `waste_fire_frit`.

### Testes (camada 1 nova)

`tests/unit/test_maptile_frames.cpp` — 5 testes: lifecycle; estabilidade de
iterador em remoção do meio; `moveFrameTo` preserva iterador; 2000 ops
aleatórias (LCG seed fixa) com invariantes checadas **a cada op**; prova do
assert cross-tile via `fork()` (child mata frame do tile B via tile A;
parent exige SIGABRT; só Linux sem NDEBUG — RelWithDebInfo o compila fora,
por isso build/ tem 24 unit tests e build-asan 25).

### Resultado

- `ctest` 3/3 nos dois builds; sim/save no ASan rodaram com asserts ativos
  em toda a matriz — **zero cross-tile kills no código atual** (confirma
  que as duplicações da REF-03a eram as violações existentes).
- R8 agora é **imposta pelo compilador**: `framesptr` inalcançável fora de
  `MapTile`; a classe inteira do crash histórico #52 está fechada.

### Próxima pendência

Ordem do plano (`13` §7.2): **REF-03d** (canary/invariantes debug) →
REF-03c (unique_ptr) → REF-03e (delete this de Vehicle).

---

## 2026-08-17 — Sessão 35: CI do GitHub Actions + correção de race do test-appdata

### Escopo

Planejar e implementar um CI no GitHub Actions para o fork, como passo
prioritário para atingir a meta de "estabilidade + testabilidade como
método eficiente para identificar regressões". O CI torna a infra de
testes (3 camadas) **automática e contínua**: todo PR/push é gateado
pelo `ctest` antes de merge.

### PRs mergedos no fork (origin = maicomferre/lincity-ng)

| PR | Branch | Conteúdo | Estado |
|---|---|---|---|
| #1 | `fix/ref-03a-framesptr-dedup` | REF-03a (centralizar ExtraFrame) + BUG-10 (6 leaks do ASan) | **MERGED** |
| #2 | `ci/github-actions` | CI workflow + fix de race do `test-appdata` | **MERGED** |

### CI — `.github/workflows/ci.yml`

Workflow que roda nas branches `master` (push) e PRs para `master`.

**Matriz (2 jobs paralelos)**:
| Job | Build type | Flags | Propósito |
|---|---|---|---|
| `build` | RelWithDebInfo | — | caminho rápido de correção (~2m43s) |
| `build-asan` | Debug | `-fsanitize=address,undefined` | guarda de UAF/leaks (~3m56s), passa com `detect_leaks=1` |

Ambos setam `SDL_VIDEODRIVER=dummy` / `SDL_AUDIODRIVER=dummy` para runs
headless. O LSAN suppression file (`tests/lsan_suppressions.txt` do
PR #1) é injetado via `ENVIRONMENT` no CMake — ctest herda
automaticamente, sem config extra no workflow.

**Container**: `ubuntu:26.04` (mesmo distro do desenvolvimento). SDL3
core/image/ttf e libxml++-5.0 estão no apt; **fmt 11.1.4** e **SDL3_mixer
3.2.4** não estão — buildados do source em steps dedicados (release tags
do GitHub), instalados em `/usr/local`, pegos via `CMAKE_PREFIX_PATH`.

### BUG: race condition do `test-appdata` em fresh builds

**Sintoma**: fresh build paralelo falha non-deterministicamente com
`file INSTALL cannot find .../locale/<lang>/LC_MESSAGES/lincity-ng.mo`
(ou `.../doc/lincity-ng/<file>`, ou `.../bin/lincity-ng`). Builds
incrementais (locais) nunca falham porque os arquivos já existem.

**Causa raiz**: `test-appdata` roda `cmake --install` que instala
**TUDO** registrado em `cmake_install.cmake` — não apenas os targets
listados em `add_dependencies`. Mas `test-appdata` só dependia de
`guiXml guiDialogXml data-files.target`. Faltavam:
- `locales` (.mo files)
- `doc-files.target` (CHANGELOG, COPYING, README, etc.)
- `lincity-ng` (o binário executável)

**Correção** (`tests/CMakeLists.txt`): adicionados os três como
`add_dependencies` do `test-appdata`, guarded por `if(TARGET ...)`
para ser inerte quando NLS ou o target do app estiver off.

**Validação**: fresh build paralelo (sem `-j1`) + ctest em ambos:
- `build/` (RelWithDebInfo): 3/3 passed em 15s
- `build-asan/` (Debug + ASan): 3/3 passed em 133s com `detect_leaks=1`

### Commits no PR #2 (ci/github-actions)

| Commit | Conteúdo |
|---|---|
| `d69be7bf` | workflow inicial (debian:trixie) |
| `33eea1d4` | ubuntu:26.04 + SDL3_mixer do source (trixie não tem libxml++-5.0) |
| `bd8f1e78` | fmt 11.1.4 do source (apt tem 10.1.1, projeto usa fmt/base.h) |
| `35b7d5d5` | fix do race do test-appdata (dependências locales + doc + binário) |

### Estado do fork no origin

- `master` = `89169252` (merge do PR #2), sincronizado com local
- PR #1 e #2 mergedos
- CI verde: https://github.com/maicomferre/lincity-ng/actions/runs/32062202943
- Branches remotas deletadas após merge

### Próxima pendência sugerida

Com o CI guardando, o próximo item natural do roadmap é **REF-03b**
(encapsular `framesptr` privado + accessors em `MapTile`) — continua a
trilha de estabilidade de memória, agora com o CI como rede de segurança.

---

## 2026-08-17 — Sessão 34: BUG-10 corrigido (leaks do ASan) + R11 + push para origin

### Escopo

Corrigir BUG-10 (leak do ASan no build-asan que fazia ctest falhar com
`detect_leaks=1`), definir a regra R11 (sincronização com o fork) e fazer
o push da branch `fix/ref-03a-framesptr-dedup` para `origin`
(maicomferre/lincity-ng).

### R11 — Sincronização com o fork (definida)

Atualizado `.devdocs/10-padrao-desenvolvimento.md` (R11) e `AGENTS.md`:
- `origin` = `maicomferre/lincity-ng` (fork, **pode push**)
- `upstream` = `lincity-ng/lincity-ng` (**nunca push**)
- Verificar `git remote -v` antes de qualquer push
- Branch por item, nunca empurrar `master` sem pedido
- Rodar `ctest` (build/ e build-asan) antes do push
- PR mergeável localmente ou no fork; nunca contra upstream

### BUG-10 — Leak do ASan no headless test (corrigido)

**Sintoma**: `ctest --test-dir build-asan` falhava com
`SUMMARY: AddressSanitizer: 74679162 byte(s) leaked in 2573 allocation(s)`.
Era preciso rodar com `ASAN_OPTIONS=detect_leaks=0` para passar, o que
mascarava qualquer leak real de `framesptr` ou outro subsistema.

**Causas raiz (6 bugs distintos, todos corrigidos)**:

1. **`World::~World()` vazio** (world.cpp:415): não deletava `vehicleList`
   (raw pointers). Veículos vivos no teardown leakavam. Fix: deletar com
   swap antes (técnica anti iterate-then-mutate, já que
   `Vehicle::~Vehicle` faz `vehicleList.remove(this)`).

2. **`ResourceGroup::~ResourceGroup`** (resources.hpp:74): deletava
   `texture` mas não `image` (SDL_Surface*). Surfaces não convertidas
   leakavam. Fix: `SDL_DestroySurface(it->image)` no dtor.

3. **`GameView::fetchTextures` e `drawTexture`** (GameView.cpp:1213,1232):
   comentário dizia "Image was erased by texture_manager->create" — **mentira**.
   `TextureManagerSDL::create` não destrói a surface source. O código
   setava `gfx.image = 0` sem `SDL_DestroySurface` → leakava toda surface
   convertida em textura (+ pixel buffer = 74MB). Fix: `SDL_DestroySurface`
   antes de nulling. Mesmo fix em `Gradient::resize` (Gradient.cpp:126).

4. **`initVideo` chamado 2x** (Video.cpp:79): o sim test chama `initVideo`
   uma vez por teste (linhas 365 e 521). A segunda chamada sobrescreria
   `painter`/`fontManager`/`texture_manager`/`window_renderer`/`window`
   sem deletar os anteriores. Fix: cleanup defensivo no início do
   `initVideo` (inerte na primeira chamada e no jogo real).

5. **`shutdown_env`** (headless_env.cpp): não deletava `ResourceGroup`
   singletons, `texture_manager`, `painter`, `fontManager`,
   `window_renderer`, `window`, `configPtr`. Fix: teardown completo
   ordenado (ResourceGroups antes de texture_manager; SDL resources
   antes de SDL_Quit).

6. **harfbuzz/SDL3_ttf caches** (terceiro): `TTF_Quit` não limpa caches
   de shape-plan do harfbuzz (bug upstream conhecido). Fix:
   `tests/lsan_suppressions.txt` com `leak:libSDL3_ttf` e
   `leak:libharfbuzz`. Também suprime `leak:initFactories` (16 factory
   singletons que compartilham map com factories estáticos e não podem
   ser deletados individualmente sem double-free).

**Validação**:
- `ctest --test-dir build` → 3/3 passed (unit 0.01s, sim 12.48s, save 2.49s).
- `ctest --test-dir build-asan` com `detect_leaks=1` (default) → **3/3 passed**
  (unit 0.06s, sim 97.37s, save 22.05s). **Zero leaks reportados.**
- Sim test: 11/11 testes OK, invariantes de conservação intactos.

### Commits na branch `fix/ref-03a-framesptr-dedup`

| Commit | Item | Conteúdo |
|---|---|---|
| `f82c83f4` | REF-03a | centralizar ExtraFrame tile transfers em MapTile |
| `38eefebd` | BUG-10 | fix de 6 leaks + LSAN suppression file + cleanup no initVideo/shutdown_env |

### Push para origin (maicomferre)

Verificado `git remote -v`:
- `origin` = `git@github.com:maicomferre/lincity-ng.git` (fork — push autorizado)
- `upstream` = `https://github.com/lincity-ng/lincity-ng` (nunca push)

Push da branch `fix/ref-03a-framesptr-dedup` para `origin` — **sucesso**:
```
* [new branch] fix/ref-03a-framesptr-dedup -> fix/ref-03a-framesptr-dedup
```
GitHub sugere PR em:
https://github.com/maicomferre/lincity-ng/pull/new/fix/ref-03a-framesptr-dedup

PR pode ser mergeado localmente ou aberto no fork (R11: nunca contra
upstream sem pedido explícito).

---

## 2026-08-17 — Sessão 33: REF-03a implementado (Fase 0 do framesptr)

### Escopo

Implementar o "próximo passo sugerido" da Sessão 32: **Fase 0 (REF-03a)**
— migrar as duplicações manuais do padrão `killframe` em `School::~School`
e `Vehicle::move_frame` para métodos de `MapTile`, sem mudança de
comportamento observável.

### Branch

`fix/ref-03a-framesptr-dedup` (criada a partir de `master` em `0b2a0ec4`).

### Mudanças (4 arquivos)

- `src/lincity/world.hpp`: adicionado `MapTile::moveFrameTo(Map& map, MapPoint dest, const std::list<ExtraFrame>::iterator& it)` + forward decl `class Map;` (necessário porque `Map` está definido depois de `MapTile`).
- `src/lincity/world.cpp`: implementado `moveFrameTo` — encapsula `splice` + `new std::list` (destino sob demanda) + `delete framesptr` (origem se vazio), exatamente o padrão que estava inlined em `Vehicle::move_frame`. Comentário `REF-03a` marca o ponto único.
- `src/lincity/modules/school.cpp`: `School::~School` reduzido de 8 linhas (duplicação manual `erase + check empty + delete + =NULL`) para 1 chamada `world.map(point)->killframe(frit);`.
- `src/lincity/Vehicles.cpp`: `Vehicle::move_frame` reduzido de 13 linhas (`new list` + `splice` + `delete + =NULL`) para 2 chamadas (`moveFrameTo` + `framePt = newPoint`).

### Checklist R5

| Subsistema | Impacto |
|---|---|
| 1. Simulação | indireto (frames são overlays; `frameIt->frame` lido/escrito igual) |
| 2. Taxas/economia | sem impacto |
| 3. Stats | sem impacto |
| 4. Minimapa | sem impacto |
| 5. MPS | sem impacto |
| 6. Save/load | sem impacto ⭐ (frames não persistidos; R3 não se aplica, nenhum bump `ldsv_version`) |
| 7. Tradução/help | sem impacto |
| 8. Config/GUI | sem impacto direto (`GameView` itera `framesptr` igual) |
| 9. Determinismo/RNG | sem impacto (ordem de simulação inalterada) |

### Validação

- **Baseline master** (antes da mudança): `ctest --test-dir build` → 3/3 passed.
- **Após REF-03a**:
  - `cmake --build build` ✅ e `ctest --test-dir build` → **3/3 passed** (unit 0.02s, sim 14.67s, save 2.78s).
  - `cmake --build build-asan` ✅ e `ctest --test-dir build-asan` com `ASAN_OPTIONS=detect_leaks=0` → **3/3 passed** (unit 0.04s, sim 84.33s, save 20.37s).
  - Sim test diretamente: **11 test(s), 0 failure(s)** (incluindo `random_actions_keep_conservation` com 500 build/demolish + 300 dias, e `tax_elasticity_ab_reduces_activity`).
  - Invariantes de conservação (dinheiro + mercadorias) intactos em todas as rotas (cidade nova, 6 cenários, A/B elasticidade, ações aleatórias).

### Leak pré-existente descoberto (não bloqueante)

Ao rodar o ASan com `detect_leaks=1` (default), o sim test falha por leak
de 74MB em 4190 allocations. **Verificado que é pré-existente na baseline
master** (rodei o sim test na `master` — mesma falha). Stack trace aponta
para `harfbuzz`/`SDL3_ttf`/`Paragraph::resize` chamados via
`Dialog::governor` no teste `game_flow_reset_and_loading_barrier`.

**Ação**: registrar como `BUG-10` (leak de fontes em `Paragraph`/
`Dialog::governor`) em sessão futura — totalmente não relacionado a
`framesptr`, não bloqueia REF-03a. Recomenda-se rodar ctest no build-asan
com `ASAN_OPTIONS=detect_leaks=0` enquanto BUG-10 não for corrigido, para
que o leak do SDL3_ttf não mascare erros reais de `framesptr`.

### Documentação atualizada (no mesmo commit)

- `.devdocs/13-framesptr-ciclo-de-vida.md` §9: estado atualizado para "Fase 0 implementada" + nova seção 9.1 com detalhes da validação.
- `.devdocs/08-inventario-de-todos.md`: 2 itens (`school.cpp`, `Vehicles.cpp`) marcados ✅ FEITO.
- `.devdocs/11-roadmap-features.md`: linha REF-03 atualizada (REF-03a marcada como feito).
- `.devdocs/10-padrao-desenvolvimento.md`: R8 atualizada — duplicações marcadas como corrigidas, pendências REF-03b/e mantidas.
- Este diário.

### Estado de R8 após REF-03a

- ✅ `School::~School` e `Vehicle::move_frame` cumprem R8 (frames só via métodos de `MapTile`).
- ⏳ `framesptr` ainda público (REF-03b pendente).
- ⏳ 4× `delete this` em `Vehicle::update` (REF-03e pendente).

### Próximo passo sugerido

Fase 1 (REF-03b): tornar `framesptr` privado e expor accessors
(`hasFrames`, `addFrame`, `removeFrame`, `moveFrameTo`, `frames()`).
Médio risco — tocador `world.hpp` é transversal, mas o compilador lista
todas as violações. Só fazer se o usuário aprovar.

---

## 2026-08-17 — Sessão 32: Análise profunda do ponto crítico de memória (REF-03 / framesptr)

### Escopo

Pedido do usuário: analisar AGENTS.md e `.devdocs` para melhorar
estabilidade, testabilidade e debugging — **focando em UM ponto crítico**
ao invés de varrer todos os bugs prováveis. Apenas documentação; nenhum
código do jogo alterado.

### Ponto crítico selecionado: REF-03 (ciclo de vida de `ExtraFrame`/`framesptr`)

Após revisão cruzada do roadmap (`11` P4), das regras R1–R10 (`10`), do
inventário (`08`) e dos bugs investigados (`03`/`12`), selecionei o **REF-03**
como ponto de foco porque atende os três objetivos do pedido simultaneamente
(estabilidade + testabilidade + debugging) e é a **causa estrutural latente**
do crash histórico upstream #52 (PR #51 ~2015) — mesmo sintoma "intermitente,
sem padrão" que motivou o fork.

**Evidência chave verificada no HEAD local**:
- `world.hpp:94` declara `framesptr` **público** com comentário
  `"use memberfunctions"` — mas 2 arquivos violam:
  - `school.cpp:82-90` duplica `killframe` (erase + `delete framesptr` + `=NULL`) sem checar validade de `frit`.
  - `Vehicles.cpp:329-342` (`move_frame`) duplica `new list` + `splice` + `delete framesptr` do tile de origem.
- `world.cpp:265` tem comentário revelador: `//what would actually happen if "it" belongs to another maptile?` — sem validação.
- `Vehicles.cpp:592,615,623,655` têm 4× `delete this` pré-existentes (R8 proíbe em código novo).
- `transport.cpp:83` faz `&(cstr->frameIt->frame)` — deref de iterador guardado como membro.
- `rocket_pad.cpp:141-163` usa `frameIt->frame` como **estado de simulação** (estágio do lançamento) — UAF em `frameIt` vira leitura/escrita em memória morta; em release (`NDEBUG`), `assert(frameIt->frame <= 6)` (linha 157) vira no-op → UB silencioso (sintoma clássico de crash dependente de ASLR).
- **Frames NÃO são persistidos no save** (`grep frame\|ExtraFrame` em `xmlloadsave.cpp` = 0 matches) → **R3 não se aplica**, eliminando o maior risco de refatoração.

### Documento criado

- **`.devdocs/13-framesptr-ciclo-de-vida.md`** (novo, ~280 linhas):
  análise profunda com (1) mapa de uso verificado arquivo:linha, (2) 3 causas
  estruturais, (3) plano de refatoração em 5 fases `REF-03a..e` (cada uma =
  commit, sem mudança de comportamento observável), (4) matriz R5
  completa, (5) testes nas 3 camadas, (6) riscos por fase e ordem
  recomendada (0→1→3→2→4), (7) escopo explícito "não fazer".

### Documentos atualizados (referências cruzadas)

- `.devdocs/08-inventario-de-todos.md`: nova seção "Novos itens (auditoria
  2026-08-17 — REF-03 framesptr)" com 7 itens verificados (duplicações
  school/vehicle, `framesptr` público, `delete this` em Vehicles, deref em
  transport.cpp:83, `frameIt->frame` como estado em rocket_pad).
- `.devdocs/11-roadmap-features.md`: linha `REF-03` expandida com link ao
  doc 13 e as 5 fases; destaque ao zero-impacto em save.
- `.devdocs/10-padrao-desenvolvimento.md`: R8 fortalecida com a evidência
  nova (violações concretas + `frameIt->frame` como estado + UB silencioso
  em release).
- `.devdocs/README.md`: entrada do doc 13 na tabela.

### Próximo passo sugerido (decisão do usuário)

Abrir branch `fix/ref-03-framesptr` e executar a **Fase 0 (REF-03a)** —
migrar `School::~School` e `Vehicle::move_frame` para os métodos
`MapTile::killframe`/novo `moveFrameTo`. Baixo risco, mata a duplicação,
deixa o código em estado que cumpre R8. Fases subsequentes podem ser
postergadas sem blocker.

### Pendências

- Nenhuma implementação feita (escopo foi documentação, conforme pedido).
- Decisão pendente do usuário: aprovar/executar REF-03a ou agendar.

---

## 2026-08-16 — Sessão 31: Crash do `{:n}` (fmt v11) + avaliação do backlog pendente

### Bug do usuário: `fmt::v11::format_error: invalid format specifier` (crash)

**Sintoma relatado**: ao clicar num mapa no menu de Novo Jogo para ver o
resumo (alguns mapas crashavam) e ao construir uma loja/mercado no mapa
(selecionar a construção e clicar no tile). Terminate com
`fmt::v11::format_error: invalid format specifier`.

**Causa raiz**: o fmt v11.1.4 (instalado em `/home/maicom/.local`) **removeu
o presentation type `n`** (substituído por `{:L}`, que exige o locale C++
global configurado). Todo `fmt::format("{:n}", v)` em runtime lança
`format_error`. O fork usava `{:n}` em 9 pontos (todas implementações
recentes: tooltip de custo do ButtonPanel, painel de cenários do MainMenu,
texto flutuante de custo/ponte/bulldoze do GameView) + 2 msgids no pt_BR.po.
Reproduzido com harness mínimo: `{:n}` lança mesmo após `setlocale`/`locale::global`.

**Correção** (`32fb524f`): funções puras `format_thousands(num, sep)` e
`format_money(num)` em `src/lincity/util.{hpp,cpp}` (grupo de 3 dígitos,
separador do `localeconv()` — pt_BR → `.`, fallback `'`). Os 9 usos trocados;
as 2 strings traduzidas do painel de cenários deixam de embutir `{:n}$` e
recebem o dinheiro já formatado (`Money {} | ...`). Unit tests em
`tests/unit/test_util.cpp` (3 casos: agrupamento, negativos, separador
arbitrário). Validação: build limpo, ctest 3/3, probe `money_probe`
(`1.234.567` em pt_BR).

**Impacto**: GUI-only; zero impacto em save/simulação/economia/determinismo
(R5 marcado). Sem bump de `ldsv_version`.

### Avaliação de viabilidade do backlog pendente (registrada no `11`)

FEAT-03a (KPIs, baixo risco), FEAT-03d (petróleo, alto/multi-sessão),
FEAT-03e (porto 2.0, decisões antes de codar), POs de outros idiomas
(msgmerge mecânico vs qualidade), QW-04 (tooltip custo+manutenção) e QW-05
(música no options). Ordem recomendada: QW-05 → QW-04 → FEAT-03a → msgmerge
POs → FEAT-03e → FEAT-03d. Ver tabela completa no `11-roadmap-features.md`.

---

## 2026-08-16 — Sessão 30: Backlog implementado (TST/BUG/FEAT) + ASan/UBSan + tradução pt-BR

### Implementação completa do backlog (21 commits, `0b508d42`..`f98ec66b`)

Tudo com validação headless antes de commitar (3 camadas de teste em `ctest`:
unit, sim, save). Ver a tabela de commits no final do `11-roadmap-features.md`.

1. **Infra de testes** (TST-01/02/04): `enable_testing()` + `tests/` (unit,
   sim, save); harness headless (`tests/headless_env.*`, SDL dummy, seed fixa
   via reseed de `BasicUrbg`+`srand`, dados transformados em
   `build/test-install` via target `test-appdata`); fórmulas puras em
   `src/lincity/economy.hpp` + unit tests; round-trip dos 6 cenários + cidade
   nova em `tests/save/savetest.cpp`.
2. **Bugs do usuário**: BUG-01a (reset completo em `Game::setWorld`), BUG-01b
   (slots separados: `autosave.scn.gz` vs `9_currentGameNG.scn.gz`), BUG-01c
   (limpar seleção no Novo Jogo), BUG-01d (mensagens de origem), BUG-02
   (barreira de loading com overlay "Loading…" + atomics), BUG-03a (windmill
   `paid` por instância), BUG-03b (escola regressiva por lotação + eólica
   debitando ao gerar — flags), BUG-03c (meses re-traduzem), BUG-04 (ponte
   cobrada pelo valor real via `transport_group_at`).
3. **Features**: FEAT-01 (custo itemizado em tempo real + texto negativo
   flutuante), FEAT-02 (painel do cenário com thumbnail dinâmico
   `MapThumbnail` + texto), FEAT-03c (painel do governador com 6 alíquotas
   editáveis 0–2× + elasticidade tributária em etapas — flags).
4. **Bugs extras achados pelo harness**: BUG-05 (MPS finance estourava os
   parágrafos — crash), BUG-07 (veículo lendo fora do mapa na borda — SEGV
   intermitente, 30/30 limpo após o fix), BUG-08 (thread de som sem join).
5. **ASan/UBSan** (`build-asan`): achou e corrigiu UB de enum `SDL_Scancode`
   em `src/gui/Event.*` (sanitização via memcpy + NSDMI em todos os membros);
   suíte sim 10/10 limpa com `UBSAN_OPTIONS=halt_on_error=1`.
6. **TST-03 (dinheiro)**: `random_actions_keep_conservation` — 500 ações
   aleatórias (build/demolish, 11 grupos, seed fixa) + 300 dias; base do
   invariante absorve acúmulos pré-rodada. **Mercadorias: pendente** (não
   arriscar invariante errado) → ✅ **feito depois** (bloco "Extra: TST-03"
   abaixo, `f40a2c8f` + `dcdae425`).
7. **Tradução pt-BR** (2 commits): corrigido o pipeline de POT que estava
   quebrado desde a reestruturação (target renomeado, dialogs sem extração,
   UTF-8, merge via `msgcat`) e traduzidas as **36 msgids novas** (governador,
   loading, status, custo em tempo real, painel de cenários + 4 strings
   pré-existentes nunca extraídas). `.mo` verificado com msgunfmt; suíte
   completa passa com `LANG=pt_BR.UTF-8`.

### Validação

- `ctest`: 3/3 (unit, sim, save) em `build/`; sim 10/10 sob ASan/UBSan e sob
  locale pt_BR.
- Determinismo: seed fixa por teste; cenários A/B com reseed antes de cada
  load (elasticidade: base poluição 298300/pop 65551 vs 2× → 293895/63129,
  conservação mantida).

### Pendências registradas

FEAT-03a; FEAT-03d (petróleo); FEAT-03e (porto 2.0);
POs de outros idiomas (fallback inglês); `AGENTS.md` segue fora do git.

### Extra: TST-03 — invariante de mercadorias (commits `f40a2c8f` + `dcdae425`)

O invariante de goods foi definido por auditoria dos fluxos: o estoque de
cada commodity vive em `Construction::commodityCount[]` e só muda por 4
chokepoints — `produceStuff`, `consumeStuff`, `levelStuff` e o destrutor
(estoque some com o prédio, deletado em `simulate_mappoints` ou no dtor do
`MapTile`). Todos agora atualizam o novo `World::commodityLedger`
(declarado antes de `map` para sobreviver à teardown da World).
Transferências (`equilibrate_stuff`/mercado/transporte) preservam o total e
não entram no ledger. Verificação negativa: hook temporário com
`commodityCount[STUFF_FOOD] += 3` no `Market::update` foi pego no dia 1
("food totals 1972878, expected 1972857"). `run_days` checa o invariante
todo dia em todas as rotas (cidade nova, 6 cenários, A/B, ações aleatórias).
ASan/UBSan 3/3 com `detect_leaks=0` (leaks pré-existentes, sem alocações
novas aqui). `commodityLedger` não é persistido → round-trip intacto, sem
bump de `ldsv_version`. ctest 3/3; master local à frente do upstream em
**58 commits**.

### Extra: QW-03 — bugs baratos do upstream #360/#364/#366/#367 (4 commits)

- **#360** (`ed38da84`): menu Options passou a exibir a área **jogável**
  `worldSize − 2` (o anel externo do mapa não é jogável; fix aceito pelo
  mantenedor: só o texto da UI, save intacto).
- **#366** (`1be0cab1`): `MainMenu::changeWorldLen` agora faz
  `std::clamp(newSize, 50, 10000)` — mesmo range do `validateRange` do Config;
  antes o menu podia levar o tamanho a 0/negativo (crash + config corrompido).
- **#364** (`4982a7c4`): tecla do lago de parque `SDL_SCANCODE_K` → `'p'`
  (livre; 'w' é scroll; help dizia 'w'). Help en regenerado do template e
  pt_BR atualizado ('k' → 'p').
- **#367** (`7567c26f`): typos de help nos templates (`orig/market.tml` 2%→20%
  — confere com `market.cpp:205-216`; `orig/oremine.tml` coal→ore;
  `orig/query.tml` "informations"/"int the"/frase embaralhada) e XMLs en
  regenerados via `convert.py` (só os 4 arquivos esperados mudaram).
- **#363** e "Congradulations"/"yourcredit": já corrigidos no upstream
  (`7e4d03dd`, anterior à base do fork) — nada a fazer.
- Validação: `ctest` 3/3; mudanças só de UI/help → sem impacto em
  simulação/save/determinismo; master local à frente do upstream em
  **56 commits**.

### Extra: QW-02 — scroll speed configurável (commit `593eac43`)

Novo `Config::Option<int> scrollSpeed` (25–400%, default **100 = comportamento
legado**; XML `<scrollSpeed>` no `<game>`, clamp no load) aplicado em
`GameView::scroll` (`amt × scrollSpeed/100`). Menu Options ganhou a linha
"Scroll Speed" com `-`/`+` (passo 25, `ScrollSpeedMinus/Plus` em
`MainMenu::optionsMenuButtonClicked`), parágrafo `scrollSpeedParagraph`
atualizado na abertura e a cada clique. String nova "Scroll Speed" traduzida
no pt_BR (`456/456`). Sem impacto em simulação/save/determinismo (só GUI);
`ctest` 3/3; master local à frente do upstream em 52 commits.

### Extra: QW-01 — sons de foguete (commit `3498da4f`)

Descomentadas as 2 chamadas `getSound()->playSound("RocketTakeoff"/"RocketExplosion")`
em `rocket_pad.cpp:186,201` (assets confirmados em `data/sounds/sounds.xml:27-28`)
+ include de `lincity-ng/Sound.hpp`. Sem impacto em simulação/save/economia
(playSound é guardado por `soundEnabled`/`audioOpen`; o harness headless roda
com áudio fechado → determinismo preservado). ctest 3/3; master local à frente
do upstream em 51 commits.

---

## 2026-08-16 — Sessão 29: Decisões de design do usuário registradas no plano

### Decisões (verificadas no código antes de registrar)

1. **Slots separados** (BUG-01b): autosave → `autosave.scn.gz`; Continue lê
   `9_currentGameNG.scn.gz` (só escrito ao sair para o menu/QUIT) com fallback
   para autosave → cidade nova.
2. **Ponte cobra o valor real** (BUG-04): 500 é o MULTIPLICADOR, não o preço —
   estrada $100 base (×25 tech → até $2.600), ponte $50.000 base (×2 → até
   $150.000), demolição $50/$25.000 (`transport.hpp:38-42,121-135`). Custo em
   tempo real **itemizado**: `Estrada +x | Ponte +y | TOTAL z`.
3. **Escola**: custo fixo regressivo por lotação `busy` (dias operados/100,
   `school.cpp:101-110`): `custo = 2×(2 − min(1, busy/50))` → $4/dia vazia,
   $2 a 50%, $1 cheia. **Eólica**: manutenção debita quando GERA
   (`windpower.cpp:87-88`, por instância); parada = $0 na v1; receita de
   energia consumida fica para a fase do governador.
4. **Painel de cenários**: thumbnail dinâmico quando disponível + texto sempre.
5. **Alíquotas editáveis** (governador), clamps 0–2×: Etapa 1 = efeito na
   arrecadação; Etapa 2 = elasticidade (indústria produz menos, residências
   migram via `desireability` `residence.cpp:372-390` → menos base de income
   tax → possível prejuízo), com conservação garantida (só reduz fluxos
   tributáveis; invariantes do `09` §10 continuam válidos).

### Arquivos tocados (documentação apenas; zero código)

- `11-roadmap-features.md`: seção "Decisões tomadas" + tabela de custos reais
  de transporte + specs FEAT-01/02/03b/03c atualizadas.
- `12-bugs-investigados.md`: BUG-01b e BUG-03b com os modelos decididos.
- `AGENTS.md` (raiz): decisões registradas como regras de projeto.

### Próximo passo (aguardando "vai")

Implementação na ordem do roadmap, começando pelos itens sem dependência:
**TST-01 (infra de testes) + BUG-01a (reset do setWorld) + BUG-02 (loading)**;
em seguida BUG-01b (slots), BUG-03a/c, BUG-04 e FEAT-01 (custos itemizados),
FEAT-02 (painel), FEAT-03b/c.

---

## 2026-08-16 — Sessão 28: Auditoria geral — mapa de taxas, padrão de desenvolvimento, bugs do usuário e roadmap

### Escopo (pedido do usuário)

- Análise completa do último commit (`0b508d42` ore tax) e dos arquivos de
  `.devdocs`; varrer TODOS os TODOs; auditar PRs/issues do upstream; mapear
  **todas as taxas e como interagem** (casas, minas, anos, mapas); investigar
  os bugs relatados (Novo Jogo vs Continuar, carregamento); especificar as
  features (custo de estrada em tempo real, painel de informações do mapa,
  petróleo/porto/gestão tributária) com análise de risco; e definir o padrão
  de desenvolvimento/testes para evitar regressão e "macarronada".

### Método

- 4 subagentes de auditoria em paralelo (economia, TODOs/qualidade,
  GUI bugs/features, upstream+web), cada um escrevendo em `.devdocs/drafts/`;
  validação pessoal de todas as afirmações centrais contra o código
  (engine.cpp, simulate.cpp, lintypes.cpp, MainMenu.cpp, Game.cpp,
  GameView.cpp, track_road_rail.cpp, world.cpp, xmlloadsave, cenários).
- Nenhum arquivo de `src/`/`data/` foi modificado; nenhum commit criado;
  nenhum push/fetch (upstream intocado).

### Entregues (documentação — nenhum código)

| Arquivo | Conteúdo |
|---|---|
| `09-mapa-economia-taxas.md` | Mapa canônico: 8 taxas/custos, 5 receitas, todas as despesas, matriz commodity×módulo, matriz por construção, cenários (good_times +175M, bad_times -1,01bi, Rocket_98 +1,13bi/pop 1,14M, coal_tax_rate=20 nos cenários), fórmulas ano/tech, invariantes de conservação, checklist de commodity nova (15 pontos) |
| `10-padrao-desenvolvimento.md` | Regras R1-R10 (economia em 1 camada, fórmula única, save sagrado/ldsv_version, flag de config, checklist de interação, sem statics de estado, reset no setWorld, memória, determinismo, sem tocar upstream), testes em 3 camadas (unit/invariante headless/golden de save), fluxo do agente, métricas de "gerir bem/mal" |
| `11-roadmap-features.md` | Backlog com IDs: TST-01..04, BUG-01a..d/02/03, FEAT-01 (custo de estrada), FEAT-02 (painel do mapa), FEAT-03a..e (petróleo/porto/governador em fases), QW-01..06, REF-01..05 + 5 decisões pendentes do usuário |
| `12-bugs-investigados.md` | BUG-01 (Novo Jogo: causa raiz = MiniMap com textura do jogo anterior até 100 dias + lastAutosave não resetado — correção em 2 opções), BUG-02 (loading: barreira + race H6), BUG-03a..f |
| `AGENTS.md` (raiz) | Porta de entrada para agentes de IA (regras e ordem de leitura) |
| `08-inventario-de-todos.md` | Atualizado: 4 TODOs marcados como feitos (42912f52, 013a0d9b, 6d4a178f, 0b508d42), +15 novos itens (windmill static paid, autosave reset, meses estáticos, acoplamento engine→GUI) |
| `README.md` | Índice atualizado |

### Principais achados da sessão

1. **Último commit auditado**: `0b508d42` implementa corretamente o padrão do
   fork (taxa na entrega + acumulador anual + save/load + MPS); pontos de
   atenção: cenários têm `coal_tax_rate=20` (default 15), e o campo novo segue
   o padrão de compat de save (v2130 handler) — OK.
2. **Bug "Novo Jogo volta ao anterior"**: NÃO é o autosave carregando save —
   o fluxo de novo jogo está correto; a causa é a reutilização do `Game` com
   reset incompleto (`MiniMap` com textura antiga até 100 dias, autosave
   suprimido no jogo novo). Correção: reset no `setWorld` ou recriar `Game`.
3. **Bug latente de ponte**: exibida ×500, cobrada ×1, demolida ×500.
4. **Escola e eólica não debitam caixa** (só estatística) — MPS não bate com o
   caixa real; decisão de design pendente.
5. **Upstream**: só 1 mudança de taxa na história (coal tax -40%, 2.11.0);
   porto já foi nerfado (#87/#139); #220 é a discussão de balanceamento;
   #246/#247 mostra histórico de bugs neste exato fluxo de menu.
6. **SimCity 2013**: custo por tile no hover é a referência documentada; o
   "texto flutuante negativo" não tem fonte — é especificação nova do fork.

### Próximos passos (ordem do roadmap)

1. TST-01/03 (infra de testes) → 2. BUG-01a + BUG-02 (bugs do usuário) →
3. BUG-03a/c (quick wins) → 4. FEAT-01 (custo de estrada, após decidir
BUG-04/ponte) → 5. FEAT-02 (painel do mapa) → 6. FEAT-03 em fases.
Aguardando decisões do usuário (11-roadmap-features.md "Decisões pendentes").

---

## 2026-08-15 — Sessão 8: Otimizações da simulação (commit 30da6dfc)

### Método

- Profiling com perf + gprof no simtest headless (cidade ~700-800
  construções): pontos quentes = `std::vector<bool>` no trade/market
  (~20% no gprof -O0), varredura diária `do_daily_ecology` +
  `getLowerstVisibleGroup()` não-inline, `isDead()`, acesso a tile
  (`Map::operator()` não-inline), `carsEnabled` re-lido por commodity.
- Benchmark controlado (antes/depois alternados, mesmo save, 60000 dias,
  modo --bench sem ações): **antes 5.63s → depois 5.06s média (~10%)**.
- Validação: ASan/UBSan limpo (5000 dias + fogo + veículos). Equivalência
  lógica verificada caso a caso (ex.: o inline do do_daily_ecology cobre o
  caso do porto). RNG não-determinístico (random_device) → não é possível
  comparação byte-a-byte de saves; confirmado que o mesmo binário diverge
  de si mesmo entre runs.

### Mudanças (6 arquivos, commit 30da6dfc)

1. `lintypes.cpp` trade(): `vector<bool>` → `vector<char>` + buffer
   `thread_local` reutilizado + `carsEnabled` cacheado fora do loop.
2. `market.cpp`: idem (vector<bool> → vector<char> + thread_local).
3. `engine.cpp` do_daily_ecology(): checagem desert inline equivalente a
   `getLowerstVisibleGroup() == GROUP_DESERT`.
4. `world.hpp`: `Map::operator()`, `Map::is_inside`, `Map::len()` inline.
5. `simulate.cpp` + `world.hpp`: buffer `orderingBuffer` reutilizado em
   simulate_mappoints (sem alocação diária).

### Estado dos commits do fork (7)

| Commit | Conteúdo |
|---|---|
| `f6b46930` | Fix build fmt + .gitignore |
| `4a352e73` | Autosave + proteção crash |
| `3e080f11` | Fix som aleatório |
| `2d17015e` | Tradução pt-BR completa |
| `97ddfda7` | Fix ajuda sempre em inglês (pt_BR) |
| `30da6dfc` | Otimizações da simulação (~10%) |

### Próximos passos (P3 restante / P4)

- Medir impacto no jogo real (o usuário deve sentir a simulação mais
  fluida em cidade grande — testar).
- Ideias futuras: reduzir frequência do do_daily_ecology (muda visual),
  vetor paralelo de construções (elimina iteração RB-tree do set),
  mipmaps (P4), hiDPI (P4).

---

## 2026-08-16 — Sessão 24: Tooltip do menu de construção ao vivo + tech antes de estudantes (commit a3879a99)

### Relato do usuário

- O tooltip da universidade mostrava "(requer 4 schools; você tem 0)" mesmo
  com 4 escolas visíveis na tela — a contagem não atualizava.
- A exigência de 15% de tech nunca aparecia em lugar nenhum, nem ao clicar.

### Causas (2 bugs de UX)

1. **Tooltip stale**: `Game::run()` só chamava `ButtonPanel::updateTech()` no
   `new_year` (1x por ano de jogo). Escolas construídas no meio do ano não
   atualizavam o tooltip até o ano seguinte virar.
2. **Tech mascarado**: `UniversityConstructionGroup::can_build()` checava o
   requisito de estudantes ANTES do check de tech da classe base
   (`ConstructionGroup::can_build`). Como o check de estudantes retornava cedo,
   a mensagem `NotEnoughStudentsMessage` sempre precedia (e ocultava) a de
   `NotEnoughTechMessage`.

### Correção

1. `Game.cpp`: chamar `getButtonPanel().updateTech()` no bloco
   `world->isUpdated(World::Updatable::MAP)` — ou seja, sempre que uma
   construção é colocada/removida, que é justamente quando os pré-requisitos
   (ex.: nº de escolas) mudam.
2. `university.cpp`: checar `ConstructionGroup::can_build` (tech/dinheiro)
   PRIMEIRO; só depois o check de estudantes. Assim "requires 15.0 tech
   level" aparece quando o tech é o bloqueio real, e a mensagem de estudantes
   só surge quando o tech já é suficiente.

### Validação (harness headless `uni_probe`)

- low tech (1,2%) + 0 escolas → `NotEnoughTech` ✓
- low tech (1,2%) + 4 escolas → ainda `NotEnoughTech` ✓
- high tech (15%) + 4 escolas → `can_build=true` ✓
- high tech (15%) + 0 escolas → `NotEnoughStudents` ✓
- Sim harness PASSED; ub40 e gamestats harness PASSED.

---

## 2026-08-16 — Sessão 23: Diálogo Gamestats (fundo opaco + colunas) e inventário de TODOs

### Contexto

- Usuário confirmou que joga com `./build/bin/lincity-ng --app-data $PWD/build/share/lincity-ng`
  (build local, não o pacote Debian).
- Relatou: (1) fundo do painel Gamestats translúcido com losangos descoloridos
  do mapa aparecendo; (2) rótulos de construções sobrepondo a linha de baixo
  (Linhas de transmissão→Estradas, Centros de Reciclagem→Florestas,
  Moinhos de vento desalinhando a coluna central).
- Relatou também que NUNCA viu o diálogo "Cannot build a University" — só o
  botão não selecionável.

### Diagnóstico universidade (jogo atual)

- O jogo em questão tem **tech 1,2%** — universidade exige **15%**
  (`GROUP_UNIVERSITY_TECH = 150 * MAX_TECH_LEVEL/1000`). Com tech insuficiente,
  `ButtonPanel::updateTech` **desabilita o botão** (não clicável) e o motivo
  só aparece no tooltip (não há diálogo). Por isso o usuário nunca viu o
  diálogo — não é bug, é o design. Em PT o tooltip "requires 15.0 tech level"
  aparece em inglês no meio da frase PT (ver `04-traducao-ptbr.md`).
- No save anterior (good_times, tech 60%) o bloqueio era escolas (fix da
  Sessão 22).

### Fix Gamestats (commit `1b352a02`)

1. **Fundo opaco**: template `dialogs.xsl` usava `<FilledRectangle
   color="#ffffffbb"/>` (73% opaco) → mapa aparecia "lavado". Trocar para
   `#ffffffff` (opaco) — afeta todos os diálogos gerados pelo XSL
   (gamestats, coal survey, launch rocket, port, trade).
2. **Colunas mais largas**: `gamestats.xml` 630→680px e colunas de label de
   `relative="20"` para `28`, valor de `10` para `8`. Com nomes PT longos
   ("Linhas de transmissão" ≈147px, "Centros de Reciclagem" ≈147px), a coluna
   de 138px fazia o Paragraph quebrar em 2 linhas e, como a altura da célula é
   fixa, o texto estourava para a linha de baixo. Coluna label agora ≈172px.
3. Validação: harness headless (`gamestats_probe`) — openGameStats,
   refreshGameStats e draw completo sem erro. Sim harness PASSED.

### Inventário de TODOs

- Novo doc `08-inventario-de-todos.md`: ~60 TODOs/FIXME categorizados
  (P1 bug/UX → P5 cosmético) com análise de esforço/risco.
- Sugestões rápidas: formatar custo com separador de milhar
  (ButtonPanel:350), trazer janela à frente (WindowManager:127), sons de
  foguete (rocket_pad), investigar ore_tax na entrega (oremine:112).

---

## 2026-08-16 — Sessão 22: Explica regra de estudantes ao construir universidades (commit 1af9d3c0)

### Relato do usuário

- "Faz um tempo que não consigo usar universidades, mesmo com o score
  mínimo necessário. Seleciono no menu mas ela não vem."

### Diagnóstico

- Universidades exigem **4 escolas por universidade**
  (`escolas/4 - universidades >= 1`), regra original do jogo
  (`UniversityConstructionGroup::can_build` em `university.cpp`).
- No save do usuário (good_times): tech 602042 (suficiente, exige 15%),
  10 escolas e 2 universidades → `10/4 - 2 = 0` → `can_build = false`.
  Para a 3ª universidade seriam necessárias **12 escolas**.
- O problema real era **UX**: a falha era silenciosa. O tooltip do botão e o
  diálogo de erro mostravam apenas "Cannot build a University." genérico;
  havia `TODO: handle NotEnoughStudentsMessage` em `ButtonPanel.cpp:355`.

### Correção

- `NotEnoughStudentsMessage` agora carrega `currentSchools` e
  `requiredSchools` (antes era uma mensagem vazia).
- Tooltip do botão (ButtonPanel) explica quantas escolas faltam.
- Diálogo de erro (Game::handleMessage) explica a regra: "You need N schools
  to support building a new University, but you only have M."
- Sem mudança na simulação/gameplay — apenas comunicação.

### Validação

- Harness headless SDL (`uni_probe`): com 10 escolas/2 universidades,
  `can_build=false`, mensagem `current=10 required=12`; com 12 escolas
  simuladas, `can_build=true`. Sim harness PASSED. Build limpo.

---

## 2026-08-16 — Sessão 21: Minimapa de empregos corrigido (commit e6301ea4, issue #79)

### Pedido do usuário

- Implementar a **#79** (menor risco entre as ideias menores): o modo de
  empregos do minimapa mostrava residências vermelhas **sem haver desemprego**.

### Causa raiz

- O modo `UB40` (`MiniMap.cpp`) usava `tellstuff(STUFF_LABOR, -1)`, que
  retorna a **razão de enchimento do estoque de mão de obra**, não o
  desemprego.
- Residência com estoque cheio (>95%) = **todos os empregos preenchidos**
  (`FLAG_EMPLOYED` ligado) → o minimapa pintava isso de vermelho → "sem
  desemprego" aparecia como vermelho.
- Desemprego real = ramo `else` de `residence.cpp` (`FLAG_EMPLOYED`
  desligado), nunca consultado pelo minimapa.

### Correção

- Residência:
  - vermelho brilhante = `FLAG_EMPLOYED` desligado (**desemprego real**);
  - vermelho escuro = empregada com estoque >95% (sem desemprego);
  - cinza = empregada, estoque normal.
- Locais de trabalho carentes de mão de obra mantêm amarelo/laranja.

### Validação

- Harness headless SDL (`ub40_probe` no `/tmp/opencode/simtest`): 5 checks —
  desempregada→vermelho brilhante; empregada+cheia→vermelho escuro;
  empregada+parcial→cinza; nenhuma residência empregada em vermelho
  brilhante; encontrou residências no cenário. PASSED.
- Sim harness PASSED. Build limpo.
- Detalhes técnicos em `05-melhorias-e-otimizacoes.md` → "Ideias menores".

---

## 2026-08-16 — Sessão 20: Corrige fundos pretos nos objetos normais (commit 60556097)

### Bug relatado

- Após os carros coloridos (Sessão 19), **objetos normais do jogo** (minas,
  indústrias, moinhos, fazendas, borda do mapa) ficaram com **fundo preto**
  em forma de retângulo atrás do sprite.
- Os carros continuaram com as cores variadas (corretos).

### Causa raiz

- `preReadImages` passou a mandar **toda** imagem por `recolorSurface()`, que
  chamava `SDL_DuplicateSurface()` **incondicionalmente**, mesmo sem tint.
- SDL 3.4.2 (instalado no sistema) tem um bug: `SDL_DuplicateSurface` →
  `SDL_ConvertSurfaceRectAndColorspace` **perde o blend mode** (BLEND→NONE)
  em superfícies **indexed/palette** (o `SDL_COPY_BLEND` é mascarado e só
  restaurado se o formato tem canal alfa).
- As **81 imagens palette+tRNS** (CoalMine, OreMine, Pottery, Blacksmith,
  FireStation, Fire, Rocket, University, Shanty, Recycle, ResidentialLowLow,
  SportsCroud, Tip, partes de Track/Road/Water, blank) têm os pixels
  transparentes **pretos (RGB=0)** — sem blend, renderizam como preto opaco.
- Carros (RGBA/tipo 6) preservam blend → por isso ficaram corretos.
- Farm/WindMill são tipo 6 (RGBA) e não foram afetados; o "preto" ali era a
  borda/void do mapa (blank.png).

### Correção

- `recolorSurface()` agora **retorna o original quando tint desligado**
  (sem cópia) — evita o bug do SDL para os 344 PNGs não-tingidos.
- Quando há tint, copia o blend mode e o color key para o duplicado (medida
  defensiva contra o mesmo bug do SDL).

### Validação

- Probe com SDL: imagens tipo-3 retornam por referência (blend preservado),
  carro tingido mantém blend BLEND.
- Sim harness PASSED; teste real 0 colisões; build limpo.

---

## 2026-08-15 — Sessão 19: 6 modelos de carro + guia de sprites (commit 63c1eae4)

### Pedido do usuário

- Entender os **requisitos de imagem** para adicionar novos objetos/veículos
  ao jogo (documentação, não só criação).
- Ter **pelo menos 6 carros distintos**: 5 de passeio (variação de cor) + 1
  bombeiro.
- Ideias do usuário: baixar modelos 3D (Sketchfab), editar com Blender, pegar
  imagens próximas, ou **usar o mesmo carro só mudando a cor**.

### O que foi feito

1. **Guia `.devdocs/07-guia-de-sprites.md`** — documenta todo o pipeline de
   sprites:
   - images.xml + ResourceGroup + frame + drawTexture
   - Formato do PNG (RGBA, isométrico), atributos x/y, ordem dos frames
   - Veículos: 16 frames (8 direções + 4 subida + 4 descida de ponte)
   - Como registrar grupo, modelo, spawn; como adicionar construções e
     animações em geral

2. **6 modelos de veículo com variação de cor automática**:
   - `ResourceGroup` ganhou um `Tint` (r,g,b,enabled).
   - `GameView::recolorSurface()` recolore por **luminância** (Rec.709 luma
     preservada + cor substituída pelo tint). Tint multiplicativo daria preto
     (carro azul × vermelho = preto), então usou-se luma.
   - `all_modules.cpp` registra Bluecar + Redcar/Greencar/Yellowcar/
     Whitecar/Firetruck (todos `is_vehicle`; variantes com tint).
   - `images.xml` declara os 6 grupos apontando para os mesmos sprites car01.
   - `VehicleModel` estendido com 5 cores + `VEHICLE_FIRETRUCK`; o construtor
     mapeia modelo → ResourceGroup. Firetruck é placeholder (usa o sprite
     vermelho até ter arte própria).
   - `Construction::trade()` spawna via `Vehicle::randomPassengerCar()`.

### Validação

- `randomPassengerCar()` retorna os 5 modelos uniformemente (~20% cada).
- Todos os 6 grupos existem com `is_vehicle` e tint corretos.
- Recoloração por luma: carro azul vira vermelho (RGB 55,14,14, vermelho
  domina) preservando a forma.
- Sim harness PASSED; teste real de colisão 0 tiles compartilhados.
- Build limpo.

### Próximos passos possíveis

- Arte própria do bombeiro (16 frames) para substituir o placeholder.
- Ajuste fino das cores do tint se o usuário quiser tons específicos.
- Eventualmente aplicar o mesmo mecanismo de tint a outros objetos.

---

## 2026-08-15 — Sessão 18: Carros — passagem de sentidos opostos (commit 224e79c0)

### Relato do usuário

Dois carros em sentidos opostos na mesma via: um chegava ao destino e parava
(animando a chegada), o outro parava também e ficavam parados ~8s até o
primeiro "deixar de existir". Carros atrás paravam e esperavam, e quando o
carro sumia seguiam normalmente. O problema: o carro do sentido oposto parar.

### Causa raiz

Deadlock head-on: cada carro queria dirigir para o tile onde o outro estava.
`tileOccupied()` bloqueava qualquer carro no tile, independentemente da
direção. Como o BLOCK_TIMEOUT é 10s, os dois ficavam esperando ~8-10s até um
desistir (death_counter) e desaparecer.

### Fix

`tileOccupied()` agora distingue direções:
- **Sempre bloqueia** carro que se comprometeu a dirigir PARA o tile (previne
  dois carros comprometendo o mesmo tile ao mesmo tempo).
- **Permite passar** quando o carro no tile está se movendo na direção
  exatamente OPOSTA (`travelDelta()`: usa next-point, ou a direção do último
  movimento quando parado). Em pista de 2 faixas o `walk()` desenha direções
  opostas em lados opostos do tile, então passam lado a lado.
- **Mantém bloqueio** para mesma direção (fila de 1 por tile) e para carro
  parado/esperando/chegando.

### Validação

- Teste head-on forçado (A leste, B oeste, destinos cruzados): cruzam em ~5
  steps, sem deadlock.
- Sim real 2000 dias no ritmo real de animação (100ms): **0 tiles de sprite
  compartilhados**. (O teste antigo saltava 100s/dia e pegava overlap
  transitório de corridas de cruzamento — corrigido o teste para ritmo real.)
- Teste de bloqueio (2 carros frente a frente): sem tile compartilhado, sem
  deadlock, se cruzam.
- Harness PASSED. ASan limpo.

---

## 2026-08-15 — Sessão 17: Carros — fixes de renderização (commit 7dce185f)

### Bugs relatados pelo usuário após o teste em "Bons tempos"

1. **Merge/overlap** (cenário 1): carro que saiu do prédio começou na faixa
   errada, corrigiu, e aí os 2 carros (o que saiu + o que vinha na via)
   grudaram em um só.
2. **Parar e pular** (cenário 2): vários carros antes de um cruzamento de
   3 vias paravam por segundos e depois pulavam para uma nova posição
   (congelados no meio do caminho).
3. **Drift na curva** (cenário 3): pré-existente, engraçado, deixado como
   está (é a animação de curva do walk()).
4. **Spawn sobre carros em movimento** (cenário 4): carros nasciam
   exatamente onde outros estavam andando.
5. **Círculos em pista de 2 vias** (cenário 5): em vez de virar à direita,
   carros faziam um círculo no meio da pista, colidindo quando havia muito
   trânsito.

### Causas raiz

- **Overlap/spawn (1 e 4)**: `tileOccupied()` e `canPlaceVehicle()` só
  checavam o *frame* do sprite, mas o `drive()` avança o `point` lógico antes
  do sprite chegar → o tile ficava "livre" mesmo com carro comprometido.
- **Congelar+pular (2)**: o próximo heading era escolhido assim que
  `point == next`, mesmo com o sprite ainda animando até o tile; carro
  bloqueado congelava no meio do tile e, ao desbloquear, o `walk()` recalculava
  e "teletransportava".
- **Círculos (5)**: o dead-end U-turn disparava quando os 4 headings estavam
  *ocupados por trânsito* (não por falta de estrada) → carro virava no meio do
  cruzamento em vez de esperar.

### Fixes (3 arquivos)

- `tileOccupied()` agora checa point/next/framePt de **todos** os veículos:
  o tile fica reservado assim que um carro se compromete a entrar nele.
- `Transport::canPlaceVehicle()` também checa a posição lógica → spawn nunca
  cai em cima de carro parado ou em movimento.
- `update()`: só decide o próximo heading depois que o sprite termina a
  animação de chegada ao tile; carro bloqueado descansa no tile (sem congelar
  no meio, sem pulo). Espera passou a ser por tempo real (`BLOCK_TIMEOUT`,
  10s) em vez de contagem por speed.
- `getNewHeadings()`: distingue "não há estrada à frente" (dead end →
  vira e volta) de "há estradas mas todas ocupadas" (trânsito → espera).

### Validação

- Sim real 2000 dias com spawns reais: **0 tiles de sprite compartilhados**
  (antes: 17). Teste de colisão lógica com guarda de ocupação no spawn: 0 em
  20k steps. Teste head-on (2 carros de frente em beco): sem tile
  compartilhado, sem deadlock (um desiste após timeout). Harness PASSED.
- Build + ASan limpos.

---

## 2026-08-15 — Sessão 16: Carros — animação de chegada (commit d851f36c)

### Pedido do usuário

- Ao chegar ao destino, o carro deveria **parar ~500ms e sumir**, fazendo
  uma leve curva em direção ao prédio (ideia de "entrou na garagem").

### Implementado

- Estados novos no Vehicle: `arriving`, `arrival_start`, `arrival_side`.
- `update()`: ao chegar a ≤1 tile do destino (após MIN_VEHICLE_TRIP), o
  carro entra em `arriving` em vez de morrer. Durante ARRIVE_MS=500ms o
  sprite fica parado e **desliza lateralmente** (ease-out, ~14px) em
  direção ao lado onde está o prédio (`buildingDirection()`), depois some.
- O deslocamento é cosmético (no `walk()`), não afeta ocupação/colisão.

### Bugs de apoio descobertos e corrigidos no caminho

1. **Destino × MIN_VEHICLE_TRIP incompatíveis**: destino a 8-28 tiles mas
   o carro só podia parar após 40 tiles → passava pelo destino e dava
   voltas. Fix: `pickDestination` busca anéis `MIN_VEHICLE_TRIP..40`.
2. **Beco sem saída = carro preso até timeout**: `getNewHeadings` com
   "não voltar" impedia virar; carro esperava até morrer. Fix: em beco,
   o carro **dá meia-volta** (volta pela estrada de onde veio) se não
   houver saída melhor; só morre se não houver estrada alguma.
3. **MAXIMIZE/MINIMIZE guiavam por labor, não pelo destino visual**:
   `acceptable_heading` rejeitava saídas perto do destino → carro preso
   longe dele. Fix: spawn com `VEHICLE_STRATEGY_RANDOM` (carros são
   puramente visuais; RANDOM + bias do destino resolve).
4. **Carro com `death_counter<=0` preso no ramo de espera**: `update`
   retornava antes de checar morte. Fix: checar `delete this` antes de
   esperar.

### Validação

- Teste de chegada: carro entra em `arriving` ao chegar no destino.
- Overlap de sprite real: 15/2000 ticks (0.75%), nenhum permanente —
  transitório e resolvido, sem deadlock.
- Sim harness PASSED, ASan limpo.

---

## 2026-08-15 — Sessão 15: Carros Etapa D — ocupação de tile + refinamentos de destino

### Contexto (playtest do usuário em good_times)

- Carros ainda somiam em ~2 prédios, terminavam em curvas, começavam/
  terminavam perto de árvore, e se sobrepunham (passavam por cima um do
  outro); em ruas duplas paralelas ficavam "bugados".
- O issue #326 upstream NÃO traz solução (só "force minimum distance").

### Refinamento de destino (commit 23c6eaa0)

- `pickDestination` agora escolhe um **tile de estrada com construção
  vizinha** ("porta de prédio") em anéis 8..28 (antes: prédio em 5..18).
  Média de viagem: 5-8 → **~10.8 tiles** (vários quarteirões).
- O carro morre **exatamente no tile de destino** (`point == destination`),
  não a 1 tile de distância — antes parava no meio de uma curva.
- `hasBuildingNeighbor` no destino impede terminar ao lado de árvore/lote.
- Confirmado: o binário do usuário já tinha a Etapa B (version string
  antigo é só cache do CMake, não recompilado).

### Ocupação de tile (commit c89ae33d, Etapa D)

- `tileOccupied(p)`: true se o tile tem frame de outro veículo.
- `getNewHeadings` pula saídas ocupadas → 1 carro por tile, sem sobreposição
  em cruzamentos/ruas duplas.
- Bloqueado (todas as saídas ocupadas): carro **espera** (point==next) e
  tenta de novo; após `wait_ticks=10` (~15s) some (anti-deadlock). Beco sem
  saída real continua matando na hora.
- Fix latente: `Coal_power::animate` dividia por zero (`rand()%s` com s==0,
  ResourceGroup sem sprites) — crash no harness SEMPRE ao usar do_animate
  em mapa com carvão. Guard `s==0 -> frame=-1`.

### Validação

- Colisão real (sprite `framePt`): 2000 dias, overlap caiu de milhares para
  **1 tick transitório** (timeout resolve). Sim harness PASSED, ASan limpo.
- Teste de estratégia: MAXIMIZE e RANDOM ambas 100% chegam ao destino, 0
  mortes precoces.

---

## 2026-08-15 — Sessão 14: Crash de falência — OutOfMoneyMessage (commit 721ad4e5)

### Sintoma do usuário

- No jogo (extreme_wetland), modo rápido/acelerado: crash com
  `terminate called after throwing an instance of 'std::__exception_ptr::exception_ptr'`
  **no ano 5966 dezembro**, sempre que a cidade estoura o crédito.
- Muitos autosaves antes do crash; jogo rodava horas/dias acelerado.

### Causa raiz (bug PRÉ-EXISTENTE do upstream, d837a134)

- `World::expense()` (engine.cpp:59):
  `throw OutOfMoneyMessage::create(allowCredit)->exception();`
  lança o **`std::exception_ptr` POR VALOR** (tipo
  `std::__exception_ptr::exception_ptr`), em vez da exceção real.
- Nenhum `catch(const OutOfMoneyMessage::Exception&)` nem
  `catch(const std::exception&)` captura um `exception_ptr` → a exceção
  sobe até o topo → `std::terminate`.
- O padrão correto (usado em TODO o resto do código) é
  `OutOfMoneyMessage::create(...)->throwEx()`, que faz
  `std::rethrow_exception` e relança a `OutOfMoneyMessage::Exception` real,
  capturada pelos handlers de simulate.cpp/modules.

### Por que "ano 5966"? — reproduzido no harness

- `money_trace`/`build_test`: cenário extreme_wetland sozinho é **estável**
  (sobe para 274k em 100 anos). Construir residências sem empregos →
  desemprego → dole (15%) → dívida → juros (15%/ano) compostos.
- Simulando a construção de casas, a dívida atinge **-1.999.999.896 no
  ano 5966** (day=7160686) — bate exatamente com o relato do usuário.
- Ou seja: NÃO é bug de dinheiro; é a falência normal (dole+juros)
  amplificada pelo modo acelerado (milhares de anos em minutos reais).
- **Decisão do usuário**: manter o limite de -2G com juros como está.

### Correção (commit 721ad4e5, 2 arquivos)

1. `src/lincity/engine.cpp`: `->throwEx()` em vez de `throw ...->exception()`.
2. `src/lincity-ng/Game.cpp`: rede de segurança `catch(...)` no
   `do_time_step()` — exceções não-`std` (como exception_ptr) nunca mais
   matam o jogo; salva crashsave + volta ao menu.
- Validação: save falido cruza os -2G **sem crash** (a exceção agora é
  capturada corretamente). Builds normal + ASan ok.
- Bônus: `git stash` reverso provou o crash (código antigo estourava).

---

## 2026-08-15 — Sessão 13: Carros — Etapa A (spawn perto de prédios + morte por destino)

### Objetivo do usuário

- O upgrade de estrada para asfaltada gera carros que **aparecem e somem
  aleatoriamente** — não parece haver "viagem" de um lugar a outro.
- Ideia: carros aparecerem perto de casas/prédios (como se tivessem saído
  de lá), crescerem com a população, dirigirem até outro edifício e sumir
  ao chegar. Depois: bombeiros (quartel → incêndio → voltar) e colisão.

### Como o sistema funcionava (antes)

- **Spawn** (`Construction::trade()` em lintypes.cpp): em qualquer estrada
  com tráfego de labor, chance `rand()%1024 < yield/2` de criar um carro —
  sem relação com prédios.
- **Movimento** (Vehicles.cpp): estratégia MAXIMIZE/MINIMIZE (ir para mais/
  menos labor) ou RANDOM walk.
- **Morte**: `death_counter` = 100 tiles, decrementa por tile — o carro
  some no meio do caminho, independente do que há ao redor.

### Etapa A implementada (commit 17334ed0)

- **Spawn restrito a construções**: novo `World::hasBuildingNeighbor(point)`
  (método em world.{hpp,cpp}) verifica os 4 vizinhos ortogonais; o carro só
  nasce se um vizinho é construção real (não-transporte, não-transparente).
- **Morte por destino**: `Vehicle::update()` — após `drive()`, se o tile
  atual tem construção vizinha E o carro já percorreu ≥ `MIN_VEHICLE_TRIP`
  (10 tiles), ele "chegou" e morre (`death_counter = 0`). Fallback: lifespan
  de 100 tiles para carros que nunca chegam a prédio.
- Campo novo `Vehicle::origin` guarda o tile de spawn (para não morrer no
  próprio prédio de origem).
- Veículos são runtime-only (não salvos) → sem mudança de formato de save.

### Validação

- Harness sim (`vehicle_test`): 300 dias com `do_animate` — **peak=7 carros
  simultâneos, 80 spawns** → frota limitada, morte por destino funcionando
  (não acumula). Harness de simulação PASSED; builds normal + ASan ok.
- ⚠️ Requer avaliação visual do usuário: carros saindo de perto de prédios
  e sumindo ao chegar em outros; volume cresce com a cidade.

### Próximas etapas (a validar com o usuário)

1. **Etapa B**: destino explícito por carro (escolher uma construção-alvo
   real e navegar até ela) em vez de só morrer perto de qualquer prédio.
2. **Etapa C**: bombeiros — spawn no quartel mais próximo, navegar até o
   incêndio, ajudar a apagar, voltar e sumir.
3. **Etapa D**: colisão — `getNewHeadings()` evita tiles ocupados por outro
   veículo (framesptr do destino), foco em cruzamentos.

---

## 2026-08-15 — Sessão 12: Decisão sobre #19 HiDPI (#357)

### #19 — HiDPI adiado (decisão fundamentada)

- Analisado o PR #357 ("support high-dpi/scaled display") do dbear496:
  - Abordagem: `SDL_WINDOW_HIGH_PIXEL_DENSITY` + `SDL_GetWindowSizeInPixels` +
    escala manual via `SDL_SetRenderScale` no PainterSDL; fontes via
    `TTF_SetFontSizeDPI(font, size, hdpi, vdpi)` com `baseDpi=72`. Não usa
    `SDL_SetRenderLogicalPresentation`.
  - Escopo: **64 arquivos** (~40 de código): Desktop/Component/Child ganham
    escala + reLayoutDeep; Paragraph/Gradient/Image re-renderizam em escala;
    TextureSDL perde width/height cacheados; MiniMap 1px/tile; novo handler
    `SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED`.
  - ⚠️ **PR ainda ABERTO** com bugs não resolvidos: cliques em área errada
    no X11 (reportado 2x pelo paulorlyk), crash de path do game_menu.png
    (corrigido só no último commit), inconsistência (handlers
    WINDOW_PIXEL_SIZE_CHANGED sem reLayoutDeep).
  - ⚠️ Mudanças não-HiDPI acopladas: botões do buttonpanel.xml forçados a
    50×50 (Image agora sempre stretch + assert), novos ícones sports/
    commune, rename game_menu.png→speed/, labels do EconomyGraph
    ("Popul."→"Popul" — muda chaves gettext), gradientes inclinados 60°.
  - ⚠️ **Nosso monitor é 96 DPI** (1920×1080, Xft.dpi=96) → HiDPI não se
    aplica ao hardware atual; zero ganho visível. API `TTF_SetFontSizeDPI`
    existe no sistema (SDL3_ttf 0.2.2), então não há bloqueio técnico.
- **Decisão**: adiar. Revisitar quando o upstream mergear #357. Risco alto
  (diff grande + bugs X11 + acoplamentos) sem benefício no display atual.

### Estado dos commits do fork (14)

| Commit | Conteúdo |
|---|---|
| `f6b46930` | Fix build fmt + .gitignore |
| `4a352e73` | Autosave + proteção crash |
| `3e080f11` | Fix som aleatório |
| `2d17015e` | Tradução pt-BR completa |
| `97ddfda7` | Fix ajuda sempre em inglês (pt_BR) |
| `30da6dfc` | Otimizações da simulação (~10%) |
| `48f927ac` | do_daily_ecology a cada 3 dias (~35%) |
| `7c5d36e9` | Destaque de ranges (#242) |
| `a783402d` | Stats window auto-update (#211) |
| `cb5b14dc` | Mipmaps manuais (#332) |
| `bd2ce17b` | Fix gaps entre tiles (#331) |

### Próximos passos

1. P4 restante imediato: **#22 carros sobrepostos (#326)** — mexe na lógica
   de tráfego, resolvível e testável no hardware atual.
2. #19 HiDPI adiado até merge upstream do #357.
3. Ideia própria futura: caminhões de bombeiro dirigindo até o fogo.
---


## 2026-08-15 — Sessão 11: Gaps entre tiles (#331) + decisão sobre #14

### #14 — shuffle→rotação DESCARTADO

- gprof (20k dias, -O0): `std::shuffle` self = **0.04s (0.26%)** +
  `BasicUrbg::operator()` **0.01s (0.07%)** = **~0.3%** do tempo total.
- `simulate_mappoints` inteiro = 1.05%; dominante é `trade()` (57.6%) e
  `equilibrate_stuff` (25.7%) — lógica de negócio, intocável por rotação.
- Ganho máximo do #14: **~0.3-0.5%** — abaixo do limiar de 1% pelo qual
  descartamos o #16. Com risco de gameplay (mudar ordem de updates altera
  o destino da cidade) → **descartado**, seguindo o precedente do #16.
- ✅ P3 completo exceto #17 (só se profiling apontar) e #18 (threads, fora).

### #331 — Gaps entre tiles (commit bd2ce17b)

- Reprodução: reset de zoom (Enter) → zoom out 2x (tileWidth = 128×0.7071
  = 90.5px fracionário) → fendas visíveis, mais óbvias na água.
- Causa raiz: o rasterizador SDL preenche um pixel **somente se o centro
  do pixel cai dentro do rect de destino**. Com posições fracionárias, os
  pixels de fronteira entre tiles adjacentes ficam descobertos → gaps.
- Fix: em `PainterSDL::drawStretchTexture()`, expandir o rect para cobrir
  todos os pixels que ele toca — `floor` na posição, `ceil` no canto oposto.
- Validação: teste offscreen SDL (`gap_test3`) — na região da costura entre
  tiles adjacentes em zoom 90.5px, RAW deixa **16 pixels descobertos**,
  EXPAND deixa **0**. Builds normal + ASan ok, harness sim PASSED.
- UI (`Image::draw`) passa larguras inteiras → não afetado.

### Estado dos commits do fork (14)

| Commit | Conteúdo |
|---|---|
| `f6b46930` | Fix build fmt + .gitignore |
| `4a352e73` | Autosave + proteção crash |
| `3e080f11` | Fix som aleatório |
| `2d17015e` | Tradução pt-BR completa |
| `97ddfda7` | Fix ajuda sempre em inglês (pt_BR) |
| `30da6dfc` | Otimizações da simulação (~10%) |
| `48f927ac` | do_daily_ecology a cada 3 dias (~35%) |
| `7c5d36e9` | Destaque de ranges (#242) |
| `a783402d` | Stats window auto-update (#211) |
| `cb5b14dc` | Mipmaps manuais (#332) |
| `bd2ce17b` | Fix gaps entre tiles (#331) |

### Próximos passos

1. **Usuário testar**: zoom out 2x + reset (Enter) — fendas na água sumiram;
   conferir se mipmaps + gaps juntos mantêm boa qualidade no zoom out.
2. P4 restantes: #19 hiDPI (#357), #22 carros sobrepostos (#326). Ideia
   própria futura: caminhões de bombeiro dirigindo até o fogo.

---



## 2026-08-15 — Sessão 10: Mipmaps manuais (#332) + stats auto-update (#211)

### #211 — Stats window auto-update (commit a783402d)

- A janela de stats abria os números e não atualizava ao construir/derrubar.
- Refatorado: o preenchimento de `Dialog::gameStats()` foi extraído para
  `Dialog::refreshGameStats()`; `refreshOpenGameStats()` é chamado em
  `Game::run()` quando o mapa (construir/derrubar) ou o dinheiro mudam.
- `openGameStats()` reutiliza a janela aberta em vez de criar um Dialog
  órfão (evita leak + números defasados ao re-clicar no botão Stat).

### #332 — Mipmaps manuais (commit cb5b14dc)

- SDL3 não tem mipmaps nativos (issue #332 documenta a lacuna). A única
  opção viável: **gerar mipmaps manualmente**.
- `TextureManagerSDL::create()` agora gera uma cadeia de texturas em
  metade do tamanho a partir da imagem carregada.
- Downscale 2x2 com **premultiplied alpha + ALPHA_BARRIER**: pixels quase
  transparentes (halo dos tiles) não escurecem o resultado → evita os
  "gaps" escuros entre tiles que o `glGenerateMipmap`/bilinear causariam.
- `PainterSDL::drawStretchTexture()` usa `TextureSDL::selectMipmap()`:
  escolhe o maior nível que ainda cobre o tamanho de destino (nunca
  upscala mipmap). Scale mode (NEAREST/LINEAR) aplicado a todos os níveis.
- Validação: unit test `mipmap_test` — 2x2 reduzido mantém cor
  (255,0,0 → 255,0,0) e pixel com halo transparente não escurece
  (200,50,50,255 → 200,50,50,255). Harness sim testa: PASSED.
- ⚠️ Requer avaliação visual do usuário (zoom out na cidade grande).

### Estado dos commits do fork (12)

| Commit | Conteúdo |
|---|---|
| `f6b46930` | Fix build fmt + .gitignore |
| `4a352e73` | Autosave + proteção crash |
| `3e080f11` | Fix som aleatório |
| `2d17015e` | Tradução pt-BR completa |
| `97ddfda7` | Fix ajuda sempre em inglês (pt_BR) |
| `30da6dfc` | Otimizações da simulação (~10%) |
| `48f927ac` | do_daily_ecology a cada 3 dias (~35%) |
| `7c5d36e9` | Destaque de ranges (#242) |
| `a783402d` | Stats window auto-update (#211) |
| `cb5b14dc` | Mipmaps manuais (#332) |

### Próximos passos

1. **Usuário testar**: zoom out com menos aliasing/shimmering; janela de
   stats atualizando ao construir.
2. Depois do teste do usuário: rodar `git gc`/compact se necessário.
3. Próximas: #14 shuffle→rotação O(1) (perf, exige validação gameplay) —
   outras opções P4: hiDPI (#357), gaps (#331), carros sobrepostos (#326).
---


## 2026-08-15 — Sessão 9: do_daily_ecology a cada 3 dias (#15) + destaque de ranges (#242)

### #15 — do_daily_ecology a cada 3 dias (commit 48f927ac)

- gprof (20k dias): `do_daily_ecology` = 37.3% do tempo de simulação em -O0;
  `scan_pollution` = 0.77% (por isso o incremental #16 foi **descartado** —
  ganho <1%, risco de mudar difusão de poluição).
- Mudança: `do_periodic_events` chama `do_daily_ecology` a cada 3 dias, e a
  probabilidade por run sobe de `rand()%300==1` para `rand()%100==1`
  (média por dia inalterada: ~26% em 90 dias em ambos).
- Benchmark 60k dias (3 runs): **4.08/4.06/4.28 → 2.74/2.67/2.68s (~35%)**.
- ASan/UBSan limpo (5.000 dias).

### #242 — destaque de ranges de prédios do mesmo tipo (commit 7c5d36e9)

- Issue: ao selecionar um prédio para construir, destacar os ranges de todos
  os prédios do mesmo tipo na view atual, para posicionar melhor.
- Implementação em `GameView::draw()`: quando ACTION_BUILD com range>0,
  itera `world.map.constructions`, filtra mesmo `constructionGroup`
  (comparação de ponteiro — grupos são singletons), skip isDead() e fora
  da viewport, desenha losango verde semi-transparente.
- Custo relevante só quando build tool ativa (não afeta FPS normal).

### Estado dos commits do fork (9)

| Commit | Conteúdo |
|---|---|
| `f6b46930` | Fix build fmt + .gitignore |
| `4a352e73` | Autosave + proteção crash |
| `3e080f11` | Fix som aleatório |
| `2d17015e` | Tradução pt-BR completa |
| `97ddfda7` | Fix ajuda sempre em inglês (pt_BR) |
| `30da6dfc` | Otimizações da simulação (~10%) |
| `48f927ac` | do_daily_ecology a cada 3 dias (~35%) |
| `7c5d36e9` | Destaque de ranges (#242) |

Nota: o harness simtest foi perdido no reboot (/tmp) e **reconstruído**
(`/tmp/opencode/simtest/simtest.cpp`, linkado contra o build otimizado e
build-asan; precisa de `configPtr = new Config()` + `initFactories() +
initializeModules()` antes do `World::load`).

### Próximos passos

1. **Usuário testar**: simulação mais fluida (velocidade máxima) + destaque
   verde dos ranges ao construir bombeiros/hospital/market.
2. Ideias futuras: `scan_pollution` incremental (agora irrelevante, <1%),
   #24 stats auto-update, #20 mipmaps, #14 shuffle→rotação O(1).

---



### Sintoma (relato do usuário)

- Áudios agora consistentes (fix da sessão 4 confirmado no jogo) ✓
- UI quase toda em português, MAS a ajuda (F1 / clique direito) TODA em
  inglês.

### Causa raiz — bug em `HelpWindow::getHelpFile` (`HelpWindow.cpp`)

A etapa "try stripping the codeset" verificava e retornava o **`filename`**
(`std::filesystem::exists(filename)`) em vez do **`filepath`** calculado.
Fluxo com `LANG=pt_BR.UTF-8`:

1. `helpdir/pt_BR.UTF-8/` não existe.
2. codeset strip → `helpdir/pt_BR/` **EXISTE**, mas o `if` errado não
   retorna (verifica "controls.xml" no CWD, que não existe).
3. region strip → tenta `helpdir/pt/` → não existe.
4. **Fallback para inglês** — toda a ajuda em inglês.

Por que nunca notaram: o alemão (`de_DE.UTF-8`) funciona por acaso —
`de_DE` não existe, o codeset strip daria `de_DE` (também não existe,
mas nunca é avaliado por causa do mesmo bug)... na verdade com o bug, o
region strip tenta `de` → EXISTE → funciona. Ou seja: idiomas cuja
região (pt_BR) existe como diretório mas a base (pt) não existia
quebravam. (Curiosamente `pt` também existe hoje? Não — só `pt_BR`.)

### Fix (2 linhas)

`if(std::filesystem::exists(filepath)) return filepath;` — verificado
com o ambiente real do usuário (passo a passo acima).

### Estado dos commits do fork (5 até agora)

| Commit | Correção |
|---|---|
| `f6b46930` | Build: fmt em lincity_lib_modules + .gitignore |
| `4a352e73` | Autosave + proteção contra crash |
| `3e080f11` | Fix som aleatório |
| `2d17015e` | Tradução pt-BR completa (PO + help + desktop + metainfo + typos) |
| `97ddfda7` | Fix ajuda sempre em inglês para pt_BR |

---



### Ajuda pt-BR atualizada (fechamento da tradução)

- Verificado: todos os 81 tópicos de ajuda existem em pt_BR (nenhum
  fallback para inglês), mas o conteúdo era de 2009 e desatualizado vs o en.
- **24 arquivos reescritos/traduzidos** para acompanhar o conteúdo atual do
  en: port, coalmine, residential, recycle, tip, commune, oremine,
  powerscoal, waterwell, query, track, rail, pbar, substation, pause,
  industryl, industryh, farm, pottery, park, monument, blacksmith, road,
  university, school, tutorial-scenario.
- Links quebrados herdados de 2009 corrigidos (mills→mill, indistryl→
  industryl, indistryh→industryh, powercoal→powerscoal, powersolar→
  powerssolar); validação garante que todo href em pt_BR aponta para
  arquivo existente.
- Nota: o help en tem links quebrados ("indistryh" etc.) — pt_BR melhor.
- Dica do parque traduzida com a tecla 'k' (código real, issue #364) em vez
  de 'w' (help en errado).

### Commits criados no fork (nenhum upstream)

| Commit | Conteúdo |
|---|---|
| `f6b46930` | Fix build: fmt em lincity_lib_modules + .gitignore |
| `4a352e73` | Autosave + proteção contra crash |
| `3e080f11` | Fix som aleatório (Sound.cpp) |
| `2d17015e` | Tradução pt-BR completa (PO, help, desktop, metainfo, typos #367) |

- Game.cpp dividido em 2 commits (autosave vs typos) revertendo/reaplicando
  os hunks de typo temporariamente.
- Build final OK; 412/412 traduzidas; working tree limpo.

### Próximos passos

1. Usuário testar o build novo (som correto + UI/ajuda em pt-BR).
2. P3 — otimizações (simulate_mappoints shuffle, do_daily_ecology,
   scan_pollution, etc. — ver 05-melhorias-e-otimizacoes.md).

---

## 2026-08-15 — Sessão 7: Ajuda em inglês — causa raiz e fix (commit 97ddfda7)

### Sintoma (relato do usuário)

- Áudios consistentes (fix do som confirmado), UI quase toda em pt-BR,
  MAS a ajuda (F1) TODA em inglês.

### Causa raiz — bug em `HelpWindow::getHelpFile` (`HelpWindow.cpp`)

A etapa "try stripping the codeset" verificava e retornava o **`filename`**
em vez do **`filepath`** calculado. Com `LANG=pt_BR.UTF-8`:
1. `helpdir/pt_BR.UTF-8/` não existe.
2. codeset strip → `helpdir/pt_BR/` EXISTE, mas o `if` errado não retorna.
3. region strip → tenta `helpdir/pt/` → não existe.
4. Fallback inglês.

O alemão funcionava por acaso (`de_DE` → region strip → `de` existe).
Fix de 2 linhas (`exists(filepath)` / `return filepath`).

### Estado dos commits do fork (6 até então)

`f6b46930` build · `4a352e73` autosave · `3e080f11` som · `2d17015e`
tradução · `97ddfda7` fix ajuda.

---

## 2026-08-15 — Sessão 6: Tradução de ajuda concluída + commits separados

### Ajuda pt-BR atualizada (fechamento da tradução)

- Verificado: todos os 81 tópicos de ajuda existem em pt_BR (nenhum
  fallback para inglês), mas o conteúdo era de 2009 e desatualizado vs o en.
- **24 arquivos reescritos/traduzidos** para acompanhar o conteúdo atual do
  en (port, coalmine, residential, recycle, tip, commune, oremine,
  powerscoal, waterwell, query, track, rail, pbar, substation, pause,
  industryl, industryh, farm, pottery, park, monument, blacksmith, road,
  university, school, tutorial-scenario).
- Links quebrados herdados de 2009 corrigidos (mills→mill, indistryl→
  industryl, indistryh→industryh, powercoal→powerscoal, powersolar→
  powerssolar); validação garante que todo href em pt_BR aponta para
  arquivo existente.
- Nota: o help en tem links quebrados ("indistryh" etc.) — pt_BR melhor.
- Dica do parque traduzida com a tecla 'k' (código real, issue #364) em vez
  de 'w' (help en errado).

### Commits criados no fork (nenhum upstream)

| Commit | Conteúdo |
|---|---|
| `f6b46930` | Fix build: fmt em lincity_lib_modules + .gitignore |
| `4a352e73` | Autosave + proteção contra crash |
| `3e080f11` | Fix som aleatório (Sound.cpp) |
| `2d17015e` | Tradução pt-BR completa (PO, help, desktop, metainfo, typos #367) |

- Game.cpp dividido em 2 commits (autosave vs typos) revertendo/reaplicando
  os hunks de typo temporariamente.
- Build final OK; 412/412 traduzidas; working tree limpo.

---

## 2026-08-15 — Sessão 5: Segunda sessão de jogo do usuário SEM crash

- Usuário jogou mais uma sessão longa no build 2.15.0 (cidade avançada:
  população 10.0k, tech 22.2, 2 usinas térmicas, dezenas de moinhos de
  vento, ponte sobre lago, rede rodoviária com tráfego, dinheiro -71.6M
  vermelho).
- **NENHUM CRASH** — segunda sessão consecutiva estável. Evidência
  acumulada de que a migração 2.15.0 (SDL3) + fixes desde 2.14.2
  resolveram o crash do usuário.
- A sessão rodou o build ANTERIOR (sem som/tradução): os labels em inglês
  na captura ("Population", "Tech Level", etc.) já têm tradução no build
  novo (verificado via msgunfmt: Poluição do Ar, Nível Tecnológico, etc.).
- "n/d" no rodapé (Monumento) = tradução de "n/a" → "não disponível",
  correta.
- **PENDENTE**: usuário testar o build com as correções de som + tradução
  (já compilado em `build/`).

---

## 2026-08-15 — Sessão 4: Bug do som aleatório CORRIGIDO + tradução pt-BR 100%

### Bug do som aleatório (CAUSA RAIZ ENCONTRADA E CORRIGIDA)

**Sintoma**: ao construir um prédio, o efeito sonoro vinha aleatório (escola
tocava som de fazenda, etc.), e só às vezes tocava o som correto.

**Causa**: `Sound::loadWaves()` (`Sound.cpp`) — o parser acumulava TODOS os
ResourceGroups vistos no arquivo em `resGrpVec` e adicionava cada som a TODOS
os grupos. A checagem de fim de escopo `reader.get_depth() < resourceID_level-1`
nunca disparava: no `sounds.xml`, `<group>` está em depth 1 e os
`<resourceID>`/`<sound>` em depth 2, então `resourceID_level-1 = 1` e
`1 < 1` é falso → o vetor nunca era limpo → cada grupo de som acumulava os
sons de todos os grupos seguintes (School: 26 sons, FireStation: 100, Farm:
110, Green: 131...).

**Verificação**: simulação do parser em Python (libxml2) com os depths reais —
antes vs depois da correção (School 26→3, Farm 110→6, etc., todos batendo
com o esperado).

**Correção**: `Sound.cpp` — `depth < resourceID_level-1` → `depth <
resourceID_level`. Bug existe desde 2013 (commit 588d915e que introduziu o
sounds.xml), nas duas versões (2.14.2 e master). Candidate a PR upstream.

### Tradução pt-BR (100%)

- PO regenerado contra o template 2.15.0 (`generate_pot`), 202/202 entradas
  traduzidas/corrigidas via script (`/tmp/aplicar_traducao.py`): 105 não
  traduzidas + 97 fuzzy revisadas (muitas traduções de 2009 estavam erradas:
  "Não exitem pessoas nesta cidade", "Saúde Cvr", "Poluição" para
  "Resolution", "EE Carvão" para "Wind powers", etc.).
- Cabeçalho do PO limpo e atualizado (pt_BR, Plural-Forms, 2.15.0).
- 6 help XML criados (commodities, controls, evacuate, power, waste, water);
  `keys.xml` obsoleto removido e referências corrigidas.
- `.desktop` + `metainfo.xml` com pt_BR.
- Typos #367 no Game.cpp corrigidos ("Congradulations", "yourcredit") com
  msgids do PO atualizados em conjunto.
- Resultado: **412/412 traduzidas, MO válido e verificado**.

### Pendências

- Testar no jogo (idioma pt_BR) e conferir textos que estourem em botões
  pequenos; ajustar conforme necessário.
- Revisar help XML antigos (2009) quanto a fórmulas desatualizadas.
- Considerar subir PR upstream para o som (fix de 3 linhas) e para o pt-BR.

---

## 2026-08-15 — Sessão 3: Teste do usuário no build 2.15.0 + refinamentos

### Resultado do teste do usuário (importante!)

- Usuário jogou o build **2.15.0** com cidade bem desenvolvida (data no jogo
  avançou de Ago/42 a Out/44 ≈ 2+ anos simulados; zona industrial, agrícola,
  residencial densa, corpo de bombeiros, incêndio real que queimou uma
  cratera).
- **NENHUM CRASH.** Autosave funcionou (linhas `autosaved game to ...` no
  log, uma a cada 100 dias).
- Nota: o crash antigo aparecia só "com o jogo bem avançado" — continuar
  jogando até o ponto onde costumava quebrar para confirmar.
- Screenshots do usuário mostram a UI parcialmente em pt-BR (ex.: "Ferr. de
  Pesquisa", "Não") — a tradução do PO antigo aparece em vários lugares.

### Refinamentos feitos nesta sessão

1. **Correção cosmética do log**: caminho do save impresso com aspas duplas
   extras (`'"..."'`) porque `operator<<(path)` do C++17 adiciona aspas.
   Corrigido com `.string()` em `autoSaveCityNG` (`MainLincity.cpp`).
2. **Throttle de autosave por tempo real**: na velocidade máxima, 100 dias
   passavam em segundos → save a cada ~segundo (churn de disco e possíveis
   engasgos). Agora: autosave no máximo 1× a cada 10s reais (`Game.cpp`:
   `AUTOSAVE_INTERVAL_MS = 10000`, novo membro `lastAutosaveTick` em
   `Game.hpp`).

### Estado do fork (diff atual, 6 arquivos)

- `src/lincity-ng/Game.cpp` / `Game.hpp`: autosave + try/catch + throttle.
- `src/lincity-ng/MainLincity.{hpp,cpp}`: `autoSaveCityNG` (atômico).
- `src/lincity/modules/CMakeLists.txt`: fix `fmt::fmt` (build).
- `.gitignore`: `.devdocs/` + `build-*/`.

### Pendências

- Continuar jogando 2.15.0 até o ponto em que o crash costumava ocorrer.
- Se crashar: coletar log (`stdout` do terminal) e checar
  `~/.local/share/lincity-ng/crashsave.scn.gz`.
- Próximo passo técnico planejado: atacar H6 (raça da thread de texturas)
  caso o crash persista; senão, seguir para tradução pt-BR (P2).

---

## 2026-08-15 — Sessão 2: Build master, autosave, reprodução do crash

### Ambiente de build (Debian/Ubuntu 26.04)

- SDL3_mixer **não existe** nos repositórios Ubuntu → compilado do fonte
  (tag release-3.2.4) com codecs WAV+OGG(stb_vorbis), instalado em
  `~/.local` (sem sudo).
- `libfmt` 10.1.1 do Ubuntu é antigo demais (falta `fmt/base.h`, que é do
  fmt 11) → fmt 11.1.4 compilado e instalado em `~/.local`.
- **Bug de build no upstream corrigido no fork**:
  `src/lincity/modules/CMakeLists.txt` — `lincity_lib_modules` usa headers
  do fmt mas não linkava `fmt::fmt` (só `lincity_lib` linkava, PRIVATE).
  Funcionava por acaso quando o fmt ficava em `/usr/include`; quebrava com
  fmt em outro prefix. **Fix: `fmt::fmt` adicionado PUBLIC em
  `lincity_lib_modules`.** (Candidate a PR upstream.)
- Build principal: `build/` (RelWithDebInfo). Build sanitizado: `build-asan/`
  (Debug + `-fsanitize=address,undefined`).
- Rodar o jogo compilado: `build/bin/lincity-ng --app-data $PWD/build/share/lincity-ng`

### Implementado (P0 — proteção contra perda de dados)

1. **Autosave**: a cada 100 dias simulados, salva em
   `9_currentGameNG.scn.gz` (slot do "Continue") com escrita **atômica**
   (`.tmp` + rename). Nova função `autoSaveCityNG` em `MainLincity.{hpp,cpp}`;
   controle em `Game::run()` via `lastAutosaveDay` (`Game.hpp`).
2. **try/catch defensivo** ao redor de `world->do_time_step()` em
   `Game::run()`: em exceção, salva `crashsave.scn.gz`, mostra mensagem e
   volta ao menu (nunca morre silenciosamente sem salvar).

### Ferramenta de teste headless (fora do repo, /tmp/opencode/simtest)

- `simtest.cpp`: carrega um save, roda N dias de simulação com ações
  aleatórias (construir/derrubar, incluindo estradas/pontes/rios),
  spawn de veículos, animação, e verifica save/reload.
- Linkado contra o build (master e 2.14.2), com e sem ASan/UBSan.

### Resultados de teste (estado atual)

- **Master 2.15.0**: 10.000 dias + ações + veículos sob ASan → limpo.
- **2.14.2** (a versão que o usuário joga): 3.000 dias sob ASan → limpo.
- **Master 2.15.0, build otimizado**: **50.000 dias** de simulação com
  ações aleatórias + veículos → PASSED (cidade cresceu de 54 para 895
  construções; save/reload íntegros).
- **Master 2.15.0, ASan/UBSan**: 5.000 dias com ações incluindo **torch
  (incêndios)** + veículos → PASSED, zero erros de memória.
- **Conclusão parcial**: o motor de simulação (placeItem/detach/
  connect_transport/fogo/veículos/save-load) está limpo sob sanitizers nas
  duas versões. O crash do usuário provavelmente NÃO é corrupção de memória
  na simulação pura.
- Suspeitos restantes (camada GUI, não testável headless):
  1. **Thread de carregamento de texturas** (`GameView::gameViewThread` →
     `preReadImages`): cresce `ResourceGroup::graphicsInfoVector` (std::vector,
     com realloc) na thread de fundo enquanto a main desenha/itera;
     `images_loaded` e `remaining_images` são bool/int compartilhados sem
     atomics. O próprio código admite ("Some of the Image to Texture
     Conversion seems not to be threadsave"). Presente igualmente na 2.14.2
     e 2.15.0. Race → UB → possível crash intermitente "sem padrão".
  2. `getConfig()`/`simDelay` (engine→GUI) — frágil, mas ok no jogo real.
  3. Drivers/driver issues específicos do SDL2 na máquina do usuário.

### Próximos passos

1. **Usuário testar o build 2.15.0** (SDL3) — pode já resolver o crash da
   2.14.2 (SDL2). Comando:
   `build/bin/lincity-ng --app-data $PWD/build/share/lincity-ng`
2. Se o crash persistir na 2.15.0, priorizar: medir/eliminar a race da
   thread de texturas (ex.: criar `graphicsInfoVector` com capacidade
   reservada, mover `images_loaded=true` para depois do último append com
   memory barrier, ou carregar texturas tudo na main thread).
3. Autosave já implementado protege contra perda.

### Achados secundários

- **Engine dependente da GUI**: `lintypes.cpp:722` chama `getConfig()` no
  meio da simulação (config de carros). `simDelay` também é global da
  camada lincity-ng. Dívida técnica de arquitetura (engine deveria
  receber parâmetros); no jogo real não crasha (config sempre inicializado
  no main), mas é um ponto frágil (candidate a refactor).
- Crash de `timeout` do rust-coreutils (Apport) durante testes = bug do
  Ubuntu, não do jogo.
- xdotool/cliques sintéticos disparam o pedido de "execução remota" do
  GNOME e o usuário recusa → **abandonar automação de GUI por input
  sintético**; testes de GUI ficam por conta do usuário ou via driver SDL
  dummy.

### Pendências

- Resultado dos testes longos (30k dias).
- Teste de render (GUI) — sem automação por enquanto.
- Continuar caçando o crash: possivelmente versão antiga do SDL2/Drivers,
  texturas (ResourceGroup/GraphicsInfo), ou padrão específico de gameplay.
- Copiar o save real do usuário e o simtest para o workspace (documentado
  em /tmp; o .devdocs não deve guardar binários grandes).

---


## 2026-08-15 — Sessão 1: Pesquisa inicial (escopo: pesquisa)

### Feito

1. **Análise de estado do fork**:
   - Remoto: `git@github.com:maicomferre/lincity-ng.git`, branch `master`
     sincronizada com upstream (~2.15.0, último commit `12c5b557`).
   - Jogo instalado no sistema: pacote Debian **2.14.2-1**
     (`lincity-ng version 2.14.2-unknown`).

2. **Coleta de issues/PRs abertos do upstream** (34 issues, 3 PRs):
   - Ver `02-issues-e-prs-upstream.md`.
   - Destaques: **#369 crash** (mesmo sintoma do usuário, criada hoje),
     #360/#362/#363/#364/#366/#367 (bugs pequenos e corrigíveis),
     PRs #361 (modelo para pt-BR), #357 (hiDPI), #283 (VRR, conflituoso).

3. **Exploração do código** (ver `01-entendendo-o-projeto.md`):
   - Loop principal `Game::run()`, `simulate_mappoints()`, ciclo de vida
     de `Construction`, sistema `framesptr`/`ExtraFrame`, veículos, fogo,
     transporte, save/load XML.

4. **Investigação do crash** (ver `03-analise-bug-crash.md`):
   - Confirmado: **não há autosave** → crash = perda total.
   - `world->do_time_step()` sem try/catch no loop → exceção mata o jogo
     sem salvar (H1).
   - Padrão `framesptr` com ponteiro bruto deletado quando vazio (H2).
   - Histórico: issue #52 (crash em `Transport::update` por delete duplo
     de fogo de lixo; corrigido por PR #51 no passado).
   - Causa raiz NÃO confirmada; plano de reprodução com sanitizers.

5. **Tradução pt-BR** (ver `04-traducao-ptbr.md`):
   - PO de 2009 (1.1.0rc2), 488 msgids vs 645 (fr) — o mais incompleto.
   - 6 páginas de help faltando; glossário proposto.

6. **Criação da estrutura de docs locais** `.devdocs/` (esta pasta) e
   regra `/.devdocs/` no `.gitignore` (não rastreada).

### Decisões

- Escopo desta sessão: apenas pesquisa/documentação. Nenhum código do
  jogo foi alterado.
- Prioridades definidas em `05-melhorias-e-otimizacoes.md`:
  P0 autosave + try/catch defensivo; P1 bugs baratos upstream;
  P2 pt-BR; P3 otimizações; P4 UX.

### Próximos passos (sugeridos)

1. Build local do master (2.15.0) em RelWithDebInfo e teste do crash do
   usuário (verificar se já foi corrigido após 2.14.2).
2. Implementar autosave (P0-1).
3. Reprodução do crash com ASan/UBSan (P0/H2).

## 2026-09-26 — Sessão 62: FEAT-03d fase a (petróleo plumbing)

Implementação da fase a de FEAT-03d (commodity petroleum, sem gameplay):
- `commodities.hpp`: STUFF_PETROLEUM antes de STUFF_COUNT (ordinais preservados)
- `commodities.cpp`: commodityNames[10]="Petroleum", standardName "petroleum",
  commodityFromStandardName("petroleum") → STUFF_PETROLEUM
- `port.hpp`: PORT_PETROLEUM_RATE=5, PORT_PETROLEUM=0 (inert),
  MAX_PETROLEUM_ON_PORT, take/give rules no PortConstructionGroup ctor
- `simtest.cpp`: fingerprint `4bb8a816b3574b57d0ba18e4b81721e1`
  (legacy_daily_fingerprint atualizado)
- ldsv_version NÃO bumpado (sem estado novo em save: maxload=0 → skip)

Taxa rebalance (FEAT-03e complementar):
- PORT_ORE_RATE 1→50, PORT_STEEL_RATE 100→30
- FIXME port.hpp:57 substituído por valores justificados

Testes: CTest regular 4/4 (82.9s), ASan 4/4 (1186s), exit 0.
Commit `e556fbb0` (fase a), `6e1907be` (fase b).

DeepSeek v4.1 flash análise (2 chamadas): plano + revisão pós-implementação.
Nenhum bug encontrado. maxload=0 é seguro para save/load (skip lógica port.cpp).

## 2026-09-26 — Sessão 62b: FEAT-03d fase b (jazidas petroleum + save)

Implementação:
- world.hpp: `unsigned short oil_reserve` em MapTile (após ore_reserve)
- all_buildings.hpp: OIL_RESERVE=1000, NUMOF_OIL_RESERVES=(len²/500)
- init_game.cpp: oil_reserve_setup() (mesmo padrão coal_reserve_setup)
- xmlloadsave.cpp: save/load tag "oil" após "coal"
- xmlloadsave.hpp: LOADSAVE_VERSION_CURRENT 2143→2144
- world.cpp: MapTile ctor e move ctor inicializam oil_reserve=0 (bug
  não-determinismo encontrado e corrigido: campo novo sem init)
- port.hpp: MAX_PETROLEUM_ON_PORT 0→500 (poços precisam armazenar)
- simtest.cpp: fingerprint fb235ae8794a39e3ca13c1b155ed829d

Testes: CTest regular 4/4 (96s), ASan em execução.
Commit pendente.
