# Лабораторная работа 1. Добыча и анализ корпуса

## 1. Задача

В [задании преподавателя](https://github.com/toshunster/MAI-IR/blob/main/lesson-01/homework.md)
требуется:

- выбрать не менее двух источников документов;
- скачать примеры;
- изучить формат сырого документа и метаданные;
- выделить основной текст;
- посчитать размеры и средние характеристики;
- найти существующие поисковики по этим данным;
- показать примеры запросов и недостатки существующей выдачи.

В проекте выбраны три источника технических текстов: Wikipedia, arXiv HTML и
Microsoft Learn. Такое объединение даёт энциклопедические, научные и прикладные
документы в одной широкой теме — информационные технологии и компьютерные науки.

## 2. Где находится результат

Каждый источник исследуется в отдельном ноутбуке:

- `notebooks/wikipedia.ipynb`;
- `notebooks/arxiv.ipynb`;
- `notebooks/microsoft_learn.ipynb`.

В каждом ноутбуке одинаковая логическая структура:

1. получение 30 примеров;
2. описание сырого формата и метаданных;
3. выделение текста;
4. расчёт статистики;
5. график распределения объёма;
6. примеры существующего поиска и его недостатки.

Локальные выборки записываются в `notebooks/samples`. Они нужны, чтобы повторное
открытие ноутбука не создавало лишнюю нагрузку на сайты.

## 3. Как запустить

```bash
uv sync
uv run jupyter notebook notebooks
```

В браузере нужно последовательно открыть три файла и выполнить `Run All`.
Ноутбуки используют UTF-8.

## 4. Wikipedia

### Получение документов

Используется официальный MediaWiki API:

```text
https://en.wikipedia.org/w/api.php
```

Выборка строится по категориям Computer science, Engineering, Mathematics,
Physics, Chemistry и Biology. Сначала API возвращает членов категории, затем для
выбранных страниц запрашиваются `pageid`, заголовок, полный URL, Wikidata ID и
текстовый `extract`.

В ноутбуке используется именно текстовый `extract`, а не HTML. Это допустимо для
первичного анализа источника: поле уже содержит очищенный API-текст. В crawler для
общего конвейера позднее сохраняется HTML страницы, чтобы все источники имели
единый сырой формат и можно было повторно улучшать извлечение текста.

### Метаданные

- `pageid` — устойчивый идентификатор страницы;
- `title` — заголовок;
- `fullurl` — канонический URL;
- `wikibase_item` — Wikidata ID;
- категория, из которой документ попал в выборку.

### Выделение текста

API уже возвращает текст без HTML-тегов. Обработка сводится к нормализации
последовательностей пробельных символов.

### Разбор отдельной страницы: Computer science

В выборке есть статья `Computer science` с `pageid = 5323`. Для неё важно
различать два представления.

Исходная страница для самостоятельного просмотра HTML:
[Computer science — Wikipedia](https://en.wikipedia.org/wiki/Computer_science).
В браузере можно открыть DevTools → Elements и найти `id="mw-content-text"`.

В ноутбуке её API JSON занимает 30 814 байт, выделенный текст — 30 137 байт,
число слов — 4 306. Здесь сырой размер относится именно к JSON-ответу API, а не к
HTML, который позднее сохраняет crawler.

В ноутбуке запрашивается API-представление. Сокращённый ответ выглядит так:

```json
{
  "pageid": 5323,
  "title": "Computer science",
  "extract": "Computer science is the study of computation, information...",
  "fullurl": "https://en.wikipedia.org/wiki/Computer_science",
  "pageprops": {
    "wikibase_item": "Q21198"
  }
}
```

Поле `extract` уже очищено MediaWiki от HTML. Код ноутбука запрашивает именно его:

```python
response = session.get(API_URL, params={
    'action': 'query',
    'format': 'json',
    'formatversion': 2,
    'pageids': page_id,
    'prop': 'extracts|info|pageprops',
    'explaintext': 1,       # просим обычный текст, а не HTML
    'inprop': 'url',
}, timeout=60)

page = response.json()['query']['pages'][0]
text = re.sub(r'\s+', ' ', page.get('extract', '')).strip()
```

Разбор по шагам:

1. `pageids` делает запрос устойчивым к переименованию статьи.
2. `extracts` добавляет содержимое статьи.
3. `explaintext=1` заставляет API вернуть текст без wiki/HTML-разметки.
4. `info` и `inprop=url` добавляют канонический URL.
5. `pageprops` добавляет Wikidata ID.
6. Регулярное выражение схлопывает переносы строк и повторные пробелы.

Рабочий crawler сохраняет не `extract`, а исходную HTML-страницу. Её структура
приблизительно выглядит так; фрагмент сокращён до значимых узлов:

```html
<body>
  <header>...</header>
  <main id="content">
    <h1 id="firstHeading">Computer science</h1>
    <div id="mw-content-text">
      <div class="mw-parser-output">
        <p><b>Computer science</b> is the study of computation...</p>
        <div id="toc">...</div>
        <h2>Fields of computer science</h2>
        <p>The discipline includes algorithms, data structures...</p>
        <div class="reflist">...</div>
        <table class="navbox">...</table>
      </div>
    </div>
  </main>
  <footer>...</footer>
</body>
```

Для общего C++-конвейера выбирается `#mw-content-text`, затем удаляются `toc`,
`reflist`, `navbox`, кнопки редактирования, header/footer и навигация. В результате
остаются первый и второй абзацы, но не оглавление, ссылки литературы и навигационная
таблица.

```cpp
const char *root_xpath = "//*[@id='mw-content-text']";
xmlNodePtr root = first_xpath_node(context, root_xpath);
context->node = root;

remove_xpath_nodes(
    context,
    ".//*[contains(concat(' ', normalize-space(@class), ' '), "
    "' mw-editsection ') or "
    "contains(concat(' ', normalize-space(@class), ' '), ' navbox ') or "
    "contains(concat(' ', normalize-space(@class), ' '), ' reflist ')]"
    "|.//*[@id='toc']"
);
```

То есть ноутбук демонстрирует доступность чистого текста через API, а production
обработчик самостоятельно выделяет текст из сохранённого HTML. Это позволяет
показать преподавателю оба уровня источника.

### Результаты выборки

| Показатель | Значение |
|---|---:|
| Документов | 30 |
| Сырой размер | 643 895 байт |
| Размер выделенного текста | 623 248 байт |
| Средний сырой документ | 21 463,17 байт |
| Средний текст | 20 774,93 байт |
| Среднее количество слов | 3 121,37 |

### Существующий поиск

Используется поиск Wikipedia. Примеры:

- `neural information retrieval`;
- `python memory model`.

Недостатки для будущей собственной системы: поиск ограничен только Wikipedia,
не объединяет научные статьи и документацию, а управление точной булевой логикой
и объяснение нормализации запроса ограничены.

## 5. arXiv HTML

### Получение документов

Метаданные получаются из Atom API:

```text
https://export.arxiv.org/api/query
```

HTML каждой работы загружается отдельно со страницы:

```text
https://arxiv.org/html/<arxiv-id>
```

Выборка содержит по 10 документов из `cs.IR`, `cs.AI` и `cs.CL`, всего 30.
Между запросами выдерживается пауза, чтобы не создавать агрессивную нагрузку.

### Метаданные

- arXiv ID и версия;
- название;
- список авторов;
- abstract;
- категории;
- даты публикации и обновления;
- лицензия;
- категория, по которой статья выбрана в выборку.

### Выделение текста

В HTML выбирается `article.ltx_document`, с запасными вариантами `article` и
`main`. Удаляются `script`, `style`, навигация, боковые блоки и footer. Формулы
остаются в текстовом представлении, но их исходная двумерная структура теряется.

### Разбор отдельной страницы arXiv

В качестве примера можно взять статью `Multimodal Reranking Is a System One
Decision` из категории `cs.IR`. Сокращённая структура arXiv HTML/LaTeXML имеет вид:

Исходная HTML-страница:
[Multimodal Reranking Is a System One Decision](https://arxiv.org/html/2610.28435v1).
В DevTools нужно найти `article.ltx_document`, затем сравнить обычный раздел
`ltx_section` с `ltx_bibliography`.

В исследовательской выборке её HTML занимает 393 967 байт, очищенный текст —
36 089 байт, число слов — 5 806. Большая разница на одной конкретной странице
наглядно показывает объём LaTeXML-разметки и формул.

```html
<html>
  <head>
    <title>Multimodal Reranking Is a System One Decision</title>
  </head>
  <body>
    <article class="ltx_document">
      <h1 class="ltx_title">Multimodal Reranking Is a System One Decision</h1>
      <div class="ltx_authors">...</div>

      <section class="ltx_abstract">
        <h2>Abstract</h2>
        <p>Multimodal document retrieval requires...</p>
      </section>

      <section class="ltx_section">
        <h2>1 Introduction</h2>
        <p>Recent retrieval systems...</p>
        <math>
          <semantics>
            <mrow>...</mrow>
            <annotation encoding="application/x-tex">s(q,d)</annotation>
          </semantics>
        </math>
      </section>

      <section class="ltx_bibliography">...</section>
      <footer class="ltx_page_footer">...</footer>
    </article>
  </body>
</html>
```

Главный полезный контейнер — `article.ltx_document`. Внутри него находятся abstract,
разделы, абзацы, списки, таблицы, формулы и код. Библиография формально тоже текст,
но для поискового корпуса она создаёт большое число фамилий и повторяющихся
названий работ, поэтому production-парсер её удаляет.

Код ноутбука:

```python
def extract_article_text(html):
    soup = BeautifulSoup(html, 'lxml')

    # Сначала специфичный arXiv-контейнер, затем безопасные fallback.
    article = soup.select_one('article.ltx_document, article, main')
    if article is None:
        return ''

    # Эти узлы не являются содержанием научной статьи.
    for node in article.select('script, style, nav, aside, footer'):
        node.decompose()

    # Берём только осмысленные блочные элементы, сохраняя порядок документа.
    blocks = [
        node.get_text(' ', strip=True)
        for node in article.select('h1, h2, h3, p, li, pre')
    ]
    return re.sub(r'\s+', ' ', ' '.join(blocks)).strip()
```

Разбор по шагам:

1. `BeautifulSoup(..., 'lxml')` восстанавливает даже неидеальный HTML.
2. CSS selector сначала ищет arXiv-класс, но оставляет fallback для старых страниц.
3. `decompose()` полностью удаляет узел вместе с дочерним текстом.
4. Селектор блоков исключает случайные атрибуты и служебные подписи.
5. `get_text(' ', strip=True)` не склеивает слова на границах вложенных тегов.
6. Финальная нормализация превращает HTML-переносы в обычные пробелы.

В C++-версии дополнительно удаляются bibliography и MathML `annotation`:

```cpp
remove_xpath_nodes(
    context,
    ".//*[contains(concat(' ', normalize-space(@class), ' '), "
    "' ltx_bibliography ') or "
    "contains(concat(' ', normalize-space(@class), ' '), "
    "' ltx_page_footer ')]|.//annotation"
);
```

Удаляется именно `annotation`, а не весь `math`: видимый текст формулы остаётся,
но дублирующая TeX-запись не попадает в корпус второй раз.

### Результаты выборки

| Показатель | Значение |
|---|---:|
| Документов | 30 |
| Сырой размер | 16 108 664 байта |
| Размер выделенного текста | 2 242 943 байта |
| Средний сырой документ | 536 955,47 байта |
| Средний текст | 74 764,77 байта |
| Среднее количество слов | 11 931,97 |

Большая разница между HTML и текстом объясняется формулами, служебной разметкой,
CSS-классами и структурой LaTeXML.

### Существующий поиск

Используется arXiv Search. Примеры:

- `neural information retrieval`;
- `vector database`.

Недостатки: поиск работает только по arXiv, не объединяет документацию и
энциклопедические статьи; пользователю трудно контролировать общую нормализацию
терминов для разных источников.

## 6. Microsoft Learn

### Получение документов

URL обнаруживаются через официальный индекс sitemap:

```text
https://learn.microsoft.com/_sitemaps/sitemapindex.xml
```

Выбираются английские страницы `/en-us/`. Исключаются ответы пользователей,
видео, события, профили, credentials, API-справочник и архив старых версий.
Выборка распределяется по разным продуктам, чтобы не получить 30 почти одинаковых
страниц одного раздела.

### Метаданные

- продукт Microsoft Learn;
- заголовок;
- описание из meta-тега;
- автор;
- дата документа;
- канонический URL.

### Выделение текста

Основной узел — `main`. Из него удаляются навигация, кнопки, скрипты, стили и
служебные блоки. Затем пробелы нормализуются.

### Разбор отдельной страницы Microsoft Learn

В выборке есть страница `Index vector data in Cosmos DB (in Azure and Fabric)`.
Сокращённо типичная Learn-страница выглядит так:

Исходная HTML-страница:
[Index vector data in Cosmos DB](https://learn.microsoft.com/en-us/cosmos-db/index-vector-data).
В DevTools нужно сравнить содержимое `main` с внешними `header`/`footer`, а внутри
`main` найти `article-metadata` и `site-user-feedback-footer`.

Её HTML в выборке занимает 61 456 байт, выделенный текст — 14 273 байта, число
слов — 2 149. Остальной объём приходится на общий шаблон сайта, метаданные,
навигацию и служебную разметку.

```html
<html lang="en-us">
  <head>
    <meta name="document_id" content="...">
    <meta name="description" content="Learn how to index vector data...">
    <meta name="author" content="seesharprun">
    <meta name="ms.date" content="2026-05-22T00:00:00Z">
    <link rel="canonical"
          href="https://learn.microsoft.com/en-us/azure/cosmos-db/...">
  </head>
  <body>
    <header>...</header>
    <main id="main" role="main">
      <div id="article-header">...</div>
      <div id="article-metadata">...</div>
      <h1>Index vector data in Cosmos DB</h1>
      <p>Vector indexes improve similarity search...</p>
      <h2>Configure a vector policy</h2>
      <pre><code>{ "path": "/embedding", ... }</code></pre>
      <table>...</table>
      <section id="site-user-feedback-footer">...</section>
    </main>
    <footer>...</footer>
  </body>
</html>
```

В отличие от arXiv, значимая информация находится не только в абзацах: команды,
JSON, таблицы параметров и элементы списка важны для технического поиска. Поэтому
селектор включает `li`, `pre` и `table`.

```python
def extract_clean_main_text(html):
    soup = BeautifulSoup(html, 'lxml')
    main = soup.select_one('main')
    if main is None:
        return ''

    for node in main.select(
        'script, style, nav, aside, footer, button, .notification, '
        '#article-header, #article-metadata, '
        '#site-user-feedback-footer, .doc-outline'
    ):
        node.decompose()

    blocks = [
        node.get_text(' ', strip=True)
        for node in main.select('h1, h2, h3, p, li, pre, table')
    ]
    return re.sub(r'\s+', ' ', ' '.join(blocks)).strip()
```

Разбор по шагам:

1. `main` отделяет статью от общей шапки и подвала сайта.
2. `article-header` удаляется, потому что заголовок отдельно берётся из `h1`.
3. `article-metadata` удаляется из текста, но автор и дата сохраняются как поля.
4. `doc-outline` удаляется: это повтор заголовков разделов.
5. `pre` сохраняет код как поисковое содержимое.
6. `table` сохраняет имена параметров и их описания.

Метаданные извлекаются независимо от основного текста:

```python
def meta_content(soup, name):
    node = soup.find('meta', attrs={'name': name})
    return node.get('content') if node else None

title_node = soup.select_one('h1') or soup.select_one('title')
canonical = soup.find('link', rel='canonical')

document = {
    'source_id': meta_content(soup, 'document_id') or raw['url'],
    'title': title_node.get_text(' ', strip=True) if title_node else '',
    'url': canonical.get('href') if canonical else raw['url'],
    'description': meta_content(soup, 'description'),
    'author': meta_content(soup, 'author'),
    'date': meta_content(soup, 'ms.date'),
}
```

Это разделение важно: metadata полезна для фильтрации и будущего зонного поиска,
но её повторение внутри основного текста исказило бы частоты терминов.

### Результаты выборки

| Показатель | Значение |
|---|---:|
| Документов | 30 |
| Сырой размер | 1 770 272 байта |
| Размер выделенного текста | 307 385 байт |
| Средний сырой документ | 59 009,07 байта |
| Средний текст | 10 246,17 байта |
| Среднее количество слов | 1 474,33 |

### Существующий поиск

Можно использовать поиск Microsoft Learn или Google с ограничением `site:`.
Примеры:

- `site:learn.microsoft.com vector search HNSW`;
- `site:learn.microsoft.com python async memory`.

Недостатки: выдача зависит от внешнего ранжирования, может смешивать продукты и
версии, а единый поиск с Wikipedia и arXiv отсутствует.

## 7. Почему эти источники подходят для следующих работ

1. У каждого документа есть устойчивый идентификатор и URL.
2. Все источники имеют существующий поиск, поэтому выполняется условие задания.
3. Документы содержат большой объём естественного и технического текста.
4. Источники дают разные HTML-структуры, что позволяет проверить качество
   выделения текста.
5. Документы связаны общей технической тематикой, но не являются дубликатами друг
   друга.
6. Есть способы массового обнаружения URL: категории API, Atom API и sitemap.

## 8. Что показать преподавателю

1. Открыть каждый ноутбук и показать описание источника.
2. Показать код получения документов и паузы между запросами.
3. Показать один сырой документ и выделенный текст.
4. Показать таблицу статистики и график.
5. Открыть ссылки на существующие поисковики и объяснить их ограничения.
6. Сравнить средний объём: arXiv заметно длиннее Microsoft Learn и Wikipedia.

## 9. Ограничения анализа

- 30 документов на источник — исследовательская выборка, а не полный корпус.
- Выборка отражает состояние источников на дату запуска.
- На arXiv формулы переводятся в линейный текст с потерей части структуры.
- В ноутбуке Wikipedia используется API `extract`, а массовый crawler сохраняет
  HTML; это осознанное различие между анализом примера и рабочим конвейером.

## 10. Как формируется выборка и где возможен bias

### Wikipedia

Сначала запрашивается до 30 кандидатов каждой категории. Затем цикл идёт по
позициям и по очереди берёт кандидата каждой категории. Это round-robin:

```python
for position in range(30):
    for category in CATEGORIES:
        pages = candidates_by_category[category]
        if position >= len(pages):
            continue
        page_id = pages[position]['pageid']
        if page_id not in seen_page_ids:
            candidates.append((page_id, category))
```

Так одна большая категория не заполняет всю выборку. `seen_page_ids` нужен,
потому что одна статья может одновременно входить в несколько категорий.

Bias: MediaWiki возвращает определённый порядок членов категории, а не случайную
выборку всего раздела. Поэтому 30 документов подходят для исследования формата,
но не для статистического вывода обо всей Wikipedia.

### arXiv

Берутся последние работы трёх категорий с сортировкой по `submittedDate`. По 10
документов каждой категории обеспечивают баланс, но создают временной bias: выборка
характеризует свежие статьи, а не всю историю arXiv.

### Microsoft Learn

URL группируются по продуктам, затем выбираются round-robin. Это уменьшает риск
получить 30 однотипных API pages. Однако список `SAMPLE_PRODUCTS` всё равно задаёт
ручное ограничение продуктового пространства.

## 11. Что именно считается «сырым размером»

Показатели разных источников нужно интерпретировать аккуратно:

| Источник | Что является raw document в ноутбуке |
|---|---|
| Wikipedia | JSON API с текстовым `extract` и metadata |
| arXiv | полный HTML LaTeXML |
| Microsoft Learn | полный HTML страницы |

Поэтому высокий коэффициент `text_size/raw_size` Wikipedia нельзя напрямую
сравнивать с arXiv: Wikipedia API уже выполнил часть очистки.

Расчёт размера всегда производится по UTF-8 bytes:

```python
raw_size = len(raw_html.encode('utf-8'))
text_size = len(text.encode('utf-8'))
```

`len(text)` без encode считал бы Unicode code points, а не байты. Для оценки
хранения и сетевого объёма нужны именно байты.

## 12. Как проверить качество выделения текста вручную

Для каждого источника взять минимум три точки документа:

1. начало — title и первый содержательный абзац;
2. середина — heading, paragraph, list/table/code;
3. конец — последняя содержательная секция перед references/footer.

Затем проверить:

| Проверка | Хороший результат |
|---|---|
| Слова на границе inline-тегов | не склеены |
| Heading и paragraph | идут в исходном порядке |
| Navigation | отсутствует |
| Повторный outline/TOC | отсутствует |
| Code и параметры Learn | присутствуют |
| References arXiv | отсутствуют по принятому решению |
| Формула arXiv | видима один раз, без дублирующей annotation |
| Unicode spaces | превращены в нормальный пробел |

Пример ручной диагностики в ноутбуке:

```python
sample = documents[0]
print(sample['title'])
print('raw bytes:', sample['raw_size'])
print('text bytes:', sample['text_size'])
print('words:', sample['word_count'])
print(sample['text'][:1500])
print(sample['text'][-1500:])
```

## 13. Как рассчитываются показатели

Для документа `i`:

```text
raw_size_i  = bytes(raw_i, UTF-8)
text_size_i = bytes(extracted_text_i, UTF-8)
word_count_i = число частей text.split()
```

Для выборки из `N=30`:

```text
total_raw = Σ raw_size_i
total_text = Σ text_size_i
average_raw = total_raw / N
average_text = total_text / N
average_words = Σ word_count_i / N
```

`word_count` ноутбука является простой оценкой по пробелам. В следующих работах
используется более строгий C++ tokenizer, поэтому числа могут немного различаться.

## 14. Почему существующий поиск обязателен

Условие преподавателя защищает от непроверяемого корпуса. Если по данным уже нельзя
найти документы никаким внешним способом, трудно:

- сформировать осмысленные тестовые запросы;
- получить baseline выдачи;
- вручную оценить релевантность;
- сравнить будущую систему с существующим доступом.

Внешний поиск не должен совпадать с будущим алгоритмом. Его задача — подтвердить,
что корпус является реальным поисковым пространством.

## 15. Подробные контрольные вопросы

### Почему не взять только arXiv?

Тогда корпус был бы более однородным, но система проверялась бы только на длинных
научных статьях. Три источника дают разные длины, metadata и HTML, что лучше
проверяет parser. Недостаток — более широкая тематика и неодинаковый стиль.

### Почему минимум три источника, если требуется два?

Это делает корпус уникальнее и даёт три разных механизма discovery: API categories,
Atom API и sitemap. При этом усложняется очистка и поддержка.

### Почему сохранять metadata отдельно, если она есть в HTML?

Metadata структурирована и пригодна для фильтров, зонного поиска и обновлений.
Если смешать её с текстом, author/date/navigation будут влиять на частоты.

### Можно ли считать 30 документов полным корпусом?

Нет. Это анализ примеров по заданию. Массовый корпус создаёт crawler. Выборка
нужна, чтобы до массовой загрузки проверить формат, длину и применимость источника.

### Как понять, что extraction не слишком агрессивен?

Сравнить начало, середину и конец с исходной страницей; убедиться, что headings,
paragraphs, lists, code/table сохраняются, а navigation повторения исчезают. Также
следить за отношением text/raw и за документами с неожиданным нулевым текстом.


