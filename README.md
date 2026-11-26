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

## 8. Построить координатный индекс

Сначала должен быть построен булев индекс: координатный индекс использует ту же
таблицу соответствия `docID` и MongoDB ObjectId.

```bash
./.build/positional_index configs/positional_index.yaml
```

Результат сохраняется в `results/positional_index`. Индекс необходимо
перестраивать после изменения обработанного корпуса или булева индекса.

## 9. Запустить цитатный поиск

```bash
./.build/phrase_search configs/search.yaml
```

Пример точной цитаты:

```text
"distributed database"
```

Кавычки необязательны: каждая строка стандартного ввода считается одной
цитатой. Выход из интерактивного режима - `Ctrl+D`.

## 10. Открыть поиск в браузере

После сборки C++-компонентов, запуска MongoDB и построения индексов:

```bash
uv run python src/web/main.py configs/search.yaml
```

Открыть [http://127.0.0.1:8080](http://127.0.0.1:8080). На странице доступны
TF-IDF, булевый и цитатный поиск. Запросы выполняются теми же C++-алгоритмами,
что и в консольных утилитах. Начало текста, заголовок и ссылка берутся из MongoDB.
Лимиты выдачи задаются в `configs/search.yaml`. Для остановки сервера - `Ctrl+C`.
Перед перестроением индексов остановить веб-сервер и запустить его снова
после завершения построения.

Если порт занят:

```bash
uv run python src/web/main.py configs/search.yaml --port 8081
```

## 11. Запустить тесты

Python-тесты crawler и веб-интерфейса:

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

Только тесты координатного индекса и цитатного поиска:

```bash
ctest --test-dir .build -R 'unit\.(positional_index|phrase_search)\.' --output-on-failure
```

Только тесты веб-интерфейса (без MongoDB):

```bash
uv run pytest tests/web -q
```

## 12. Остановить MongoDB

```bash
docker compose down
```
