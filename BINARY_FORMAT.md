# Формат бинарного индекса

Файлы находятся в `results/boolean_index`. Текущая версия формата - `4`. Числа записываются в native little-endian представлении среды x86-64.

| Файл | Назначение |
|---|---|
| `lexicon.bin` | Заголовок индекса и адреса термов и posting lists. |
| `terms.bin` | UTF-8-байты всех термов. |
| `postings.bin` | Сжатые списки документов и частот термов. |
| `documents.bin` | Соответствие внутреннего `docID` и MongoDB ObjectId. |
| `norms.bin` | Нормы документов для TF-IDF. |
| `skips.bin` | Разреженные указатели для ускорения булевой операции `AND`. |

## `lexicon.bin`

Первые 32 байта - заголовок `IndexHeader`:

| Смещение | Размер | Тип | Значение |
|---:|---:|---|---|
| 0 | 8 | `char[8]` | `magic` - сигнатура `MAIIRIDX`. |
| 8 | 4 | `uint32_t` | `version` - версия формата. |
| 12 | 4 | `uint32_t` | `document_count` - число документов. |
| 16 | 8 | `uint64_t` | `term_count` - число уникальных термов. |
| 24 | 8 | `uint64_t` | `posting_count` - число пар «терм - документ». |

Затем следуют записи термов `LexiconEntry` размером 24 байта:

| Относительное смещение | Размер | Тип | Значение |
|---:|---:|---|---|
| 0 | 8 | `uint64_t` | `term_offset` - смещение терма в `terms.bin`. |
| 8 | 8 | `uint64_t` | `postings_offset` - смещение posting list в `postings.bin`. |
| 16 | 4 | `uint32_t` | `term_length` - длина терма в байтах UTF-8. |
| 20 | 4 | `uint32_t` | `postings_count` - число документов в posting list (`df`). |

Запись терма с номером `i` начинается по смещению `32 + i × 24`.

## `terms.bin`

Заголовка и разделителей нет. Все термы записаны подряд в UTF-8:

```text
clouddatabasequeryretrieval...
```

Границы терма задаются полями `term_offset` и `term_length` из `lexicon.bin`:

```text
term = terms.bin[term_offset : term_offset + term_length]
```

## `postings.bin`

Заголовка нет. Для каждого терма последовательно записаны пары переменной длины:

| Поле | Кодирование | Значение |
|---|---|---|
| `docID-gap` | Variable Byte | Разность текущего и предыдущего `docID`; для первого документа равна его `docID`. |
| `term_frequency` | Variable Byte | Частота терма в документе. |

```text
VariableByte(docID-gap), VariableByte(term_frequency), ...
```

Начало списка и число пар хранятся в соответствующей записи `lexicon.bin`.

## `documents.bin`

Первые 16 байт - заголовок `DocumentHeader`:

| Смещение | Размер | Тип | Значение |
|---:|---:|---|---|
| 0 | 8 | `char[8]` | `magic` - сигнатура `MAIIRDOC`. |
| 8 | 4 | `uint32_t` | `version` - версия формата. |
| 12 | 4 | `uint32_t` | `document_count` - число документов. |

После заголовка находятся 12-байтовые MongoDB ObjectId без дополнительных полей:

```text
offset(ObjectId) = 16 + docID × 12
```

## `skips.bin`

Файл хранит указатели только для длинных posting lists. Первые 40 байт -
заголовок `SkipHeader`:

| Смещение | Размер | Тип | Значение |
|---:|---:|---|---|
| 0 | 8 | `char[8]` | `magic` - сигнатура `MAIIRSKP`. |
| 8 | 4 | `uint32_t` | `version` - версия формата. |
| 12 | 4 | `uint32_t` | `document_count` - число документов в индексе. |
| 16 | 8 | `uint64_t` | `term_count` - общее число термов в `lexicon.bin`. |
| 24 | 8 | `uint64_t` | `skip_term_count` - число термов, для которых созданы указатели. |
| 32 | 8 | `uint64_t` | `skip_entry_count` - общее число указателей. |

Значение `skip_term_count` записано в заголовке `SkipHeader` по смещению
24. Сразу после 40-байтового заголовка следуют ровно `skip_term_count` записей
`SkipTermEntry` размером 24 байта:

| Относительное смещение | Размер | Тип | Значение |
|---:|---:|---|---|
| 0 | 8 | `uint64_t` | `term_index` - порядковый номер терма в `lexicon.bin`. |
| 8 | 8 | `uint64_t` | `first_skip` - номер первого указателя этого терма. |
| 16 | 4 | `uint32_t` | `skip_count` - число указателей терма. |
| 20 | 4 | `uint32_t` | `stride` - шаг `floor(sqrt(P))`, где `P` - длина posting list терма. |

Значение `skip_entry_count` записано в `SkipHeader` по смещению 32. После
таблицы `SkipTermEntry` находятся ровно `skip_entry_count` записей
`SkipEntry` размером 8 байт:

| Относительное смещение | Размер | Тип | Значение |
|---:|---:|---|---|
| 0 | 4 | `uint32_t` | `posting_index` - позиция целевого `docID` в декодированном posting list, отсчитывается от нуля. |
| 4 | 4 | `uint32_t` | `document_id` - абсолютный `docID` в этой позиции. |

Начало общей таблицы `SkipEntry`:

```text
entries_begin = 40 + skip_term_count × 24
```

Для терма из `SkipTermEntry` его указатели читаются по смещению:

```text
offset = entries_begin + first_skip × 8
```

В `skips.bin` нет копии posting lists. Он хранит только позицию прыжка и соответствующий
`docID`. Файл используется булевой операцией `AND`; TF-IDF его не читает.

## `norms.bin`

Первые 16 байт - заголовок `DocumentNormHeader`:

| Смещение | Размер | Тип | Значение |
|---:|---:|---|---|
| 0 | 8 | `char[8]` | `magic` - сигнатура `MAIIRNRM`. |
| 8 | 4 | `uint32_t` | `version` - версия формата. |
| 12 | 4 | `uint32_t` | `document_count` - число документов. |

После заголовка находится одно значение `double` размером 8 байт на документ:

```text
offset(norm) = 16 + docID × 8
```
