# Команды для полной демонстрации

## Перейти в корень проекта — терминал WSL

```bash
cd /home/ubuntuwsl/mai/mai-ir
```

## Установить системные зависимости — терминал WSL

```bash
sudo apt update
sudo apt install build-essential cmake libmongoc-dev libbson-dev libxml2-dev libgtest-dev
```

## Установить Python-зависимости — терминал WSL

```bash
uv sync
```

## Запустить MongoDB — терминал WSL

```bash
docker compose up -d
```

## Проверить контейнер MongoDB — терминал WSL

```bash
docker compose ps
```

## Проверить подключение к MongoDB — терминал WSL

```bash
docker compose exec -T mongodb mongosh --quiet --eval \
  'db.adminCommand({ping: 1})'
```

## Запустить ноутбуки анализа источников — терминал WSL

```bash
uv run jupyter notebook notebooks
```

## Запустить поисковый робот — терминал WSL

```bash
uv run python src/crawler/main.py config.yaml
```

## Подключиться через MongoDB Compass — строка подключения

```text
mongodb://127.0.0.1:27017/mai_ir
```

## Показать сырой документ — Compass, `documents`, поле Filter

```javascript
{ source: "wikipedia" }
```

## Показать поля сырого документа — Compass, `documents`, поле Project

```javascript
{
  source: 1,
  source_id: 1,
  title: 1,
  url: 1,
  raw_size: 1,
  fetched_at: 1,
  checked_at: 1,
  content_hash: 1
}
```

## Посчитать документы по источникам — Compass, `documents`, Aggregations, стадия `$group`

```javascript
{
  _id: "$source",
  documents: { $sum: 1 }
}
```

## Отсортировать источники — Compass, `documents`, Aggregations, стадия `$sort`

```javascript
{ _id: 1 }
```

## Показать очередь Wikipedia — Compass, `crawl_queue`, поле Filter

```javascript
{ source: "wikipedia" }
```

## Собрать все C++-компоненты — терминал WSL

```bash
cmake -S . -B .build -DCMAKE_BUILD_TYPE=Release
cmake --build .build --parallel
```

## Запустить все C++-тесты — терминал WSL

```bash
ctest --test-dir .build --output-on-failure
```

## Запустить юнит-тесты токенизатора — терминал WSL

```bash
./.build/tokenizer_unit_tests
```

## Запустить юнит-тесты лемматизации — терминал WSL

```bash
./.build/lemmatization_unit_tests
```

## Запустить юнит-тесты выделения текста из HTML — терминал WSL

```bash
./.build/html_extraction_unit_tests
```

## Запустить юнит-тесты конфигурации — терминал WSL

```bash
./.build/config_unit_tests
```

## Запустить юнит-тесты булева индекса — терминал WSL

```bash
./.build/boolean_index_unit_tests
```

## Запустить юнит-тесты булева поиска — терминал WSL

```bash
./.build/boolean_search_unit_tests
```

## Запустить интеграционные тесты статистики и SVG — терминал WSL

```bash
./.build/document_processing_integration_tests
```

## Запустить интеграционный тест восстановления — терминал WSL

```bash
./.build/recovery_integration_tests
```

## Запустить только unit-тесты — терминал WSL

```bash
ctest --test-dir .build -L unit --output-on-failure
```

## Запустить все GoogleTest-сценарии — терминал WSL

```bash
ctest --test-dir .build -L gtest --output-on-failure
```

## Запустить тесты crawler — терминал WSL

```bash
uv run pytest -q tests/crawler
```

## Собрать C++-компоненты и запустить C++-тесты одной командой — терминал WSL

```bash
./check.sh
```

## Запустить выделение текста, токенизацию и лемматизацию — терминал WSL

```bash
./.build/linguistics config.yaml
```

## Показать принятый обработанный документ — Compass, `processed_documents`, поле Filter

```javascript
{ accepted: true }
```

## Показать поля обработанного документа — Compass, `processed_documents`, поле Project

```javascript
{
  source: 1,
  source_id: 1,
  title: 1,
  text_size: 1,
  word_count: 1,
  token_count: 1,
  parser_version: 1,
  accepted: 1
}
```

## Показать токены и леммы — Compass, коллекция `document_terms`

```javascript
{}
```

## Показать состояние глобальных частот — Compass, `linguistics_state`, поле Filter

```javascript
{ _id: "frequencies" }
```

## Показать статистику корпуса и параметры Мандельброта — терминал WSL

```bash
uv run python -c '
import json
s = json.load(open("results/statistics.json", encoding="utf-8"))
print("documents:", s["corpus"]["documents"])
print("tokens:", s["corpus"]["tokens"])
print("token vocabulary:", s["corpus"]["token_vocabulary"])
print("lemma vocabulary:", s["corpus"]["lemma_vocabulary"])
print("tokenization KiB/s:", round(s["corpus"]["tokenization_kib_per_second"], 1))
print("Mandelbrot:", s["mandelbrot"])
'
```

## Открыть график Ципфа–Мандельброта — терминал WSL

```bash
explorer.exe "$(wslpath -w results/zipf.svg)"
```

## Построить булев индекс — терминал WSL

```bash
./.build/boolean_index config.yaml
```

## Показать файлы булева индекса — терминал WSL

```bash
ls -lh results/boolean_index
```

## Показать формат булева индекса — терминал WSL

```bash
sed -n '1,90p' src/boolean_index/format.hpp
```

## Выполнить готовые булевы запросы — терминал WSL

```bash
printf 'database AND security\n(database OR storage) AND retrieval\nNOT cloud AND retrieval\n' \
  | ./.build/boolean_search config.yaml \
  | sed -n '1,70p'
```

## Запустить интерактивный булев поиск — терминал WSL

```bash
./.build/boolean_search config.yaml
```

## Сохранить результаты булева поиска в файл — терминал WSL

```bash
printf 'database AND security\ncloud OR retrieval\nNOT database AND security\n' \
  | ./.build/boolean_search config.yaml \
  > results/boolean_search.txt
```

## Показать сохранённые результаты булева поиска — терминал WSL

```bash
sed -n '1,160p' results/boolean_search.txt
```

## Показать готовые отчёты — терминал WSL

```bash
ls -lh reports/pdf
```

## Собрать отчёт по анализу корпуса — терминал WSL

```bash
typst compile --root reports --font-path /mnt/c/Windows/Fonts \
  reports/source/lesson01.typ reports/pdf/lesson01.pdf
```

## Собрать отчёт по поисковому роботу — терминал WSL

```bash
typst compile --root reports --font-path /mnt/c/Windows/Fonts \
  reports/source/lesson02.typ reports/pdf/lesson02.pdf
```

## Показать сырой корпус без Compass — терминал WSL

```bash
docker compose exec -T mongodb mongosh --quiet mai_ir --eval '
print("=== DOCUMENTS BY SOURCE ===");
printjson(db.documents.aggregate(
  [{$group: {_id: "$source", documents: {$sum: 1}}}, {$sort: {_id: 1}}],
  {hint: {source: 1, source_id: 1}}
).toArray());
print("=== RAW DOCUMENT ===");
printjson(db.documents.findOne(
  {source: "wikipedia"},
  {_id: 0, source: 1, source_id: 1, title: 1, url: 1,
   raw_size: 1, fetched_at: 1, checked_at: 1, content_hash: 1}
));
print("=== CRAWL STATE ===");
printjson(db.crawl_state.find({}).limit(3).toArray());
'
```

## Показать результаты лингвистической обработки без Compass — терминал WSL

```bash
docker compose exec -T mongodb mongosh --quiet mai_ir --eval '
print("=== PROCESSED DOCUMENT ===");
printjson(db.processed_documents.findOne(
  {accepted: true},
  {_id: 0, source: 1, source_id: 1, title: 1, text_size: 1,
   word_count: 1, token_count: 1, parser_version: 1, accepted: 1}
));
print("=== TOKENS AND LEMMAS ===");
printjson(db.document_terms.findOne(
  {},
  {_id: 0, source_id: 1, tokens: {$slice: 8}, lemmas: {$slice: 8}}
));
print("=== PROCESSOR STATE ===");
printjson(db.linguistics_state.findOne({_id: "frequencies"}));
'
```

## Показать последние сообщения MongoDB — терминал WSL

```bash
docker compose logs --tail=50 mongodb
```

## Остановить MongoDB без удаления корпуса — терминал WSL

```bash
docker compose down
```
