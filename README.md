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
| `IndexDocumentTool` | `indexar_documento` | Indexa um arquivo na base RAG. Args: `filepath`, `chunk_size` (opcional), `force` (opcional). |
| `SemanticSearchTool` | `busca_semantica` | Busca documentos indexados por similaridade semântica. Args: `query`, `top_k` (opcional). |

### Mensagens de voz (Whisper STT)

O bot transcreve automaticamente mensagens de voz via **OpenAI Whisper API** (`whisper-1`). Quando o usuário envia um áudio, o bot:

1. Baixa o arquivo `.oga` para `./tmp/`
2. Envia para `https://api.openai.com/v1/audio/transcriptions`
3. Encaminha a transcrição ao Agent Loop como texto
4. Responde em áudio automaticamente (se `TTS_ENABLED=true`)
5. Remove o arquivo temporário

Usa a `OPENAI_API_KEY` já configurada. Sem dependências extras.

```env
VOICE_ENABLED=true       # habilita/desabilita o handler de voz
WHISPER_MODEL=whisper-1  # modelo (sempre whisper-1 para a API OpenAI)
WHISPER_LANGUAGE=pt      # idioma (vazio = auto-detect)
```

### Configuração da pesquisa na internet

Crie uma conta em [serper.dev](https://serper.dev) (2.500 buscas/mês gratuitas, sem cartão) e adicione a chave no `.env`:

```env
SERPER_API_KEY=sua_chave_aqui
```

### Base de conhecimento RAG (busca semântica)

O DaniClaw suporta RAG (Retrieval-Augmented Generation) com [sqlite-vec](https://github.com/asg017/sqlite-vec) embutido — sem servidor externo.

**Como usar:**

1. Adicione as vars ao `.env`:
   ```env
   EMBEDDING_MODEL=text-embedding-3-small
   EMBEDDING_DIMS=1536
   ```

2. Peça ao bot para indexar um documento:
   ```
   indexa o arquivo /data/docs/manual.txt
   ```

3. O agente usará automaticamente `busca_semantica` ao responder perguntas sobre o conteúdo indexado.

**Detalhes técnicos:**
- Embeddings gerados via OpenAI (`text-embedding-3-small`, ~$0.02/1M tokens)
- Vetores armazenados no mesmo SQLite via extensão `vec0`
- Chunking automático com overlap (padrão: 800 chars, overlap 100)
- Para reindexar um arquivo: `force=true`

## Monitoramento de Gmail (IMAP)

O DaniClaw pode monitorar sua caixa de entrada do Gmail e notificar chats do Telegram quando novos e-mails chegarem.

**Configuração:**

1. Ative a autenticação de dois fatores no Google e gere uma [senha de app](https://myaccount.google.com/apppasswords).
2. Preencha no `.env`:

```env
GMAIL_ENABLED=true
GMAIL_USER=seuemail@gmail.com
GMAIL_APP_PASSWORD=xxxx xxxx xxxx xxxx
GMAIL_NOTIFY_CHAT_IDS=123456789           # chat IDs separados por vírgula
GMAIL_POLL_INTERVAL_SEC=60                # frequência de checagem
```

**Como funciona:**
- Usa libcurl com protocolo IMAP para buscar e-mails `UNSEEN`
- UIDs já notificados são persistidos em SQLite (tabela `gmail_seen`)
- Envia título + remetente para os chats configurados

---

## Feature Toggles

Todos os recursos podem ser desabilitados individualmente no `.env` sem recompilar:

| Variável | Padrão | Recurso |
|----------|--------|---------|
| `TOOL_FILE_ENABLED` | `true` | Ferramentas `criar_arquivo` e `ler_arquivo` |
| `TOOL_REMINDERS_ENABLED` | `true` | Ferramenta `agendar_lembrete` |
| `TOOL_TIMEZONE_ENABLED` | `true` | Ferramenta `definir_timezone` |
| `TOOL_SHELL_ENABLED` | `true` | Ferramenta `executar_comando` |
| `TOOL_WEB_SEARCH_ENABLED` | `true` | Ferramenta `pesquisar_internet` |
| `TOOL_RAG_ENABLED` | `true` | Ferramentas RAG (`indexar_documento`, `busca_semantica`) |
| `SKILLS_ENABLED` | `true` | Roteamento e injeção de skills |
| `MEMORY_ENABLED` | `true` | Histórico de conversas (contexto entre mensagens) |
| `TTS_ENABLED` | `true` | Resposta em áudio (síntese de fala) |
| `VOICE_ENABLED` | `true` | Handler de mensagens de voz (transcrição Whisper) |
| `DOCUMENT_ENABLED` | `true` | Handler de documentos PDF/Markdown |
| `GMAIL_ENABLED` | `false` | Poller IMAP do Gmail |
| `MULTI_AGENT_ENABLED` | `true` | Painel de 3 agentes para perguntas factuais (Pesquisador → Crítico → Sintetizador) |

---

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
