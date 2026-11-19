# Пример 5. Работа цитатного поиска

Рассмотрим точную цитату из трёх термов:

```text
"cloud database security"
```

Цель примера - показать весь путь от обработанных текстов до итоговых
документов: назначение `docID`, построение координат, запись бинарных файлов,
загрузка posting lists и последовательное позиционное пересечение.

Чтобы бинарные таблицы можно было полностью посчитать вручную, в учебном
индексе ниже оставлены только три терма запроса: `cloud`, `database` и
`security`. Остальные слова сохраняют свои места в последовательности и поэтому
влияют на координаты, но их собственные posting lists в таблицах примера не
показываются.

## 1. Исходные документы

Пусть в `mai_ir.processed_documents` находятся четыре принятых документа:

```javascript
{
  _id: ObjectId("671a00000000000000000001"),
  accepted: true,
  text: "Cloud database security improves reliability"
}

{
  _id: ObjectId("671a00000000000000000002"),
  accepted: true,
  text: "Cloud security database platform"
}

{
  _id: ObjectId("671a00000000000000000003"),
  accepted: true,
  text: "Cloud distributed database security"
}

{
  _id: ObjectId("671a00000000000000000004"),
  accepted: true,
  text: "Cloud database security and cloud database security"
}
```

Документы специально подобраны так, чтобы показать разные ситуации:

| Документ | Особенность |
|---:|---|
| 0 | Цитата встречается один раз. |
| 1 | Все три слова есть, но расположены в другом порядке. |
| 2 | `database security` стоят рядом, но перед ними нет соседнего `cloud`. |
| 3 | Цитата встречается два раза. |

## 2. Назначение внутренних `docID`

Булев индекс сортирует документы по MongoDB ObjectId и записывает их в
`documents.bin`. Координатный индекс читает документы в том же порядке и
проверяет каждый ObjectId.

Получается соответствие:

| `docID` | MongoDB ObjectId |
|---:|---|
| 0 | `671a00000000000000000001` |
| 1 | `671a00000000000000000002` |
| 2 | `671a00000000000000000003` |
| 3 | `671a00000000000000000004` |

В `documents.bin` сохраняются только 12 байт каждого ObjectId. Позиция записи
определяется непосредственно по `docID`:

```text
offset(ObjectId) = 16 + docID * 12
```

## 3. Токенизация, лемматизация и координаты

Каждый текст проходит `linguistics_normalize_sequence`. Порядок лемм не
сортируется, а координата равна номеру леммы в последовательности, начиная с
нуля.

### Документ 0

```text
Текст:  Cloud database security improves reliability
Леммы: [cloud, database, security, improve, reliability]
```

| Позиция | Лемма |
|---:|---|
| 0 | `cloud` |
| 1 | `database` |
| 2 | `security` |
| 3 | `improve` |
| 4 | `reliability` |

Цитата начинается в позиции 0:

```text
cloud(0), database(1), security(2)
```

### Документ 1

```text
Текст:  Cloud security database platform
Леммы: [cloud, security, database, platform]
```

Все термы запроса присутствуют, но координаты имеют неправильный порядок:

```text
cloud(0), security(1), database(2)
```

Булев запрос `cloud AND database AND security` вернул бы этот документ, а
цитатный поиск должен его исключить.

### Документ 2

```text
Текст:  Cloud distributed database security
Леммы: [cloud, distributed, database, security]
```

Координаты:

```text
cloud(0), distributed(1), database(2), security(3)
```

Слова `database security` образуют точную двухсловную последовательность, но
полной цитаты нет: перед `database(2)` находится `distributed(1)`, а не
`cloud(1)`.

### Документ 3

```text
Текст:  Cloud database security and cloud database security
Леммы: [cloud, database, security, and, cloud, database, security]
```

Цитата начинается дважды:

```text
cloud(0), database(1), security(2)
cloud(4), database(5), security(6)
```

Начальные позиции равны `[0, 4]`.

## 4. Временные записи построителя

Во время построения для каждого вхождения создаётся запись:

```text
(term, docID, position)
```

Для трёх интересующих термов записи выглядят так:

```text
(cloud,    0, 0)
(database, 0, 1)
(security, 0, 2)

(cloud,    1, 0)
(security, 1, 1)
(database, 1, 2)

(cloud,    2, 0)
(database, 2, 2)
(security, 2, 3)

(cloud,    3, 0)
(database, 3, 1)
(security, 3, 2)
(cloud,    3, 4)
(database, 3, 5)
(security, 3, 6)
```

После сортировки по терму, `docID` и позиции одинаковые термы оказываются
рядом. Из них формируются координатные posting lists.

## 5. Логическое представление координатного индекса

Для `cloud`:

```text
cloud ->
    doc 0: [0]
    doc 1: [0]
    doc 2: [0]
    doc 3: [0, 4]
```

Для `database`:

```text
database ->
    doc 0: [1]
    doc 1: [2]
    doc 2: [2]
    doc 3: [1, 5]
```

Для `security`:

```text
security ->
    doc 0: [2]
    doc 1: [1]
    doc 2: [3]
    doc 3: [2, 6]
```

Каждый терм встречается в четырёх документах, поэтому для всех трёх:

```text
postings_count = 4
position_count = 5
```

`postings_count` считает документы, а `position_count` - отдельные вхождения.
В документе 3 каждый терм встречается дважды, поэтому число позиций больше
числа документов.

## 6. `coordinate_terms.bin`

Термы сортируются лексикографически и записываются подряд:

```text
clouddatabasesecurity
```

| Терм | `term_offset` | `term_length` |
|---|---:|---:|
| `cloud` | 0 | 5 |
| `database` | 5 | 8 |
| `security` | 13 | 8 |

Размер файла для этих трёх термов равен:

```text
5 + 8 + 8 = 21 байт
```

Заголовка и разделителей в `coordinate_terms.bin` нет. Границы строк задаются
записями `coordinate_lexicon.bin`.

## 7. Содержимое `coordinates.bin`

Для каждого документа сохраняются логические значения:

```text
docID-gap, position_count, first_position, position-gap, ...
```

Все числа примера меньше 128, поэтому после Variable Byte-кодирования каждое
занимает один байт.

### Список `cloud`

```text
doc 0: docID-gap=0, count=1, positions=[0]
       -> [0, 1, 0]

doc 1: docID-gap=1, count=1, positions=[0]
       -> [1, 1, 0]

doc 2: docID-gap=1, count=1, positions=[0]
       -> [1, 1, 0]

doc 3: docID-gap=1, count=2, positions=[0, 4]
       -> [1, 2, 0, 4]
```

Размер списка:

```text
3 + 3 + 3 + 4 = 13 байт
```

### Список `database`

```text
doc 0 -> [0, 1, 1]
doc 1 -> [1, 1, 2]
doc 2 -> [1, 1, 2]
doc 3 -> [1, 2, 1, 4]
```

Последняя запись означает абсолютные позиции `[1, 5]`:

```text
first_position = 1
position-gap   = 4
1 + 4          = 5
```

Размер списка также равен 13 байтам.

### Список `security`

```text
doc 0 -> [0, 1, 2]
doc 1 -> [1, 1, 1]
doc 2 -> [1, 1, 3]
doc 3 -> [1, 2, 2, 4]
```

Размер списка равен 13 байтам.

Полный размер `coordinates.bin` для рассматриваемых термов:

```text
13 + 13 + 13 = 39 байт
```

Реальные байты Variable Byte имеют установленный старший завершающий бит. В
таблицах показаны исходные целые значения до кодирования, чтобы было видно их
смысл.

## 8. `coordinate_lexicon.bin`

Заголовок `Header` занимает 40 байт:

```text
document_count = 4
term_count     = 3
posting_count  = 12
position_count = 15
```

Значения получаются так:

```text
posting_count  = 4 + 4 + 4 = 12 пар «терм - документ»
position_count = 5 + 5 + 5 = 15 отдельных координат
```

После заголовка записываются три структуры `LexiconEntry` размером 32 байта:

| Терм | `term_offset` | `postings_offset` | `term_length` | `postings_count` | `position_count` |
|---|---:|---:|---:|---:|---:|
| `cloud` | 0 | 0 | 5 | 4 | 5 |
| `database` | 5 | 13 | 8 | 4 | 5 |
| `security` | 13 | 26 | 8 | 4 | 5 |

Например, запись `database` сообщает:

```text
строка терма начинается с байта 5 в coordinate_terms.bin;
длина строки равна 8 байтам;
координатный список начинается с байта 13 в coordinates.bin;
нужно декодировать 4 документа и суммарно 5 позиций.
```

Размер `coordinate_lexicon.bin`:

```text
40 байт Header + 3 * 32 байта LexiconEntry = 136 байт
```

## 9. Нормализация запроса

Пользователь вводит:

```text
"cloud database security"
```

Внешние кавычки удаляются. Затем запрос проходит тот же pipeline, что и
документы:

```text
токенизация  -> [cloud, database, security]
лемматизация -> [cloud, database, security]
```

Порядок и повторы сохраняются. Поиск не превращает цитату в множество термов.

Для каждой леммы двоичный поиск по `coordinate_lexicon.bin` находит запись и
получает:

- диапазон строки в `coordinate_terms.bin`;
- начало списка в `coordinates.bin`;
- число документов;
- контрольное число координат.

Полностью весь `coordinates.bin` не декодируется. Читаются только три диапазона
для `cloud`, `database` и `security`.

## 10. Первое пересечение: `cloud database`

Сначала сравниваются `docID` списков `cloud` и `database`. В этом примере оба
терма есть во всех четырёх документах, поэтому для каждого документа
сравниваются позиции.

### Документ 0

```text
cloud:    [0]
database: [1]

0 + 1 = 1 -> совпадение
```

Сохраняется позиция `1`, на которой заканчивается промежуточная цитата
`cloud database`.

### Документ 1

```text
cloud:    [0]
database: [2]

0 + 1 != 2 -> совпадения нет
```

Документ исключается уже на первом шаге.

### Документ 2

```text
cloud:    [0]
database: [2]

0 + 1 != 2 -> совпадения нет
```

Между термами находится слово `distributed`, поэтому документ исключается.

### Документ 3

```text
cloud:    [0, 4]
database: [1, 5]

0 + 1 = 1 -> совпадение
4 + 1 = 5 -> совпадение
```

Сохраняются конечные позиции `[1, 5]`.

После первого пересечения промежуточный список равен:

```text
cloud database ->
    doc 0: [1]
    doc 3: [1, 5]
```

Важно: это пока не начальные позиции. Это позиции последнего терма уже
совпавшей части цитаты.

## 11. Второе пересечение: добавление `security`

Теперь промежуточный результат пересекается со списком `security`.

Документы 1 и 2 уже отсутствуют, поэтому их позиции больше не рассматриваются.

### Документ 0

```text
концы cloud database: [1]
security:             [2]

1 + 1 = 2 -> совпадение
```

Сохраняется конечная позиция полной цитаты `2`.

### Документ 3

```text
концы cloud database: [1, 5]
security:             [2, 6]

1 + 1 = 2 -> совпадение
5 + 1 = 6 -> совпадение
```

Сохраняются конечные позиции `[2, 6]`.

После обработки всех трёх термов:

```text
полная цитата ->
    doc 0: конечные позиции [2]
    doc 3: конечные позиции [2, 6]
```

## 12. Получение начальных позиций

Длина цитаты равна трём термам. Чтобы перейти от конечной позиции к начальной,
вычитается:

```text
phrase_length - 1 = 3 - 1 = 2
```

Для документа 0:

```text
2 - 2 = 0
```

Для документа 3:

```text
2 - 2 = 0
6 - 2 = 4
```

Итоговый результат алгоритма:

```text
doc 0: start_positions = [0]
doc 3: start_positions = [0, 4]
```

Количество разных документов:

```text
matched_documents = 2
```

Общее количество вхождений:

```text
occurrence_count = 1 + 2 = 3
```

## 13. Почему документы 1 и 2 не попали в результат

Документ 1 содержит все три терма:

```text
cloud security database
```

Но цитата требует другой порядок:

```text
cloud database security
```

Документ 2 также содержит все три терма:

```text
cloud distributed database security
```

Однако между `cloud` и `database` находится дополнительный терм. Поэтому
обычный булев поиск дал бы два ложных совпадения, а координатная проверка
исключает оба документа.

Сравнение результатов:

```text
cloud AND database AND security -> [0, 1, 2, 3]
"cloud database security"       -> [0, 3]
```

## 14. Перевод `docID` обратно в MongoDB ObjectId

После позиционного пересечения поиск имеет только внутренние номера:

```text
[0, 3]
```

Через `documents.bin` восстанавливаются ObjectId:

```text
docID 0 -> ObjectId("671a00000000000000000001")
docID 3 -> ObjectId("671a00000000000000000004")
```

По ним из `processed_documents` загружаются только отображаемые поля:

```text
title
source
url
```

MongoDB не проверяет цитату и не вычисляет позиции. Вся поисковая операция уже
выполнена по бинарному координатному индексу.

## 15. Ожидаемая консольная выдача

Упрощённо результат выглядит так:

```text
Цитата: "cloud database security"
Найдено документов: 2 | вхождений: 3 | поиск: ... мс

1. Первый документ
   Источник: example | doc=0 | oid=671a00000000000000000001
   https://example.test/document-0
   Начальные позиции (с 0): 0

2. Четвёртый документ
   Источник: example | doc=3 | oid=671a00000000000000000004
   https://example.test/document-3
   Начальные позиции (с 0): 0, 4
```

Число документов равно двум, но число вхождений равно трём, потому что в
документе 3 цитата встретилась два раза.

## 16. Полная схема примера

```text
"cloud database security"
             │
             ▼
[cloud, database, security]
             │
             ├── cloud    -> docs 0,1,2,3
             ├── database -> docs 0,1,2,3
             └── security -> docs 0,1,2,3
             │
             ▼
cloud + database, проверка разности позиций 1
             │
             ├── doc 0: [1]
             └── doc 3: [1,5]
             │
             ▼
добавление security, снова разность позиций 1
             │
             ├── doc 0: [2]
             └── doc 3: [2,6]
             │
             ▼
вычитание длины цитаты минус один
             │
             ├── doc 0: start [0]
             └── doc 3: start [0,4]
             │
             ▼
documents.bin: docID -> ObjectId
             │
             ▼
MongoDB: title, source, url
```

## 17. Как запустить настоящий поиск

Если координатный индекс ещё не построен:

```bash
./.build/boolean_index configs/boolean_index.yaml
./.build/positional_index configs/positional_index.yaml
```

Запуск консольной утилиты:

```bash
./.build/phrase_search configs/search.yaml
```

После запуска ввести цитату:

```text
"cloud database security"
```

Пакетный вариант через стандартный ввод:

```bash
printf '%s\n' '"cloud database security"' \
  | ./.build/phrase_search configs/search.yaml
```

Тесты координатного индекса и поиска:

```bash
ctest --test-dir .build \
  -R 'unit\.(positional_index|phrase_search)\.' \
  --output-on-failure
```

Главный вывод из примера: совпадения по `docID` недостаточно. Документ считается
результатом цитатного поиска только тогда, когда для всей последовательности
существуют координаты `p`, `p + 1`, `p + 2`, ..., соответствующие порядку
термов запроса.
