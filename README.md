# DaniClaw C++

Versão portável e de alto desempenho do DaniClaw em C++.

## Dependências

Este projeto requer os seguintes pacotes de sistema base (Debian/Ubuntu):
```sh
sudo apt update
sudo apt install build-essential cmake curl zip unzip tar pkg-config
```

## Compilação (usando vcpkg)

O projeto usa o `vcpkg` para gerenciar dependências C++ (`curl`, `nlohmann_json`, `spdlog`, `sqlite3`, `yaml-cpp`).

1. **Instale e configure o vcpkg (se não tiver)**:
   ```sh
   git clone https://github.com/microsoft/vcpkg.git
   ./vcpkg/bootstrap-vcpkg.sh
   export VCPKG_ROOT=$(pwd)/vcpkg
   ```

2. **Configure com o CMake (`--preset` ou toolchain)**:
   ```sh
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
   ```

3. **Compile o projeto**:
   ```sh
   cmake --build build --config Release
   ```

## Executando

1. Copie o `.env.example` e preencha suas chaves de API:
   ```sh
   cp .env.example .env
   # Edite o .env (TELEGRAM_BOT_TOKEN, GROQ_API_KEY, etc.)
   ```

2. Inicie o bot:
   ```sh
   ./build/daniclaw
   ```

## Ferramentas disponíveis (Tools)

O agente possui acesso às seguintes ferramentas, registradas automaticamente no `ToolRegistry`:

| Tool | Nome interno | Descrição |
|------|-------------|-----------|
| `WebSearchTool` | `pesquisar_internet` | Pesquisa no Google via [Serper.dev](https://serper.dev). Args: `query`, `num` (opcional, padrão 5). |
| `ShellTool` | `executar_comando` | Executa um comando shell e retorna stdout + stderr. |
| `CreateFileTool` | `criar_arquivo` | Cria um arquivo local com conteúdo fornecido. Args: `filepath`, `content`. |
| `ReadFileTool` | `ler_arquivo` | Lê e retorna o conteúdo de um arquivo local. Args: `filepath`. |
| `ScheduleReminderTool` | `agendar_lembrete` | Agenda envio de mensagem no futuro. Args: `chat_id`, `message`, `delay_minutes` ou `data_iso_utc`. |
| `SetTimezoneTool` | `definir_timezone` | Define o fuso horário do assistente. Args: `offset_hours` (ex: `-3` para BRT). |

### Configuração da pesquisa na internet

Crie uma conta em [serper.dev](https://serper.dev) (2.500 buscas/mês gratuitas, sem cartão) e adicione a chave no `.env`:

```env
SERPER_API_KEY=sua_chave_aqui
```

## Personalizando o Assistente com Skills

Skills são "modos de comportamento" carregados dinamicamente que transformam o DaniClaw em qualquer tipo de assistente especializado — sem recompilar o código.

### Como funciona

1. O `SkillLoader` escaneia a pasta `.agents/skills/` a cada requisição (hot-reload).
2. O `SkillRouter` usa um LLM leve para detectar qual skill usar com base na mensagem do usuário.
3. O conteúdo da skill é injetado no system prompt da conversa.

### Criando uma skill nova

Crie uma pasta com o nome da skill dentro de `.agents/skills/` e adicione um arquivo `SKILL.md`:

```
.agents/
└── skills/
    └── minha-skill/
        └── SKILL.md
```

O `SKILL.md` segue este formato:

```markdown
---
name: minha-skill
description: Uma linha descrevendo quando esta skill deve ser ativada.
---

# Skill: Minha Skill

Você é um [papel/persona] especializado em [domínio].

## Instruções
[Descreva aqui como o assistente deve se comportar, responder, formatar as respostas, etc.]
```

### Exemplos de skills que você pode criar

| Skill | Descrição no frontmatter |
|-------|--------------------------|
| Suporte técnico | "Atende chamados de suporte de TI, coleta informações e escala quando necessário." |
| Chef culinário | "Sugere receitas com base nos ingredientes disponíveis e restrições alimentares." |
| Tutor de idiomas | "Corrige texto em inglês, explica erros gramaticais e sugere alternativas naturais." |
| Analista financeiro | "Interpreta dados financeiros, calcula indicadores e explica conceitos de investimento." |
| Assistente jurídico | "Resume documentos legais em linguagem simples e identifica cláusulas de risco." |

### Dicas

- **`description`** é o texto que o LLM usa para decidir se deve ativar a skill — seja específico.
- Uma skill pode conter **exemplos de input/output** para guiar melhor o comportamento.
- Skills convivem com as **tools** — o agente pode pesquisar na internet, executar comandos e ainda seguir as instruções da skill ao mesmo tempo.
- Para desativar uma skill temporariamente, basta renomear o arquivo para `SKILL.md.disabled`.
- O provedor usado para rotear skills é configurável via `SKILL_ROUTER_PROVIDER` no `.env` — use um modelo mais barato para economizar.

## Estrutura do Projeto

* `src/main.cpp` - Ponto de entrada.
* `src/telegram/` - Cliente bot via Long Polling + JSON.
* `src/agent/` - Loop ReAct portado.
* `src/skills/` - Carregador de hot-reload em YAML e sistema de rotas por LLM.
* `src/tools/` - Ferramentas do agente (tools), cada uma herda `BaseTool`.
* `src/database/` - SQLite3 nativo com transações seguras Thread/WAL.
* `data/`, `.agents/`, `tmp/` e `output/` - Espelham a arquitetura da versão TS.
