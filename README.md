# MAI-IR

Краткий порядок запуска компонентов проекта.

## 1. Подготовить окружение

Системные зависимости:

```bash
sudo apt install build-essential cmake libmongoc-dev libbson-dev libxml2-dev
```

Python-зависимости:

```bash
uv sync
```

Запустить MongoDB:

```bash
docker compose up -d
docker compose ps
```

## 2. Собрать корпус

Crawler сохраняет сырой HTML из Wikipedia, arXiv и Microsoft Learn в MongoDB:

```bash
uv run python src/crawler/main.py configs/crawler.yaml
```

Для остановки нажать `Ctrl+C`. Повторный запуск продолжает обход и обновляет устаревшие документы.

## 3. Собрать C++-компоненты

```bash
cmake -S . -B .build -DCMAKE_BUILD_TYPE=Release
cmake --build .build --parallel
```

## 4. Обработать документы

Извлечение текста, токенизация, лемматизация и подсчёт частот:

```bash
./.build/linguistics configs/linguistics.yaml
```

Результаты сохраняются в MongoDB, `results/statistics.json` и `results/zipf.svg`.

## 5. Построить индекс

```bash
./.build/boolean_index configs/boolean_index.yaml
```

Бинарные файлы создаются в `results/boolean_index`.

Индекс необходимо перестраивать после изменения обработанного корпуса.

## 6. Запустить булев поиск

```bash
./.build/boolean_search configs/search.yaml
```

Пример запроса:

```text
database AND (security OR cloud)
```

Выход из интерактивного режима - `Ctrl+D`.

## 7. Запустить TF-IDF-поиск

```bash
./.build/tfidf_search configs/search.yaml
```

Пример запроса:

```text
distributed database replication
```

Выход из интерактивного режима - `Ctrl+D`.

## 8. Запустить тесты

Python-тесты crawler:

```bash
uv run pytest
```

C++-тесты:

```bash
ctest --test-dir .build --output-on-failure
```

Отдельные тесты с кратким сравнением линейного булева поиска и поиска
с прыжками:

```bash
./.build/boolean_search_comparison_tests
```

## 9. Остановить MongoDB

```bash
docker compose down
```
