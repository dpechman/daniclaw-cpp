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
   ./build/sandeclaw
   ```

## Estrutura do Projeto

* `src/main.cpp` - Ponto de entrada.
* `src/telegram/` - Cliente bot via Long Polling + JSON.
* `src/agent/` - Loop ReAct portado.
* `src/skills/` - Carregador de hot-reload em YAML e sistema de rotas por LLM.
* `src/database/` - SQLite3 nativo com transações seguras Thread/WAL.
* `data/`, `.agents/`, `tmp/` e `output/` - Espelham a arquitetura da versão TS.
