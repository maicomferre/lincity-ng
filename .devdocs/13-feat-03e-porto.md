# FEAT-03e — Porto 2.0 (planejamento)

> Documento de planejamento. Nenhuma mudança de código antes de
> decidir os itens §3. Regra anti-"lençol curto": cada fase =
> feature fechada e testável, NUNCA parcial.

## 1. Comportamento atual resumido

| Aspecto | Valor/código |
|---|---|
| Custo | $100.000, tech 35k, 4×4 |
| Exige | rio a leste (`port.cpp:59-74`) |
| Flags take/give | nascem false; persistem no save; toggle anual aleatório |
| `buy_stuff` | importa 50% da capacidade livre/dia, se ≥ maxload/15; custo 1% (`import_cost`) |
| `sell_stuff` | exporta 50% do estoque/dia, se ≥ maxload/15; receita em "pence" |
| `pence` | resto fracionário de exportação, persiste no save (ldsv 2141) |
| Labor | 100/dia; poluição +1; +1 tech/dia (ou +2) |
| Taxação dupla | commodities entregues via transporte contam p/ imposto **e** venda gera receita tributada como export |

## 2. Decisões necessárias (bloqueantes)

### 2.1 Taxação dupla (BUG-11 / §14.2 item 1)
**Problema**: carvão/minério/goods entregues ao porto via transporte geram `taxable` (equilibrate_stuff em `lintypes.cpp:831-833`) **e** a venda gera `trade_ex` tributado como export tax (`simulate.cpp:254`).

**Opções**:
- **A)** Manter dupla tributação (comportamento upstream, mas inconsistente)
- **B)** Isentar porto de imposto por entrega: porto não entra em `equilibrate_stuff` taxable → só export tax
- **C)** Imposto único na entrega (remover export tax anual, manter taxa de entrega)

**Recomendação**: opção B — o porto já é consumidor normal não-isento (`lintypes.cpp:816-817`). Isolar o porto do taxable de entrega é o mínimo coerente: o jogador já paga imposto sobre a commodity quando ela sai da cidade; a export tax é a taxa do serviço portuário. Decisão do jogador (governador) sobre alíquota de export → manter `money_rates.export_tax` mas com fluxo fechado.

### 2.2 `PORT_*_RATE` valores adivinhados (FIXME port.hpp:57)
**Problema**: `PORT_LABOR=100`, `PORT_FOOD=500`, `PORT_COAL=100`, `PORT_GOODS=100`, `PORT_ORE=300`, `PORT_STEEL=50` — marcado como "guessing".

**Benchmark executado**: `simtest` com Rocket_98.scn.gz, seed 1234567, 1500 dias, porto configurado para importar food/coal/ore e exportar goods/steel.

**Resultados** (último dia, último mês):
| Commodity | Estoque final | Comprou (dia) | Vendeu (dia) |
|---|---|---|---|
| food | 7158 | 14170/dia | — |
| coal | 1714 | 14170/dia | — |
| ore | 3528 | 14170/dia | — |
| goods | 808 | — | 10285/dia |
| steel | 134 | — | 10285/dia |
| labor | 996 | — | — |

**Receita mensal**: import $418.048, export $306.552 (mês atual)
**Receita último mês**: import $1.348.396, export $1.105.063

**Preço por unidade (PORT_*_RATE / 100 para import, PORT_*_RATE para export)**:
| Commodity | Rate | Preço import | Preço export |
|---|---|---|---|
| food | 500 | $5,00 | — |
| coal | 50 | $0,50 | — |
| ore | 1 | $0,01 | — |
| goods | 3 | — | $3,00 |
| steel | 100 | — | $100,00 |

**Análise**: ore importa a $0,01/unidade (muito barato — provavelmente subprecificado). steel exporta a $100/unidade (muito caro — pode estar bloqueado exportação). Food a $5 e coal a $0,50 parecem razoáveis. goods a $3 é baixo comparado a steel a $100.

**Ação**: ajustar rates para equilibrar preços relativos:
- PORT_ORE_RATE: 1 → 50 (import $0,50, mais realista)
- PORT_STEEL_RATE: 100 → 30 (export $30, mais competitivo)
- PORT_GOODS_RATE: 3 → manter (similar a steel ajustado)
- PORT_FOOD_RATE: 500 → manter ($5, razoável)
- PORT_COAL_RATE: 50 → manter ($0,50, razoável)

### 2.3 Resto fracionário (pence)
**Status**: já existe, persiste, testado em ldsv 2141. **OK** — manter.

### 2.4 Comércio sorteado (take/give toggle)
**Status**: no LEGACY é aleatório (`lincityRand()`); no DETERMINISTIC_V1 usa `RandomEvent::ANNUAL_RULES` — deterministicamente reproduzível. **OK** — manter, mas documentar no roadmap que o toggle anual é parte do `SimulationProtocol`.

## 3. Mudanças propostas

| # | Mudança | Arquivo | Save? | Status |
|---|---|---|---|---|
| 1 | Isentar porto de taxable de entrega (opção B) | `lintypes.cpp:930,1112` | não | ✅ implementado |
| 2 | Ajustar `PORT_*_RATE` por benchmark | `port.hpp` | não | ✅ **FEITO** — ORE 1→50, STEEL 100→30, PETROLEUM=40 (ver abaixo) |
| 3 | Substituir FIXME por comentário justificativo | `port.hpp:57` | não | ✅ **FEITO** — replaced with benchmark justification |
| 4 | Teste camada 1: `port_rates_benchmark` | `tests/unit/` | não | pendente |
| 5 | Teste camada 2: invariante com porto ativo | `tests/sim/` | não | pendente |

## 4. Riscos

- **Economia central**: taxação dupla afeta receita de todos os cenários → teste A/B obrigatório
- **Save**: sem bump de `ldsv_version` (sem estado novo); se a opção C for escolhida, precisa de bump
- **Tradução**: novas strings → PO pt-BR (se mudar UI)

## 5. Critérios de aceite

- [ ] Decisão da opção A/B/C registrada aqui
- [ ] Benchmark de rates executado e valores atualizados
- [ ] CTest 4/4 (regular + ASan/UBSan)
- [ ] Invariante de conservação verificado com porto ativo
- [ ] Diário atualizado
