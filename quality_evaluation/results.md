# Выдачи и оценки релевантности

Первые пять позиций. Оценки выполнены языковой моделью по сохранённым текстовым фрагментам; это проверяемая первичная разметка, а не независимый человеческий эталон.

Bing приведён отдельно: после фильтрации доменов его первая страница неполна. Не следует считать её метрики оценкой всей поисковой системы Bing.

## Q01. database transaction isolation

Найти объяснение уровней изоляции транзакций и возникающих аномалий.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [2.1.1.18 E152, Basic SET TRANSACTION statement](<https://learn.microsoft.com/en-us/openspecs/sql_standards/ms-tsqliso02/5383babc-27c4-4f51-ae11-5c4df8f0d49c>) | 2 | Приводит синтаксис установки изоляции и перечень четырёх уровней SQL. |
| 2 | [Transaction Isolation](<https://learn.microsoft.com/en-us/sql/odbc/reference/develop-app/transaction-isolation?view=sql-server-ver17>) | 1 | Короткая обзорная страница в основном содержит ссылки на дальнейшее чтение. |
| 3 | [3.2.7.31 Set Transaction Attributes](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-dtco/98d9764b-dc5c-4ea7-b049-66f434c950c1>) | 1 | Описывает поля протокольного объекта транзакции, не объясняя смысл уровней. |
| 4 | [Cursor Transaction Isolation Level](<https://learn.microsoft.com/en-us/sql/relational-databases/native-client-odbc-cursors/properties/cursor-transaction-isolation-level?view=sql-server-ver15>) | 2 | Объясняет применение уровня изоляции к блокировкам курсоров ODBC. |
| 5 | [Setting the Transaction Isolation Level](<https://learn.microsoft.com/en-us/windows/win32/cossdk/setting-the-transaction-isolation-level>) | 1 | Инструкция для COM+ показывает настройку, но не сравнение уровней и аномалий. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Isolation (database systems)](<https://en.wikipedia.org/wiki/Isolation_(database_systems)>) | 3 | Специальная статья об изоляции и феноменах параллельного выполнения транзакций. |
| 2 | [Database transaction](<https://en.wikipedia.org/wiki/Database_transaction>) | 1 | Объясняет транзакции и назначение изоляции, но не раскрывает уровни. |
| 3 | [Snapshot isolation](<https://en.wikipedia.org/wiki/Snapshot_isolation>) | 2 | Подробно объясняет один механизм изоляции — согласованный снимок. |
| 4 | [ACID](<https://en.wikipedia.org/wiki/ACID>) | 1 | Изоляция рассматривается как часть ACID, без полного разбора уровней и аномалий. |
| 5 | [Atomicity (database systems)](<https://en.wikipedia.org/wiki/Atomicity_(database_systems)>) | 1 | Основная тема — атомарность; изоляция упомянута как соседнее свойство. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Entity Framework documentation hub](<https://learn.microsoft.com/en-us/ef/>) | 0 | Навигация по Entity Framework не содержит ответа об изоляции транзакций. |
| 2 | [SET TRANSACTION ISOLATION LEVEL (Transact-SQL) - SQL Server](<https://learn.microsoft.com/en-us/sql/t-sql/statements/set-transaction-isolation-level-transact-sql>) | 3 | Подробный справочник уровней изоляции SQL Server и их поведения при чтении и записи. |
| 3 | [What is the Azure SQL Database service? - Azure SQL Database](<https://learn.microsoft.com/en-us/azure/azure-sql/database/sql-database-paas-overview>) | 1 | Обзор Azure SQL Database касается хранения и резервных копий, но не уровней изоляции. |
| 4 | [Handling Concurrency Conflicts - EF Core](<https://learn.microsoft.com/en-us/ef/core/saving/concurrency>) | 2 | Разбирает конфликты параллельного доступа и влияние уровня изоляции на блокировки. |
| 5 | [Deadlocks Guide - SQL Server](<https://learn.microsoft.com/en-us/sql/relational-databases/sql-server-deadlocks-guide>) | 2 | Объясняет взаимоблокировки и откат транзакций как важный аспект конкурентного доступа. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Boosting End-to-End Database Isolation Checking via Mini-Transactions (Extended Version)](<https://arxiv.org/abs/2504.02344v1>) | 2 | Исследует проверку сильных уровней изоляции и нарушения гарантий; это отдельный механизм, а не общий разбор всех уровней и аномалий. |
| 2 | [Lockless Transaction Isolation in Hyperledger Fabric](<https://arxiv.org/abs/1911.12711v1>) | 2 | Предлагает версионную snapshot isolation для транзакций Hyperledger Fabric; полезный частный сценарий. |
| 3 | [Online Timestamp-based Transactional Isolation Checking of Database Systems (Extended Version)](<https://arxiv.org/abs/2504.01477v1>) | 2 | Рассматривает онлайн-проверку serializability и snapshot isolation по истории транзакций. |
| 4 | [VerIso: Verifiable Isolation Guarantees for Database Transactions](<https://arxiv.org/abs/2503.06284v1>) | 2 | Формально проверяет гарантии изоляции и показывает контрпримеры протокола; даёт содержательную часть ответа. |
| 5 | [AWDIT: An Optimal Weak Database Isolation Tester](<https://arxiv.org/abs/2504.06975v1>) | 2 | Исследует тестирование слабых уровней изоляции, без обзорного объяснения всего набора уровней. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Database - Wikipedia](<https://en.wikipedia.org/wiki/Database>) | 1 | Обзор баз данных даёт лишь общий контекст транзакций и ACID. |

В сохранённой выборке только 1 результатов.

## Q02. distributed database replication

Найти способы репликации распределённых баз данных и их компромиссы.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [4.1.11.1.5 NT4_REPLICATION_STATE](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-drsr/a7d8f924-3fc0-4dc1-89f3-59f4521cf0e7>) | 0 | Структура состояния SAM Windows NT не объясняет способы репликации распределённых БД. |
| 2 | [Replication Views (Transact-SQL)](<https://learn.microsoft.com/en-us/sql/relational-databases/system-views/replication-views-transact-sql?view=sql-server-ver17>) | 1 | Перечень системных представлений репликации полезен как справка, но не объясняет способы и компромиссы. |
| 3 | [Database Mirroring: Interoperability and Coexistence (SQL Server)](<https://learn.microsoft.com/en-us/sql/database-engine/database-mirroring/database-mirroring-interoperability-and-coexistence-sql-server?view=sql-server-ver17>) | 2 | Описывает совместимость зеркалирования SQL Server и ограничения распределённых транзакций. |
| 4 | [2.10 Additional Considerations](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fsmod/6d322de5-cd61-4c95-a2c3-540933e75cd1>) | 0 | Таблица транспортов файловых RPC-протоколов не решает задачу поиска. |
| 5 | [DFSR Module](<https://learn.microsoft.com/en-us/powershell/module/dfsr?view=windowsserver2025-ps>) | 0 | Каталог команд репликации файлов DFS не объясняет репликацию распределённых БД. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Distributed database](<https://en.wikipedia.org/wiki/Distributed_database>) | 3 | Основная тема — распределённая БД; непосредственно рассматриваются репликация и фрагментация. |
| 2 | [Replication (computing)](<https://en.wikipedia.org/wiki/Replication_(computing)>) | 3 | Описывает модели репликации, включая транзакционную, и свойства распределённых систем. |
| 3 | [Spanner (database)](<https://en.wikipedia.org/wiki/Spanner_(database)>) | 2 | Конкретный пример глобальной репликации и согласованности в Spanner/Paxos. |
| 4 | [Database](<https://en.wikipedia.org/wiki/Database>) | 1 | Общий обзор БД содержит краткий раздел о репликации, но не систематический разбор. |
| 5 | [Distributed SQL](<https://en.wikipedia.org/wiki/Distributed_SQL>) | 2 | Разбирает распределённые SQL-БД с синхронной репликацией и согласованностью. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Configure the Windows Firewall to Allow SQL Server Access - SQL Server](<https://learn.microsoft.com/en-us/sql/sql-server/install/configure-the-windows-firewall-to-allow-sql-server-access>) | 1 | Настройка портов SQL Server полезна для эксплуатации, но не объясняет компромиссы репликации. |
| 2 | [Editions and Supported Features of SQL Server 2025 - SQL Server](<https://learn.microsoft.com/en-us/sql/sql-server/editions-and-components-of-sql-server-2025>) | 1 | Сравнение функций редакций SQL Server даёт продуктовый контекст, не механизмы репликации. |
| 3 | [Azure Cosmos DB documentation - Azure Cosmos DB](<https://learn.microsoft.com/en-us/azure/cosmos-db/>) | 1 | Документационный хаб Cosmos DB даёт навигацию к распределённой БД, но не объясняет репликацию. |
| 4 | [Service overview and network port requirements - Windows Server](<https://learn.microsoft.com/en-us/troubleshoot/windows-server/networking/service-overview-and-network-port-requirements>) | 0 | Сетевые порты и репликация файлов DFSR — другая задача. |
| 5 | [Editions and Supported Features of SQL Server 2019 - SQL Server](<https://learn.microsoft.com/en-us/sql/sql-server/editions-and-components-of-sql-server-2019>) | 1 | Матрица редакций показывает доступность функций, а не способы репликации. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Coded Data Rebalancing for Decentralized Distributed Databases](<https://arxiv.org/abs/2010.11935v2>) | 2 | Разбирает сохранение репликационного фактора и затраты обмена при перебалансировке распределённого хранения. |
| 2 | [Providing an Object Allocation Algorithm in Distributed Databases Using Efficient Factors](<https://arxiv.org/abs/1210.1745v1>) | 2 | Предлагает алгоритм репликации объектов и сравнивает стоимость обслуживания чтений и записей. |
| 3 | [Distributed Metadata with the AMGA Metadata Catalog](<https://arxiv.org/abs/cs/0604071v1>) | 2 | Описывает механизмы распределения и репликации метаданных на конкретном каталоге AMGA. |
| 4 | [Fault-Tolerant Partial Replication in Large-Scale Database Systems](<https://arxiv.org/abs/0802.0137v3>) | 2 | Объясняет частичную репликацию, фиксацию транзакций и компромисс с масштабируемостью и отказоустойчивостью. |
| 5 | [Sharding Distributed Databases: A Critical Review](<https://arxiv.org/abs/2404.04384v2>) | 3 | Обзор распределённых репликационных систем связывает шардирование, консенсус и ограничения масштабирования. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|

В сохранённой выборке только 0 результатов.

## Q03. inverted index information retrieval

Найти устройство инвертированного индекса и его применение в поиске.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [1.3 Overview](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-mcis/7968c17c-fe89-48e6-86b4-c67f6178928e>) | 2 | Прямо связывает каталог с инвертированным индексом для сопоставления слов и извлечения документов. |
| 2 | [Inverted index](<https://en.wikipedia.org/wiki/Inverted_index>) | 3 | Даёт определение и структуру отображения термов в позиции и документы. |
| 3 | [Document retrieval](<https://en.wikipedia.org/wiki/Document_retrieval>) | 1 | Упоминает инвертированный индекс в контексте поиска документов без описания структуры. |
| 4 | [1.3 Overview](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-wsp/e2a93bd4-71ac-4776-bdb8-6ca602d16b59>) | 2 | Объясняет роль инвертированного индекса в службе индексирования Windows. |
| 5 | [Foundry IQ (Azure AI Search) documentation](<https://learn.microsoft.com/en-us/azure/search>) | 1 | Стартовая страница продукта содержит ссылки на поиск, но не объясняет устройство индекса. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Inverted index](<https://en.wikipedia.org/wiki/Inverted_index>) | 3 | Даёт определение и структуру отображения термов в позиции и документы. |
| 2 | [Search engine indexing](<https://en.wikipedia.org/wiki/Search_engine_indexing>) | 2 | Рассматривает поисковую индексацию и полнотекстовые индексы как отдельную задачу. |
| 3 | [Information retrieval](<https://en.wikipedia.org/wiki/Information_retrieval>) | 1 | Общий обзор информационного поиска лишь связывает разреженные модели с индексами. |
| 4 | [Document retrieval](<https://en.wikipedia.org/wiki/Document_retrieval>) | 1 | Упоминает инвертированный индекс в контексте поиска документов без описания структуры. |
| 5 | [Learned sparse retrieval](<https://en.wikipedia.org/wiki/Learned_sparse_retrieval>) | 2 | Описывает применение инвертированных индексов в современном разреженном поиске, но не базовое построение. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Study guide for Exam AI-200: Developing AI Cloud Solutions on Azure](<https://learn.microsoft.com/en-us/credentials/certifications/resources/study-guides/ai-200>) | 0 | План экзамена не является объяснением структуры поискового индекса. |
| 2 | [What is Foundry IQ? - Microsoft Foundry](<https://learn.microsoft.com/en-us/azure/foundry/agents/concepts/what-is-foundry-iq>) | 1 | Обзор сервиса поиска не раскрывает отображение термов в postings. |
| 3 | [Microsoft 365 Copilot Retrieval API Overview](<https://learn.microsoft.com/en-us/microsoft-365/copilot/extensibility/api/ai-services/retrieval/overview>) | 1 | Обзор Retrieval API описывает гибридный индекс как готовый сервис, а не его устройство. |
| 4 | [Retrieval augmented generation (RAG) and indexes in Microsoft Foundry - Microsoft Foundry](<https://learn.microsoft.com/en-us/azure/foundry/concepts/retrieval-augmented-generation>) | 1 | Объясняет роль индекса в RAG в целом, без устройства inverted index. |
| 5 | [Design and Develop a RAG Solution on Azure - Azure Architecture Center](<https://learn.microsoft.com/en-us/azure/architecture/ai-ml/guide/rag/rag-solution-design-and-evaluation-guide>) | 1 | RAG-архитектура упоминает создание поискового индекса, не его инвертированную структуру. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [COIL: Revisit Exact Lexical Match in Information Retrieval with Contextualized Inverted List](<https://arxiv.org/abs/2104.07186v1>) | 2 | COIL хранит контекстные представления термов в обратных списках и объясняет их использование для лексического поиска. |
| 2 | [Hybrid Inverted Index Is a Robust Accelerator for Dense Retrieval](<https://arxiv.org/abs/2210.05521v3>) | 2 | Описывает гибридные обратные списки термов и кластеров для ускорения retrieval, но не базовый учебный индекс. |
| 3 | [Inverted Semantic-Index for Image Retrieval](<https://arxiv.org/abs/2206.12623v1>) | 1 | Индекс предназначен для изображений и разбиения кодовой книги, а не соответствия текстовых термов документам. |
| 4 | [SEINE: SEgment-based Indexing for NEural information retrieval](<https://arxiv.org/abs/2311.15923v1>) | 1 | Предлагает сегментный нейронный индекс; классический термовый индекс служит главным образом сравнением. |
| 5 | [Efficient Inverted Indexes for Approximate Retrieval over Learned Sparse Representations](<https://arxiv.org/abs/2404.18812v1>) | 2 | Описывает организацию обратных списков sparse-термов по блокам и обработку запроса. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|

В сохранённой выборке только 0 результатов.

## Q04. C++ memory management

Найти управление памятью в C++: время жизни, RAII, выделение и освобождение.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Smart Cleanup - Achieve More Reliable Resource Management with Our Custom C++ Classes \| Microsoft Learn](<https://learn.microsoft.com/en-us/archive/msdn-magazine/authors/eric_niebler>) | 3 | Специальный материал о надёжном управлении ресурсами C++ и обёртках автоматического освобождения. |
| 2 | [Standard C Library Functions](<https://learn.microsoft.com/en-us/windows/win32/memory/standard-c-library-functions>) | 2 | Сопоставляет функции CRT и new/delete и объясняет допустимые способы управления памятью. |
| 3 | [Memory Management (Memory Management)](<https://learn.microsoft.com/en-us/windows/win32/memory/memory-management>) | 1 | Раздел Windows о виртуальной памяти даёт системный фон, но не модель объектов C++. |
| 4 | [Debug new](<https://en.wikipedia.org/wiki/Debug_new>) | 2 | Разбирает new/delete и диагностику выделений памяти в C++ как конкретный аспект задачи. |
| 5 | [C++ Code analysis in Visual Studio](<https://learn.microsoft.com/en-us/cpp/code-quality?view=msvc-170>) | 1 | Анализатор может находить дефекты памяти, но страница не объясняет её управление. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Memory management](<https://en.wikipedia.org/wiki/Memory_management>) | 1 | Общий обзор управления памятью не сосредоточен на C++ и времени жизни объектов. |
| 2 | [C dynamic memory allocation](<https://en.wikipedia.org/wiki/C_dynamic_memory_allocation>) | 1 | Описывает malloc/free языка C, полезные как фон, но не современное управление памятью C++. |
| 3 | [Manual memory management](<https://en.wikipedia.org/wiki/Manual_memory_management>) | 2 | Объясняет ручное управление и автоматизацию освобождения в C++. |
| 4 | [Region-based memory management](<https://en.wikipedia.org/wiki/Region-based_memory_management>) | 1 | Регионное выделение памяти — связанный подход, но не общее объяснение C++/RAII. |
| 5 | [Memory management unit](<https://en.wikipedia.org/wiki/Memory_management_unit>) | 0 | MMU — аппаратный перевод адресов, а не управление временем жизни объектов C++. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Entity Framework documentation hub](<https://learn.microsoft.com/en-us/ef/>) | 0 | Документация ORM не объясняет модель управления памятью C++. |
| 2 | [Microsoft Purview](<https://learn.microsoft.com/en-us/purview/>) | 0 | Управление данными Purview — другой смысл слова management. |
| 3 | [Dynamics 365 Supply Chain Management documentation](<https://learn.microsoft.com/en-us/dynamics365/supply-chain/>) | 0 | Управление цепочками поставок не связано с памятью C++. |
| 4 | [Release Notes for SQL Server Management Studio (SSMS) 20](<https://learn.microsoft.com/en-us/ssms/release-notes-20>) | 0 | Release notes SSMS относятся к другому продукту и другой задаче. |
| 5 | [Release Notes for SQL Server Management Studio (SSMS) 19 and Earlier Versions](<https://learn.microsoft.com/en-us/ssms/release-notes-19>) | 0 | Список исправлений SSMS не объясняет выделение памяти, RAII или время жизни объектов C++. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Parallel Memory Management in Goose, C++, and Rust: An Experimental Study of Allocation, Reclamation, and Scaling](<https://arxiv.org/abs/2611.32653v1>) | 2 | Сравнивает выделение и освобождение памяти в C++, Rust и Goose, включая узлы и арены; полезный практический аспект. |
| 2 | [Mesh: Compacting Memory Management for C/C++ Applications](<https://arxiv.org/abs/1902.04738v2>) | 2 | Объясняет malloc-совместимый аллокатор C/C++ и борьбу с фрагментацией памяти. |
| 3 | [Virtual-Memory Assisted Buffer Management In Tiered Memory](<https://arxiv.org/abs/2603.03271v1>) | 1 | Рассматривает виртуальную память и буферизацию баз данных, без объяснения времени жизни и RAII в C++. |
| 4 | [Rely-guarantee Reasoning about Concurrent Memory Management: Correctness, Safety and Security](<https://arxiv.org/abs/2309.09997v1>) | 0 | Посвящена формальной проверке менеджера памяти Zephyr на C, а не управлению объектами C++. |
| 5 | [Breaking Down Memory Walls: Adaptive Memory Management in LSM-based Storage Systems (Extended Version)](<https://arxiv.org/abs/2004.10360v2>) | 0 | Разбирает распределение памяти LSM-хранилища; время жизни, выделение и освобождение объектов C++ не раскрыты. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [C++ - Wikipedia](<https://en.wikipedia.org/wiki/C%2B%2B>) | 1 | Обзор языка отмечает ручное управление памятью, но не даёт цельного объяснения RAII. |
| 2 | [Latest Supported Visual C++ Redistributable Downloads](<https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170>) | 0 | Скачивание Visual C++ Runtime не отвечает на вопрос об управлении памятью. |

В сохранённой выборке только 2 результатов.

## Q05. C# asynchronous programming

Найти объяснение async/await и асинхронного программирования в C#.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [C# documentation](<https://learn.microsoft.com/en-us/dotnet/csharp/tour-of-csharp>) | 1 | Главная страница документации лишь направляет к отдельным учебным материалам. |
| 2 | [Implement asynchronous tasks](<https://learn.microsoft.com/en-us/training/modules/implement-asynchronous-tasks>) | 3 | Учебный модуль прямо разбирает async/await, параллельные задачи и асинхронный ввод-вывод в C#. |
| 3 | [Create and manage events](<https://learn.microsoft.com/en-us/training/modules/create-manage-events>) | 1 | Курс событий требует знания асинхронности, но обучает другой теме. |
| 4 | [Async Sample: Asynchronous programming with async and await in C#](<https://learn.microsoft.com/en-us/samples/dotnet/samples/async-and-await-cs>) | 3 | Пример и связанный учебник прямо посвящены async/await в C#. |
| 5 | [C# - The C# Memory Model in Theory and Practice, Part 2 \| Microsoft Learn](<https://learn.microsoft.com/en-us/archive/msdn-magazine/authors/igor_ostrovsky>) | 1 | Основной материал о модели памяти C#; ссылка на асинхронные задачи не делает его учебником async/await. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Message passing](<https://en.wikipedia.org/wiki/Message_passing>) | 1 | Обмен сообщениями — общий контекст конкурентности, но не требуемая техника C#. |
| 2 | [Asynchronous method invocation](<https://en.wikipedia.org/wiki/Asynchronous_method_invocation>) | 1 | Объясняет общий асинхронный шаблон, но не его реализацию через async/await C#. |
| 3 | [F Sharp (programming language)](<https://en.wikipedia.org/wiki/F_Sharp_(programming_language)>) | 0 | Материал про F# не решает задачу программирования async/await в C#. |
| 4 | [Asio (C++ library)](<https://en.wikipedia.org/wiki/Asio_(C++_library)>) | 0 | Основная тема — асинхронная библиотека C++, а не async/await языка C#. |
| 5 | [Async/await](<https://en.wikipedia.org/wiki/Async/await>) | 3 | Объясняет async/await и непосредственно рассматривает TAP в C#. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [.NET documentation - .NET](<https://learn.microsoft.com/en-us/dotnet/>) | 1 | Навигация .NET даёт общий контекст платформы без самостоятельного объяснения async/await. |
| 2 | [Asynchronous programming - C#](<https://learn.microsoft.com/en-us/dotnet/csharp/asynchronous-programming/>) | 3 | Целевой учебный материал посвящён модели асинхронного программирования C#. |
| 3 | [Entity Framework documentation hub](<https://learn.microsoft.com/en-us/ef/>) | 0 | Навигация Entity Framework не объясняет асинхронную модель исполнения C#. |
| 4 | [Introduction to .NET - .NET](<https://learn.microsoft.com/en-us/dotnet/core/introduction>) | 2 | Обзор .NET прямо объясняет поддержку async/await языком C#. |
| 5 | [Azure developer documentation](<https://learn.microsoft.com/en-us/azure/developer/>) | 0 | Главная страница Azure developers не отвечает на вопрос об async/await в C#. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [WaveCert: Translation Validation for Asynchronous Dataflow Programs via Dynamic Fractional Permissions](<https://arxiv.org/abs/2312.09326v2>) | 0 | Асинхронные dataflow-графы получены из C для CGRA; механизм async/await C# не рассматривается. |
| 2 | [Automated Synthesis of Asynchronizations](<https://arxiv.org/abs/2209.06648v1>) | 2 | Исследует преобразование C#-программ к async/await и предотвращение гонок при асинхронизации. |
| 3 | [A Survey of Asynchronous Programming Using Coroutines in the Internet of Things and Embedded Systems](<https://arxiv.org/abs/1906.00367v1>) | 0 | Обзор корутин в C/C++ и встроенных системах не объясняет асинхронное программирование C#. |
| 4 | [Thoughts on Learning Human and Programming Languages](<https://arxiv.org/abs/2407.15907v1>) | 0 | Диалог об изучении языков; слово asynchronous относится к обсуждению, а не к async/await. |
| 5 | [Honey: A dataflow programming language for the processing, featurization and analysis of multivariate, asynchronous and non-uniformly sampled scalar symbolic time sequences](<https://arxiv.org/abs/1609.03146v1>) | 0 | Описывает отдельный язык Honey для асинхронных временных рядов без связи с C#. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Operators and expressions - List all operators and expression - C# ...](<https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/operators/>) | 1 | Справочник операторов C# полезен как фон, но не объясняет асинхронный поток исполнения. |
| 2 | [C# Guide - .NET managed language - C# \| Microsoft Learn](<https://learn.microsoft.com/en-us/dotnet/csharp/>) | 1 | Навигационная страница руководства по C# не содержит самостоятельного объяснения async/await. |
| 3 | [C Sharp (programming language ) - Wikipedia](<https://en.wikipedia.org/wiki/C_Sharp_(programming_language)>) | 1 | Обзор C# даёт сведения о языке, а не развёрнутое объяснение асинхронности. |

В сохранённой выборке только 3 результатов.

## Q06. .NET garbage collection

Найти устройство сборки мусора .NET, поколения и управление памятью.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [<gcConcurrent> element](<https://learn.microsoft.com/en-us/dotnet/framework/configure-apps/file-schema/runtime/gcconcurrent-element>) | 2 | Описывает различие concurrent/background GC и зависимость от версии .NET. |
| 2 | [ICorDebugProcess5::GetGCHeapInformation Method](<https://learn.microsoft.com/en-us/dotnet/core/unmanaged-api/debugging/icordebug/icordebugprocess5-getgcheapinformation-method>) | 1 | Метод отладчика возвращает сведения о куче, но не обучает устройству GC. |
| 3 | [ICLRGCManager Interface](<https://learn.microsoft.com/en-us/dotnet/framework/unmanaged-api/hosting/iclrgcmanager-interface>) | 1 | Справочник интерфейса настройки GC, без объяснения работы поколений. |
| 4 | [Background garbage collection](<https://learn.microsoft.com/en-us/dotnet/standard/garbage-collection/background-gc>) | 2 | Специальная страница о фоновой сборке мусора и поддерживаемых режимах .NET. |
| 5 | [ICLRGCManager2 Interface](<https://learn.microsoft.com/en-us/dotnet/framework/unmanaged-api/hosting/iclrgcmanager2-interface>) | 1 | Справочник управления размером сегмента и поколения 0, а не механизм сборки. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Garbage collection (computer science)](<https://en.wikipedia.org/wiki/Garbage_collection_(computer_science)>) | 1 | Общая статья о GC даёт фон, но не раскрывает механизм поколений .NET. |
| 2 | [Tracing garbage collection](<https://en.wikipedia.org/wiki/Tracing_garbage_collection>) | 1 | Объясняет трассирующую сборку мусора в целом, не специфику .NET. |
| 3 | [Garbage in, garbage out](<https://en.wikipedia.org/wiki/Garbage_in,_garbage_out>) | 0 | Принцип качества входных данных GIGO не относится к сборке мусора. |
| 4 | [.NET Framework](<https://en.wikipedia.org/wiki/.NET_Framework>) | 2 | Объясняет поколенческую и фоновую сборку мусора в .NET Framework. |
| 5 | [Garbage (band)](<https://en.wikipedia.org/wiki/Garbage_(band)>) | 0 | Это музыкальная группа Garbage, а не управление памятью. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [.NET documentation - .NET](<https://learn.microsoft.com/en-us/dotnet/>) | 1 | Стартовая страница .NET не объясняет устройство GC. |
| 2 | [What's new in .NET 11 runtime](<https://learn.microsoft.com/en-us/dotnet/core/whats-new/dotnet-11/runtime>) | 1 | Обновления runtime упоминают исправления GC, не раскрывая его устройство. |
| 3 | [Introduction to .NET - .NET](<https://learn.microsoft.com/en-us/dotnet/core/introduction>) | 1 | Введение в .NET отмечает tracing GC и IDisposable, но не объясняет поколения. |
| 4 | [Fundamentals of garbage collection - .NET](<https://learn.microsoft.com/en-us/dotnet/standard/garbage-collection/fundamentals>) | 3 | Специальный материал о фундаментальных механизмах GC в CLR и освобождении памяти. |
| 5 | [.NET documentation - .NET](<https://learn.microsoft.com/en-us/dotnet/fundamentals/>) | 1 | Документационный хаб .NET не является руководством по GC. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Conditional Nested Pattern Matching in Interaction Net](<https://arxiv.org/abs/2410.00540v1>) | 0 | Работа о сопоставлении образцов в interaction nets; механизм сборщика мусора .NET не раскрывается. |
| 2 | [MyCaffe: A Complete C# Re-Write of Caffe with Reinforcement Learning](<https://arxiv.org/abs/1810.02272v1>) | 1 | C# и сборка мусора .NET упомянуты как преимущества платформы, но исследуется перенос Caffe. |
| 3 | [Uncovering the Hidden Dangers: Finding Unsafe Go Code in the Wild](<https://arxiv.org/abs/2010.11242v1>) | 0 | Изучает небезопасный код Go, а не поколения и устройство сборщика .NET. |
| 4 | [JETO-Bench: A Reproducible Benchmark for Execution Time Improvement Patches in Java](<https://arxiv.org/abs/2606.31767v2>) | 0 | Посвящена бенчмарку оптимизаций Java; .NET указан лишь среди других платформ. |
| 5 | [Abstract machines for game semantics, revisited](<https://arxiv.org/abs/1304.4159v1>) | 0 | Описывает абстрактные машины игровой семантики без устройства сборщика .NET. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [.NET - Wikipedia](<https://en.wikipedia.org/wiki/.NET>) | 1 | Обзор платформы .NET не даёт самостоятельного разбора поколений GC. |
| 2 | [.NET documentation - .NET \| Microsoft Learn](<https://learn.microsoft.com/en-us/dotnet/>) | 1 | Стартовая страница .NET не объясняет устройство GC. |
| 3 | [Welcome to .NET - .NET \| Microsoft Learn - learn.microsoft.com](<https://learn.microsoft.com/en-us/dotnet/welcome>) | 1 | Вводная страница платформы, не описание сборки мусора. |
| 4 | [.NET Framework - Wikipedia](<https://en.wikipedia.org/wiki/.NET_Framework>) | 2 | Объясняет поколенческую и фоновую сборку мусора в .NET Framework. |

В сохранённой выборке только 4 результатов.

## Q07. node.js event loop

Найти объяснение цикла событий Node.js и неблокирующего исполнения.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Node.js Driver for SQL Server](<https://learn.microsoft.com/en-us/sql/connect/node-js/node-js-driver-for-sql-server?view=sql-server-ver17>) | 0 | Установка драйвера SQL для Node.js не объясняет цикл событий. |
| 2 | [Get started with Node.js on Windows](<https://learn.microsoft.com/en-us/windows/dev-environment/javascript/nodejs-overview>) | 1 | Вводная инструкция упоминает неблокирующий I/O без подробного механизма. |
| 3 | [Choose the right Node.js Version for Azure](<https://learn.microsoft.com/en-us/azure/developer/javascript/choose-nodejs-version>) | 1 | Подбор версии упоминает различия event loop, но не раскрывает механизм. |
| 4 | [Create an EventHubs namespace, Event Hub, & consumer group](<https://learn.microsoft.com/en-us/samples/azure/azure-quickstart-templates/event-hubs-create-event-hub-and-consumer-group>) | 0 | Azure Event Hubs — другая сущность; создание ресурса не объясняет event loop. |
| 5 | [Libuv](<https://en.wikipedia.org/wiki/Libuv>) | 2 | Описывает libuv — основу асинхронного цикла Node.js и системные механизмы ввода-вывода. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Node.js](<https://en.wikipedia.org/wiki/Node.js>) | 3 | Статья о Node.js раскрывает неблокирующую модель и цикл событий. |
| 2 | [Event loop](<https://en.wikipedia.org/wiki/Event_loop>) | 3 | Прямо объясняет цикл событий и асинхронное выполнение в Node.js. |
| 3 | [Deno (software)](<https://en.wikipedia.org/wiki/Deno_(software)>) | 1 | Другой JavaScript-runtime даёт контекст libuv/Tokio, но не объясняет цикл Node.js. |
| 4 | [Event-driven programming](<https://en.wikipedia.org/wiki/Event-driven_programming>) | 2 | Объясняет цикл событий и обработчики, включая связь с Node.js. |
| 5 | [Npm](<https://en.wikipedia.org/wiki/Npm>) | 0 | История менеджера пакетов npm не объясняет цикл событий. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Visual Studio documentation](<https://learn.microsoft.com/en-us/visualstudio/windows/>) | 0 | Главная страница Visual Studio не отвечает на запрос о Node.js event loop. |
| 2 | [Create an Incoming Webhook - Teams](<https://learn.microsoft.com/en-us/microsoftteams/platform/webhooks-and-connectors/how-to/add-incoming-webhook>) | 0 | Создание Teams webhook — другая задача. |
| 3 | [Update Language Versions in Azure Functions](<https://learn.microsoft.com/en-us/azure/azure-functions/update-language-versions>) | 0 | Обновление версии Node.js в Functions не описывает event loop. |
| 4 | [Azure App Service documentation - Azure App Service](<https://learn.microsoft.com/en-us/azure/app-service/>) | 0 | Обзор хостинга App Service не объясняет цикл событий Node.js. |
| 5 | [Quickstart: Create a Code App Using the Power Apps CLI - Power Apps](<https://learn.microsoft.com/en-us/power-apps/developer/code-apps/how-to/create-an-app-from-scratch>) | 0 | Создание приложения Power Apps CLI требует Node.js, но не объясняет его асинхронную модель. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Predictive Autoscaling for Node.js on Kubernetes: Lower Latency, Right-Sized Capacity](<https://arxiv.org/abs/2604.19705v2>) | 2 | Использует насыщение event loop Node.js для предиктивного масштабирования; полезный практический аспект, но не общий разбор цикла. |
| 2 | [Node Compass: Multilevel Tracing and Debugging of Request Executions in JavaScript-Based Web-Servers](<https://arxiv.org/abs/2401.08595v1>) | 2 | Разбирает трассировку асинхронного исполнения Node.js и ограничения event-loop-систем. |
| 3 | [RepNet: Cutting Tail Latency in Data Center Networks with Flow Replication](<https://arxiv.org/abs/1407.1239v2>) | 1 | Node.js и неблокирующий event loop используются для RepNet, но цель работы — репликация сетевых потоков. |

В сохранённой выборке только 3 результатов.

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Node.js - Wikipedia](<https://en.wikipedia.org/wiki/Nodejs>) | 3 | Прямо описывает однопоточный цикл событий, I/O и завершение при отсутствии callbacks. |

В сохранённой выборке только 1 результатов.

## Q08. container orchestration kubernetes

Найти принципы оркестрации контейнеров в Kubernetes.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Orchestrate containers on Windows Server using Kubernetes](<https://learn.microsoft.com/en-us/training/modules/orchestrate-containers-windows-server-using-kubernetes>) | 3 | Учебный модуль прямо объясняет оркестрацию, Kubernetes и развёртывание кластера. |
| 2 | [Apply Networking concepts to Windows containers running on Azure Kubernetes Service (AKS) and AKS Hybrid](<https://learn.microsoft.com/en-us/training/modules/apply-networking-concepts-to-windows-containers>) | 2 | Рассматривает сетевой аспект оркестрации контейнеров в Kubernetes/AKS. |
| 3 | [Apply storage concepts to Windows containers running on Azure Kubernetes Service (AKS) and AKS Hybrid](<https://learn.microsoft.com/en-us/training/modules/apply-storage-concepts-to-windows-containers>) | 2 | Рассматривает хранение контейнеров в Kubernetes/AKS как часть оркестрации. |
| 4 | [Java samples for Azure Container Service](<https://learn.microsoft.com/en-us/azure/developer/java/sdk/containers-samples>) | 2 | Содержит примеры управления AKS-кластером с Kubernetes orchestration. |
| 5 | [Container services documentation](<https://learn.microsoft.com/en-us/azure/containers>) | 1 | Навигационная страница сервисов содержит ссылки, но мало самостоятельного объяснения. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Kubernetes](<https://en.wikipedia.org/wiki/Kubernetes>) | 3 | Основная статья посвящена Kubernetes и управлению контейнерным кластером. |
| 2 | [Dynatrace](<https://en.wikipedia.org/wiki/Dynatrace>) | 0 | Описание продукта мониторинга лишь упоминает Kubernetes как наблюдаемую платформу. |
| 3 | [Orchestration (computing)](<https://en.wikipedia.org/wiki/Orchestration_(computing)>) | 2 | Разбирает общую оркестрацию вычислительных систем и связь с Kubernetes. |
| 4 | [Containerization (computing)](<https://en.wikipedia.org/wiki/Containerization_(computing)>) | 2 | Объясняет контейнеризацию и назначение оркестрации с Kubernetes. |
| 5 | [Docker (software)](<https://en.wikipedia.org/wiki/Docker_(software)>) | 1 | Docker даёт контекст контейнеров; искомая оркестрация не является основной темой. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [AZ-104: Deploy and manage Azure compute resources - Training](<https://learn.microsoft.com/en-us/training/paths/az-104-manage-compute-resources/>) | 1 | Общий курс вычислительных ресурсов и ACI не раскрывает именно Kubernetes orchestration. |
| 2 | [What is Azure Kubernetes Service (AKS)? - Azure Kubernetes Service](<https://learn.microsoft.com/en-us/azure/aks/what-is-aks>) | 3 | Прямой обзор AKS как управляемого Kubernetes и средств оркестрации контейнеров. |
| 3 | [Microsoft Azure troubleshooting documentation](<https://learn.microsoft.com/en-us/troubleshoot/azure/>) | 0 | Общий каталог устранения неполадок Azure не отвечает на вопрос об оркестрации. |
| 4 | [Azure Arc](<https://learn.microsoft.com/en-us/azure/azure-arc/>) | 1 | Azure Arc связан с управлением инфраструктурой, но в хабе нет объяснения оркестрации. |
| 5 | [Microsoft Azure container services documentation](<https://learn.microsoft.com/en-us/azure/containers/>) | 1 | Навигационная страница сервисов содержит ссылки, но мало самостоятельного объяснения. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Container Orchestration on HPC Systems](<https://arxiv.org/abs/2012.08866v2>) | 2 | Описывает контейнерную оркестрацию и интеграцию Kubernetes с планировщиком HPC. |
| 2 | [Context Kubernetes: Declarative Orchestration of Enterprise Knowledge for Agentic AI Systems](<https://arxiv.org/abs/2604.11623v3>) | 1 | Kubernetes служит аналогией для оркестрации знаний AI-агентов, а не основным предметом управления контейнерами. |
| 3 | [Extensions for Shared Resource Orchestration in Kubernetes to Support RT-Cloud Containers](<https://arxiv.org/abs/2301.07479v1>) | 2 | Рассматривает расширение Kubernetes для размещения контейнеров реального времени и управления ресурсами. |
| 4 | [Enabling 5G QoS configuration capabilities for IoT applications on container orchestration platform](<https://arxiv.org/abs/2403.05686v1>) | 2 | Показывает интеграцию сетевого QoS с Kubernetes и развёртыванием контейнерных приложений. |
| 5 | [Virtualized C-RAN Orchestration with Docker, Kubernetes and OpenAirInterface](<https://arxiv.org/abs/2001.08992v1>) | 2 | Демонстрирует развёртывание и динамическое масштабирование Docker-контейнеров C-RAN с Kubernetes. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|

В сохранённой выборке только 0 результатов.

## Q09. TLS certificate validation

Найти проверку TLS-сертификатов: доверие, цепочка, имя узла и срок действия.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [4.2.3 Unauthorized IP-HTTPS Server Accepting Connections from a Genuine IP-HTTPS Client](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-iphttps/6caaeec8-99c5-4b59-b21c-907687d5aa9f>) | 1 | Сценарий атаки рекомендует TLS-валидацию, но не объясняет её процедуры. |
| 2 | [CoreWebView2ServerCertificateErrorAction Enum](<https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/winrt/microsoft_web_webview2_core/corewebview2servercertificateerroraction?view=webview2-winrt-1.0.4258.31>) | 1 | Перечень действий WebView2 при ошибке сертификата, без объяснения самих проверок. |
| 3 | [1.3.6.1.2 Trusted Certificate](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/mc-prch/6cd5f905-60e4-4098-8315-a792ed71255a>) | 1 | Требование доверять сертификату узла не раскрывает алгоритм цепочки и имени. |
| 4 | [1.4 Relationship to Other Protocols](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rcmp/5778a360-ad38-4957-8dce-647b4774ee63>) | 1 | Контекст сопоставления клиентских сертификатов не покрывает общую проверку TLS-сервера. |
| 5 | [3.2.7.1 TLS Session Established Successfully](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-peap/12b94d70-96ac-419a-b54d-7975bf169141>) | 2 | Описывает проверку сертификата при TLS-сеансе и реакцию на недоверенного издателя. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [OCSP stapling](<https://en.wikipedia.org/wiki/OCSP_stapling>) | 2 | Прямо объясняет проверку отзыва сертификата через OCSP stapling. |
| 2 | [Extended Validation Certificate](<https://en.wikipedia.org/wiki/Extended_Validation_Certificate>) | 1 | Описывает расширенную проверку владельца сертификата, не полный алгоритм TLS-валидации. |
| 3 | [Public key certificate](<https://en.wikipedia.org/wiki/Public_key_certificate>) | 2 | Описывает сертификаты сервера TLS и проверку со стороны подключающегося клиента. |
| 4 | [Domain-validated certificate](<https://en.wikipedia.org/wiki/Domain-validated_certificate>) | 1 | Проверка владения доменом при выдаче сертификата — фон, а не проверка цепочки клиентом. |
| 5 | [X.509](<https://en.wikipedia.org/wiki/X.509>) | 3 | Стандарт X.509 описывает поля срока действия, издателя и проверку доверия к сертификатам. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Install a TLS/SSL Certificate for Your App - Azure App Service](<https://learn.microsoft.com/en-us/azure/app-service/configure-ssl-certificate>) | 2 | Требования к сертификату App Service и цепочке дают практическую часть TLS-проверки. |
| 2 | [Set Up Microsoft Entra CBA - Microsoft Entra ID](<https://learn.microsoft.com/en-us/entra/identity/authentication/how-to-certificate-based-authentication>) | 2 | Настройка клиентской X.509-аутентификации раскрывает доверие и TLS-handshake в частном сценарии. |
| 3 | [Extensible Authentication Protocol (EAP) for network access in Windows](<https://learn.microsoft.com/en-us/windows-server/networking/technologies/extensible-authentication-protocol/network-access>) | 2 | Описывает серверную валидацию и EAP-TLS в конкретном сценарии сетевой аутентификации. |
| 4 | [Untrusted Certificate Authority Error When You Connect to SQL Server - SQL Server](<https://learn.microsoft.com/en-us/troubleshoot/sql/database-engine/connect/error-message-when-you-connect>) | 2 | Объясняет ошибку недоверенного центра и невозможность проверки сертификата клиентом. |
| 5 | [Certificate Requirements for SQL Server - SQL Server](<https://learn.microsoft.com/en-us/sql/database-engine/configure-windows/certificate-requirements>) | 2 | Рассматривает требования, права доступа и срок действия TLS-сертификата SQL Server. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [TLS Certificate and Domain Feature Analysis of Phishing Domains in the Danish .dk Namespace](<https://arxiv.org/abs/2603.21652v2>) | 1 | Сравнивает признаки TLS-сертификатов фишинговых доменов; порядок проверки цепочки и имени узла не объясняет. |
| 2 | [Assessing SSL/TLS Certificate Centralization: Implications for Digital Sovereignty](<https://arxiv.org/abs/2504.16897v2>) | 1 | Обсуждает централизацию удостоверяющих центров, а не алгоритм проверки сертификата клиентом. |
| 3 | [Will You Trust This TLS Certificate? Perceptions of People Working in IT (Extended Version)](<https://arxiv.org/abs/2207.11610v1>) | 2 | Рассматривает конкретные ошибки валидации, самоподписанные и ограниченные по именам сертификаты; полезная часть темы доверия. |
| 4 | [Overcoming DNSSEC Islands of Security: A TLS and IP-Based Certificate Solution](<https://arxiv.org/abs/2509.08364v2>) | 1 | Основная задача — разрывы цепочки DNSSEC; TLS используется для альтернативного решения. |
| 5 | [Detecting Malignant TLS Servers Using Machine Learning Techniques](<https://arxiv.org/abs/1705.09044v1>) | 2 | Объясняет дополнительную проверку сомнительных TLS-сертификатов после неуспеха традиционной валидации. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Transport Layer Security - Wikipedia](<https://en.wikipedia.org/wiki/Transport_Layer_Security>) | 1 | Обзор протокола TLS даёт контекст аутентификации, но не цельное руководство по валидации. |
| 2 | [TLS - Wikipedia](<https://en.wikipedia.org/wiki/TLS>) | 0 | Страница неоднозначности TLS не объясняет проверку сертификатов. |

В сохранённой выборке только 2 результатов.

## Q10. PowerShell remoting SSH

Найти настройку или механизм удалённой работы PowerShell через SSH.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [PowerShell Security](<https://learn.microsoft.com/en-us/powershell/scripting/security/overview?view=powershell-7.6>) | 1 | Навигационный раздел безопасности содержит ссылку на SSH remoting, но не саму инструкцию. |
| 2 | [PowerShell remoting to Azure Arc-enabled servers](<https://learn.microsoft.com/en-us/azure/azure-arc/servers/ssh-arc-powershell-remoting>) | 3 | Прямое руководство по PowerShell remoting через SSH для Azure Arc. |
| 3 | [Develop on a remote machine using SSH in Visual Studio Code](<https://learn.microsoft.com/en-us/training/modules/develop-on-remote-machine>) | 1 | Настройка Remote-SSH в VS Code полезна как фон, но не объясняет PowerShell remoting. |
| 4 | [Using WS-Management (WSMan) Remoting in PowerShell](<https://learn.microsoft.com/en-us/powershell/scripting/security/remoting/wsman-remoting-in-powershell?view=powershell-7.6>) | 1 | Инструкция WinRM относится к remoting, но использует другой транспорт. |
| 5 | [Using Visual Studio Code for remote editing and debugging](<https://learn.microsoft.com/en-us/powershell/scripting/dev-cross-plat/vscode/using-vscode-for-remote-editing-and-debugging?view=powershell-7.6>) | 2 | Показывает Enter-PSSession -HostName и различия SSH, WinRM и PowerShell Direct. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [PowerShell](<https://en.wikipedia.org/wiki/PowerShell>) | 1 | Обзор PowerShell отмечает SSH, но не даёт конкретной настройки remoting. |
| 2 | [OpenSSH](<https://en.wikipedia.org/wiki/OpenSSH>) | 1 | OpenSSH — необходимый фон, но не инструкция по PowerShell remoting. |
| 3 | [Shell (computing)](<https://en.wikipedia.org/wiki/Shell_(computing)>) | 1 | Объясняет удалённые оболочки в целом, включая PowerShell и SSH. |
| 4 | [Ansible (software)](<https://en.wikipedia.org/wiki/Ansible_(software)>) | 1 | Упоминает разные транспорты Ansible/PowerShell, не давая настройки PowerShell over SSH. |
| 5 | [Shell script](<https://en.wikipedia.org/wiki/Shell_script>) | 0 | Статья о shell-скриптах не решает задачу PowerShell remoting over SSH. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [What is PowerShell? - PowerShell](<https://learn.microsoft.com/en-us/powershell/scripting/overview>) | 1 | Общее описание PowerShell без инструкции SSH remoting. |
| 2 | [Differences between Windows PowerShell 5.1 and PowerShell 7.x - PowerShell](<https://learn.microsoft.com/en-us/powershell/scripting/whats-new/differences-from-windows-powershell>) | 2 | Сравнивает поддержку SSH-remoting и PSRP в версиях PowerShell. |
| 3 | [Key-Based Authentication in OpenSSH for Windows](<https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_keymanagement>) | 1 | Настройка ключей OpenSSH — необходимый фон, но не настройка PowerShell remoting. |
| 4 | [Running Remote Commands - PowerShell](<https://learn.microsoft.com/en-us/powershell/scripting/security/remoting/running-remote-commands>) | 1 | Обзор удалённых команд в основном объясняет WS-Management, а не SSH-транспорт. |
| 5 | [Connect to Remote Desktop Services in Windows Server](<https://learn.microsoft.com/en-us/windows-server/remote/remote-desktop-services/connect-remote-desktop-services>) | 0 | Remote Desktop — другой способ доступа, не PowerShell over SSH. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|

В сохранённой выборке только 0 результатов.

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [PowerShell Documentation - PowerShell \| Microsoft Learn](<https://learn.microsoft.com/en-us/powershell/>) | 1 | Главная страница документации PowerShell без самостоятельной инструкции SSH remoting. |
| 2 | [Install PowerShell 7 on Windows - PowerShell \| Microsoft Learn](<https://learn.microsoft.com/en-us/powershell/scripting/install/install-powershell-on-windows?view=powershell-7.6>) | 1 | Установка PowerShell лишь упоминает поддержку SSH remoting. |
| 3 | [PowerShell - Wikipedia](<https://en.wikipedia.org/wiki/PowerShell>) | 1 | Обзор PowerShell отмечает SSH, но не даёт конкретной настройки remoting. |

В сохранённой выборке только 3 результатов.

## Q11. Azure virtual network peering

Найти назначение, настройку и ограничения peering виртуальных сетей Azure.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Configure Azure Virtual Network peering](<https://learn.microsoft.com/en-us/training/modules/configure-vnet-peering>) | 3 | Учебный модуль прямо разбирает настройку peering, gateway transit и ограничения связности. |
| 2 | [Virtual Network documentation](<https://learn.microsoft.com/en-us/azure/virtual-network>) | 1 | Раздел документации VNet содержит нужные ссылки, но мало самостоятельного ответа. |
| 3 | [Internet Peering documentation](<https://learn.microsoft.com/en-us/azure/internet-peering>) | 0 | Internet Peering и ASN — другой сервис, не peering Azure Virtual Networks. |
| 4 | [Introduction to Azure Virtual Networks](<https://learn.microsoft.com/en-us/training/modules/introduction-to-azure-virtual-networks>) | 2 | Курс VNet включает peering и упражнение по соединению виртуальных сетей. |
| 5 | [Connect multiple Azure VMware Solution Generation 2 private clouds](<https://learn.microsoft.com/en-us/azure/azure-vmware/native-connect-multiple-private-clouds>) | 2 | Объясняет применение VNet peering для соединения приватных облаков VMware. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Content delivery network](<https://en.wikipedia.org/wiki/Content_delivery_network>) | 0 | CDN и размещение кешей не являются peering виртуальных сетей Azure. |
| 2 | [Cloud computing](<https://en.wikipedia.org/wiki/Cloud_computing>) | 0 | Общее определение облачных вычислений не объясняет Azure VNet peering. |
| 3 | [Victor Bahl](<https://en.wikipedia.org/wiki/Victor_Bahl>) | 0 | Биография исследователя и история Azure networking не решают задачу настройки. |
| 4 | [Network Performance Monitoring Solution](<https://en.wikipedia.org/wiki/Network_Performance_Monitoring_Solution>) | 1 | Мониторинг Azure VNets связан с сетями, но не с настройкой peering. |
| 5 | [VNS3](<https://en.wikipedia.org/wiki/VNS3>) | 0 | Описание стороннего сетевого продукта не объясняет Azure VNet peering. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Azure Virtual Network Peering](<https://learn.microsoft.com/en-us/azure/virtual-network/virtual-network-peering-overview>) | 3 | Специальная страница VNet peering объясняет назначение, варианты и ограничения. |
| 2 | [Hub-Spoke Network Topology in Azure - Azure Architecture Center](<https://learn.microsoft.com/en-us/azure/architecture/networking/architecture/hub-spoke>) | 2 | Hub-spoke архитектура объясняет практическое применение соединений виртуальных сетей. |
| 3 | [Azure virtual network traffic routing](<https://learn.microsoft.com/en-us/azure/virtual-network/virtual-networks-udr-overview>) | 2 | Разбирает маршруты, создаваемые при peering, как часть поведения соединённых VNet. |
| 4 | [AZ-700 Design and Implement Microsoft Azure Network Solutions - Training](<https://learn.microsoft.com/en-us/training/paths/design-implement-microsoft-azure-networking-solutions-az-700/>) | 1 | Обзор учебной траектории содержит peering среди тем, но не самостоятельное объяснение. |
| 5 | [Microsoft Azure Virtual Network support - Power Platform](<https://learn.microsoft.com/en-us/power-platform/admin/vnet-support-overview>) | 2 | Конкретный сценарий peering между регионами для Power Platform. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|

В сохранённой выборке только 0 результатов.

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Microsoft Azure - Wikipedia](<https://en.wikipedia.org/wiki/Microsoft_Azure>) | 1 | Обзор Azure даёт контекст платформы без разбора peering. |
| 2 | [Azure documentation \| Microsoft Learn](<https://learn.microsoft.com/en-us/azure/>) | 1 | Главная страница Azure — только навигация к тематическим материалам. |
| 3 | [List of Azure Regions \| Microsoft Learn](<https://learn.microsoft.com/en-us/azure/reliability/regions-list>) | 0 | Список регионов Azure не объясняет сетевой peering. |

В сохранённой выборке только 3 результатов.

## Q12. Microsoft Graph OAuth permissions

Найти разрешения OAuth для Microsoft Graph, delegated/application permissions.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Microsoft doesn't expose permissions to generate OAuth tokens for Extended MAPI access to mailboxes](<https://learn.microsoft.com/en-us/troubleshoot/outlook/authentication/expose-permissions-issue-with-mapi-oauth-tokens>) | 1 | Описывает отсутствие OAuth-модели для Extended MAPI; Graph только альтернативный API. |
| 2 | [Use Microsoft Entra ID to access the Windows 365 APIs in Microsoft Graph](<https://learn.microsoft.com/en-us/windows-365/enterprise/permission-scopes>) | 2 | Практическая настройка разрешений Microsoft Graph для Windows 365. |
| 3 | [Authentication differences between Exchange Web Services (EWS) and Microsoft Graph](<https://learn.microsoft.com/en-us/graph/migrate-exchange-web-services-authentication>) | 3 | Явно сравнивает delegated и application permissions в Microsoft Graph. |
| 4 | [Build Azure DevOps integrations with Microsoft Entra OAuth apps](<https://learn.microsoft.com/en-us/azure/devops/integrate/get-started/authentication/entra-oauth?view=azure-devops>) | 1 | Инструкция ориентирована на ресурс Azure DevOps; Graph упоминается как пример ссылки. |
| 5 | [Calling an API](<https://learn.microsoft.com/en-us/entra/msal/android/calling-an-api>) | 2 | Объясняет запрос scopes API и прямо направляет к разрешениям Microsoft Graph. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [OAuth](<https://en.wikipedia.org/wiki/OAuth>) | 1 | Даёт фон OAuth и grant types, но не разрешения delegated/application в Graph. |
| 2 | [Google Play](<https://en.wikipedia.org/wiki/Google_Play>) | 0 | Описание магазина Google Play не отвечает на запрос. |
| 3 | [Comparison of issue-tracking systems](<https://en.wikipedia.org/wiki/Comparison_of_issue-tracking_systems>) | 0 | Сравнение баг-трекеров не относится к разрешениям Graph. |
| 4 | [OSI model](<https://en.wikipedia.org/wiki/OSI_model>) | 0 | Сетевая модель OSI не объясняет OAuth-разрешения Graph. |
| 5 | [Backend as a service](<https://en.wikipedia.org/wiki/Backend_as_a_service>) | 0 | Общий BaaS и социальный вход OAuth не объясняют модель разрешений Microsoft Graph. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [OAuth 2.0 client credentials flow on the Microsoft identity platform - Microsoft identity platform](<https://learn.microsoft.com/en-us/entra/identity-platform/v2-oauth2-client-creds-grant-flow>) | 2 | Разбирает application permissions Graph в client credentials flow. |
| 2 | [Microsoft identity platform and OAuth2.0 On-Behalf-Of flow - Microsoft identity platform](<https://learn.microsoft.com/en-us/entra/identity-platform/v2-oauth2-on-behalf-of-flow>) | 2 | Рассматривает делегированный On-Behalf-Of flow со scope Microsoft Graph. |
| 3 | [Scopes and permissions in the Microsoft identity platform - Microsoft identity platform](<https://learn.microsoft.com/en-us/entra/identity-platform/scopes-oidc>) | 3 | Объясняет OAuth scopes и модель разрешений на примерах Microsoft Graph. |
| 4 | [Overview of permissions and consent in the Microsoft identity platform - Microsoft identity platform](<https://learn.microsoft.com/en-us/entra/identity-platform/permissions-consent-overview>) | 3 | Сравнивает delegated/application permissions, контекст доступа и согласие администратора. |
| 5 | [Install the Microsoft Graph PowerShell SDK](<https://learn.microsoft.com/en-us/powershell/microsoftgraph/installation>) | 1 | Установка SDK даёт инструментальный фон без разбора OAuth-разрешений. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Detecting Malicious Entra OAuth Apps with LLM-Based Permission Risk Scoring](<https://arxiv.org/abs/2512.15781v1>) | 2 | Исследует полный набор разрешений Microsoft Graph и риск OAuth-consent, но не даёт общего руководства delegated/application permissions. |

В сохранённой выборке только 1 результатов.

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|

В сохранённой выборке только 0 результатов.

## Q13. transformer attention mechanism

Найти механизм внимания в нейросетевых трансформерах.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Delta–wye transformer](<https://en.wikipedia.org/wiki/Delta%E2%80%93wye_transformer>) | 0 | Схема электрического трансформатора не относится к нейросетям. |
| 2 | [Perceiver](<https://en.wikipedia.org/wiki/Perceiver>) | 2 | Вариант Transformer раскрывает асимметричное внимание и латентное представление. |
| 3 | [Ashish Vaswani](<https://en.wikipedia.org/wiki/Ashish_Vaswani>) | 0 | Биография автора статьи не объясняет механизм внимания. |
| 4 | [Transformer utilization factor](<https://en.wikipedia.org/wiki/Transformer_utilization_factor>) | 0 | Коэффициент использования силового трансформатора относится к электротехнике. |
| 5 | [Energy efficient transformer](<https://en.wikipedia.org/wiki/Energy_efficient_transformer>) | 0 | Энергоэффективность электрических трансформаторов не относится к attention. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Transformer (deep learning)](<https://en.wikipedia.org/wiki/Transformer_(deep_learning)>) | 3 | Основная тема — нейросетевой трансформер и self-attention. |
| 2 | [Attention Is All You Need](<https://en.wikipedia.org/wiki/Attention_Is_All_You_Need>) | 2 | Обзор исходной работы о трансформерах объясняет роль внимания и архитектурную идею. |
| 3 | [Attention (machine learning)](<https://en.wikipedia.org/wiki/Attention_(machine_learning)>) | 3 | Специальная статья о механизме внимания в машинном обучении. |
| 4 | [Generative pre-trained transformer](<https://en.wikipedia.org/wiki/Generative_pre-trained_transformer>) | 2 | Архитектура GPT иллюстрирует использование внимания, но рассматривает более узкий класс моделей. |
| 5 | [Vision transformer](<https://en.wikipedia.org/wiki/Vision_transformer>) | 2 | Описывает self-attention в зрительных трансформерах и его варианты. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Introduction to computer vision concepts - Training](<https://learn.microsoft.com/en-us/training/modules/introduction-computer-vision/>) | 0 | Учебный материал о CNN не объясняет Transformer attention. |
| 2 | [Hand off conversations to ServiceNow - Microsoft Copilot Studio](<https://learn.microsoft.com/en-us/microsoft-copilot-studio/customer-copilot-servicenow>) | 0 | Преобразование событий ServiceNow не связано с attention нейросети. |
| 3 | [azureml.training.tabular.featurization.text.bilstm_attention_transformer module - Azure Machine Learning Python](<https://learn.microsoft.com/en-us/python/api/azureml-training-tabular/azureml.training.tabular.featurization.text.bilstm_attention_transformer>) | 1 | Смежный BiLSTM-attention featurizer даёт контекст NLP, но это не Transformer и не объяснение его внимания. |
| 4 | [azureml.automl.runtime.featurizer.transformer.text.bilstm_attention_transformer module - Azure Machine Learning Python](<https://learn.microsoft.com/en-us/python/api/azureml-automl-runtime/azureml.automl.runtime.featurizer.transformer.text.bilstm_attention_transformer>) | 0 | Почти пустая справочная страница модуля не объясняет механизм attention в Transformer. |
| 5 | [Customize OpenAPI documents](<https://learn.microsoft.com/en-us/aspnet/core/fundamentals/openapi/customize-openapi>) | 0 | Transformer OpenAPI-документа — другой смысл термина, не нейросетевая архитектура. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Transformer-based Personalized Attention Mechanism for Medical Images with Clinical Records](<https://arxiv.org/abs/2206.03003v2>) | 2 | Описывает attention в Transformer для сопоставления медицинских изображений и клинических записей; частное применение механизма. |
| 2 | [Music Transformer](<https://arxiv.org/abs/1809.04281v3>) | 2 | Разбирает self-attention и относительные позиции в музыкальном Transformer. |
| 3 | [Dilated Neighborhood Attention Transformer](<https://arxiv.org/abs/2209.15001v3>) | 2 | Объясняет локальное и разреженное attention, их сложность и ограничения дальних зависимостей. |
| 4 | [Attention Guided CAM: Visual Explanations of Vision Transformer Guided by Self-Attention](<https://arxiv.org/abs/2402.04563v1>) | 2 | Показывает использование self-attention для объяснения решений Vision Transformer. |
| 5 | [Efficient Mixed Transformer for Single Image Super-Resolution](<https://arxiv.org/abs/2305.11403v5>) | 2 | Описывает замену self-attention и оконное внимание в Transformer для повышения разрешения. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Transformer - Wikipedia](<https://en.wikipedia.org/wiki/Transformer>) | 0 | Статья об электротехническом компоненте не соответствует намерению. |
| 2 | [Transformer (deep learning) - Wikipedia](<https://en.wikipedia.org/wiki/Transformer_(deep_learning)>) | 3 | Основная тема — нейросетевой трансформер и self-attention. |

В сохранённой выборке только 2 результатов.

## Q14. retrieval augmented generation

Найти принцип RAG: извлечение контекста и генерация ответа.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Developing Retrieval Augmented Generation (RAG) based LLM Systems from PDFs: An Experience Report](<https://arxiv.org/html/2410.15944v1>) | 3 | Разбирает полный RAG-пайплайн от индексации PDF до retrieval и генерации. |
| 2 | [Implement Retrieval Augmented Generation (RAG) with Azure Databricks](<https://learn.microsoft.com/en-us/training/modules/retrieval-augmented-generation-azure-databricks>) | 3 | Специальный учебный модуль о соединении внешнего retrieval и генерации в RAG. |
| 3 | [Design and implement RAG with SQL](<https://learn.microsoft.com/en-us/training/modules/design-implement-rag-with-sql>) | 3 | Учебный модуль включает выбор контекста SQL, augmented prompts и обработку ответа. |
| 4 | [Optimize generative AI model performance with Microsoft Foundry](<https://learn.microsoft.com/en-us/training/modules/optimize-generative-ai-model-performance>) | 2 | Рассматривает grounding/RAG как одну из стратегий оптимизации генеративной модели. |
| 5 | [Implement AI capabilities in database solutions](<https://learn.microsoft.com/en-us/training/paths/implement-ai-capabilities-database-solutions>) | 2 | Комплексный курс БД включает RAG, векторный поиск и подготовку контекста. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Retrieval-augmented generation](<https://en.wikipedia.org/wiki/Retrieval-augmented_generation>) | 3 | Прямое определение RAG и последовательности поиска документов перед генерацией ответа. |
| 2 | [Vector database](<https://en.wikipedia.org/wiki/Vector_database>) | 1 | Векторная БД — компонент извлечения, а не самостоятельное описание полного RAG. |
| 3 | [Prompt engineering](<https://en.wikipedia.org/wiki/Prompt_engineering>) | 1 | Prompt engineering связан с контекстом, но не раскрывает весь retrieval-пайплайн. |
| 4 | [Knowledge cutoff](<https://en.wikipedia.org/wiki/Knowledge_cutoff>) | 1 | Ограничение знаний LLM объясняет мотивацию внешнего контекста, но не сам RAG-пайплайн. |
| 5 | [Large language model](<https://en.wikipedia.org/wiki/Large_language_model>) | 2 | Обзор LLM содержит прямое объяснение соединения генерации с поиском документов. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Introduction to retrieval-augmented generation concepts - Training](<https://learn.microsoft.com/en-us/training/modules/rag-fundamentals/>) | 3 | Целевой модуль объясняет подготовку, извлечение и использование данных для генерации ответа. |
| 2 | [Exam AI-500: Designing and Implementing Multi-Agent AI Solutions - Certifications](<https://learn.microsoft.com/en-us/credentials/certifications/exams/ai-500/>) | 0 | Страница регистрации экзамена не объясняет работу RAG. |
| 3 | [Azure Content Understanding documentation](<https://learn.microsoft.com/en-us/azure/ai-services/content-understanding/>) | 0 | Документационный хаб распознавания контента не объясняет RAG-пайплайн. |
| 4 | [Study guide for Exam AI-200: Developing AI Cloud Solutions on Azure](<https://learn.microsoft.com/en-us/credentials/certifications/resources/study-guides/ai-200>) | 0 | План экзамена перечисляет темы, но не объясняет алгоритм. |
| 5 | [Design and Develop a RAG Solution on Azure - Azure Architecture Center](<https://learn.microsoft.com/en-us/azure/architecture/ai-ml/guide/rag/rag-solution-design-and-evaluation-guide>) | 3 | Архитектурное руководство прямо описывает проектирование RAG и retrieval перед генерацией. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [AR-RAG: Autoregressive Retrieval Augmentation for Image Generation](<https://arxiv.org/abs/2506.06962v3>) | 1 | Извлекаются визуальные патчи для генерации изображений, а не текстовые документы для ответа; это смежный вариант retrieval augmentation. |
| 2 | [Automated Literature Review Using NLP Techniques and LLM-Based Retrieval-Augmented Generation](<https://arxiv.org/abs/2411.18583v1>) | 2 | Показывает RAG с LLM для генерации обзоров литературы из PDF-документов. |
| 3 | [EVOR: Evolving Retrieval for Code Generation](<https://arxiv.org/abs/2402.12317v2>) | 2 | Описывает извлечение из развивающейся базы знаний и генерацию кода; конкретная реализация RAG. |
| 4 | [Riddle Me This! Stealthy Membership Inference for Retrieval-Augmented Generation](<https://arxiv.org/abs/2502.00306v2>) | 2 | Объясняет контекст извлечённых документов в RAG и исследует утечки через membership inference. |
| 5 | [Ragas: Automated Evaluation of Retrieval Augmented Generation](<https://arxiv.org/abs/2309.15217v2>) | 3 | Прямо объясняет два модуля RAG: извлечение текстового контекста и генерацию LLM, затем их совместную оценку. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Retrieval-augmented generation - Wikipedia](<https://en.wikipedia.org/wiki/Retrieval-augmented_generation>) | 3 | Прямое определение RAG и последовательности поиска документов перед генерацией ответа. |
| 2 | [Retrieval augmented generation (RAG) and indexes](<https://learn.microsoft.com/en-us/azure/foundry/concepts/retrieval-augmented-generation>) | 3 | Объясняет RAG, роль индексов и связь поиска с генеративной моделью. |

В сохранённой выборке только 2 результатов.

## Q15. graph neural networks

Найти устройство и применение графовых нейронных сетей.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [List of network theory topics](<https://en.wikipedia.org/wiki/List_of_network_theory_topics>) | 0 | Список тем теории сетей не раскрывает устройство графовых нейросетей. |
| 2 | [Introduction to computer vision with TensorFlow](<https://learn.microsoft.com/en-us/training/modules/intro-computer-vision-tensorflow>) | 0 | Учебный модуль об изображениях и обычных CNN не относится к GNN. |
| 3 | [Node2vec](<https://en.wikipedia.org/wiki/Node2vec>) | 1 | Node2vec связан с представлениями графов, но не объясняет GNN. |
| 4 | [Weisfeiler Leman graph isomorphism test](<https://en.wikipedia.org/wiki/Weisfeiler_Leman_graph_isomorphism_test>) | 2 | Объясняет выразительность message-passing GNN через тест Вейсфейлера–Лемана. |
| 5 | [Microsoft Cognitive Toolkit](<https://en.wikipedia.org/wiki/Microsoft_Cognitive_Toolkit>) | 1 | Вычислительный граф обычной нейросети не равен графовой нейросети; полезен лишь как фон. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Graph neural network](<https://en.wikipedia.org/wiki/Graph_neural_network>) | 3 | Специальная статья о классе графовых нейронных сетей. |
| 2 | [Knowledge graph](<https://en.wikipedia.org/wiki/Knowledge_graph>) | 2 | Объясняет применение GNN и embeddings к графам знаний. |
| 3 | [Machine-learned interatomic potential](<https://en.wikipedia.org/wiki/Machine-learned_interatomic_potential>) | 2 | Рассматривает message-passing и эквивариантные GNN в конкретном физическом применении. |
| 4 | [Weisfeiler Leman graph isomorphism test](<https://en.wikipedia.org/wiki/Weisfeiler_Leman_graph_isomorphism_test>) | 2 | Объясняет выразительность message-passing GNN через тест Вейсфейлера–Лемана. |
| 5 | [Deep learning](<https://en.wikipedia.org/wiki/Deep_learning>) | 1 | Общий deep learning даёт фон и примеры, но не объясняет GNN. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Microsoft Graph documentation](<https://learn.microsoft.com/en-us/graph/>) | 0 | Microsoft Graph API не является графовой нейронной сетью. |
| 2 | [Install the Microsoft Graph PowerShell SDK](<https://learn.microsoft.com/en-us/powershell/microsoftgraph/installation>) | 0 | Установка Graph SDK не относится к графовым нейросетям. |
| 3 | [Windows Documentation](<https://learn.microsoft.com/en-us/windows/>) | 0 | Документационный хаб Windows не отвечает на запрос о GNN. |
| 4 | [Introduction to computer vision concepts - Training](<https://learn.microsoft.com/en-us/training/modules/introduction-computer-vision/>) | 0 | Обычные CNN для изображений не раскрывают GNN. |
| 5 | [Microsoft Graph REST API v1.0 endpoint reference - Microsoft Graph v1.0](<https://learn.microsoft.com/en-us/graph/api/overview>) | 0 | REST-модель сущностей Microsoft Graph — другая задача, не GNN. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [MECCH: Metapath Context Convolution-based Heterogeneous Graph Neural Networks](<https://arxiv.org/abs/2211.12792v2>) | 2 | Описывает агрегацию информации в гетерогенной GNN через метапути и контекст. |
| 2 | [Transformers are Graph Neural Networks](<https://arxiv.org/abs/2506.22084v1>) | 2 | Объясняет message passing и связь Transformer с GNN на графе токенов. |
| 3 | [Fast and Deep Graph Neural Networks](<https://arxiv.org/abs/1911.08941v1>) | 2 | Разбирает архитектуру глубокой GNN как динамической системы и её эффективность. |
| 4 | [Missing Data Imputation with Adversarially-trained Graph Convolutional Networks](<https://arxiv.org/abs/1905.01907v2>) | 2 | Показывает GNN-автоэнкодер и агрегацию соседей для восстановления пропусков данных. |
| 5 | [Multiclass Graph-Based Large Margin Classifiers: Unified Approach for Support Vectors and Neural Networks](<https://arxiv.org/abs/2512.13410v1>) | 1 | Изучает графовые классификаторы Gabriel и нейросетевые границы классов, а не обычные архитектуры GNN. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Graph theory - Wikipedia](<https://en.wikipedia.org/wiki/Graph_theory>) | 1 | Теория графов — необходимый фон, но не описание нейросетевых моделей. |

В сохранённой выборке только 1 результатов.

## Q16. differential privacy machine learning

Найти применение дифференциальной приватности при машинном обучении.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Model inversion attack](<https://en.wikipedia.org/wiki/Model_inversion_attack>) | 2 | Атаки инверсии и DP как защита дают содержательный прикладной аспект приватности моделей. |
| 2 | [Differential privacy composition theorems](<https://en.wikipedia.org/wiki/Differential_privacy_composition_theorems>) | 2 | Композиция механизмов объясняет важную часть учёта privacy loss при обучении. |
| 3 | [Gautam Kamath](<https://en.wikipedia.org/wiki/Gautam_Kamath>) | 0 | Биография специалиста не является объяснением алгоритма DP в ML. |
| 4 | [Numerical methods for differential equations](<https://en.wikipedia.org/wiki/Numerical_methods_for_differential_equations>) | 0 | Численные методы дифференциальных уравнений не связаны с differential privacy. |
| 5 | [Frank McSherry](<https://en.wikipedia.org/wiki/Frank_McSherry>) | 0 | Биография исследователя не объясняет методы приватного обучения. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Differential privacy](<https://en.wikipedia.org/wiki/Differential_privacy>) | 3 | Специальная статья о дифференциальной приватности и её применении в ML. |
| 2 | [Differentially private stochastic gradient descent](<https://en.wikipedia.org/wiki/Differentially_private_stochastic_gradient_descent>) | 3 | DP-SGD прямо описывает приватное обучение и инструменты TensorFlow/PyTorch. |
| 3 | [Federated learning](<https://en.wikipedia.org/wiki/Federated_learning>) | 2 | Федеративное обучение обсуждает применение DP и приватность модели в смежной постановке. |
| 4 | [Local differential privacy](<https://en.wikipedia.org/wiki/Local_differential_privacy>) | 2 | Объясняет локальную DP и её применение совместно с федеративным обучением. |
| 5 | [Flux (machine-learning framework)](<https://en.wikipedia.org/wiki/Flux_(machine-learning_framework)>) | 0 | Описание Julia-фреймворка Flux без приватности не решает запрос. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [.NET documentation - .NET](<https://learn.microsoft.com/en-us/dotnet/>) | 0 | Документация .NET не относится к differential privacy в ML. |
| 2 | [Microsoft Fabric documentation - Microsoft Fabric](<https://learn.microsoft.com/en-us/fabric/>) | 0 | Главная страница Fabric не описывает нужный математический механизм приватности. |
| 3 | [Know Where to Leverage AI in your Classroom - Training](<https://learn.microsoft.com/en-us/training/modules/know-where-leverage-ai-classroom/>) | 0 | Рекомендации по приватности в классе не являются differential privacy. |
| 4 | [AI Fluency: Explore AI basics - Training](<https://learn.microsoft.com/en-us/training/modules/explore-ai-basics/>) | 1 | Вводный модуль о ML даёт общий фон, но не дифференциальную приватность. |
| 5 | [Azure Databricks documentation](<https://learn.microsoft.com/en-us/azure/databricks/>) | 0 | Хаб Databricks не объясняет дифференциальную приватность. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Bayesian Differential Privacy for Machine Learning](<https://arxiv.org/abs/1901.09697v5>) | 2 | Предлагает Bayesian DP для обучения моделей, обсуждая гарантии, privacy accounting и точность. |
| 2 | [Momentum Gradient Descent Federated Learning with Local Differential Privacy](<https://arxiv.org/abs/2209.14086v2>) | 2 | Совмещает локальную DP с федеративным обучением для защиты данных и уменьшения обмена. |
| 3 | [Three Tools for Practical Differential Privacy](<https://arxiv.org/abs/1812.02890v1>) | 2 | Предлагает практические инструменты DP-обучения: clipping, настройку параметров и большие батчи. |
| 4 | [Statistical Privacy Guarantees of Machine Learning Preprocessing Techniques](<https://arxiv.org/abs/2109.02496v1>) | 2 | Измеряет нарушения приватности при предобработке ML-данных и влияние ресэмплинга. |
| 5 | [Kernel Normalized Convolutional Networks for Privacy-Preserving Machine Learning](<https://arxiv.org/abs/2210.00053v2>) | 2 | Сравнивает нормализацию при DP-обучении и DP-FL; содержательный частный аспект. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Hybrid Genetic Algorithm for Optimal User Order Routing :Multi …](<https://arxiv.org/html/2510.21647v1>) | 0 | Генетическая оптимизация торговых маршрутов не раскрывает дифференциальную приватность. |

В сохранённой выборке только 1 результатов.

## Q17. federated learning data privacy

Найти устройство федеративного обучения и его связь с приватностью данных.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [2.2.1.3 Federated Authentication Token](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-tds/f40cca29-29b8-43ec-b9ff-2f8682486c29>) | 0 | Протокольный токен федеративной аутентификации не описывает FL. |
| 2 | [SourceNetwork](<https://learn.microsoft.com/en-us/skype-sdk/ucwa/sourcenetwork_ref>) | 0 | Перечисление контактных сетей не относится к федеративному обучению. |
| 3 | [3.1.5.4.2 Message Validation](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-mwbf/7db4f054-f31a-447e-a8fc-6547215bfd31>) | 0 | Валидация токена федеративного партнёра — другая задача, не ML. |
| 4 | [Federated portal network](<https://en.wikipedia.org/wiki/Federated_portal_network>) | 0 | Федерация корпоративных порталов не относится к обучению ML-моделей. |
| 5 | [2.2.5.7 LocationType](<https://learn.microsoft.com/en-us/openspecs/sharepoint_protocols/ms-sadmws/edd791ce-78ec-4f5e-8739-a2c22c45cabb>) | 0 | Тип федеративной поисковой локации SharePoint не связан с FL. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Federated learning](<https://en.wikipedia.org/wiki/Federated_learning>) | 3 | Прямо объясняет федеративное обучение, децентрализацию данных и мотивацию приватности. |
| 2 | [Federated Learning of Cohorts](<https://en.wikipedia.org/wiki/Federated_Learning_of_Cohorts>) | 0 | Документ явно сообщает, что рекламный механизм FLoC не использует федеративное обучение. |
| 3 | [Machine learning](<https://en.wikipedia.org/wiki/Machine_learning>) | 2 | Содержит определение FL и пример локального обучения без передачи исходных данных. |
| 4 | [Local differential privacy](<https://en.wikipedia.org/wiki/Local_differential_privacy>) | 2 | Объясняет сочетание FL и локальной DP для защиты данных. |
| 5 | [Privacy](<https://en.wikipedia.org/wiki/Privacy>) | 1 | Общая статья о приватности даёт контекст, но не механизм FL. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Windows client documentation](<https://learn.microsoft.com/en-us/windows/resources/>) | 0 | Документация Windows не соответствует запросу. |
| 2 | [Microsoft Fabric documentation - Microsoft Fabric](<https://learn.microsoft.com/en-us/fabric/>) | 0 | Документационный хаб Fabric не объясняет FL. |
| 3 | [Federated connectors overview - Microsoft 365 Copilot connectors](<https://learn.microsoft.com/en-us/microsoft-365/copilot/connectors/federated-connectors-overview>) | 0 | Федеративные MCP-коннекторы для поиска данных — другой смысл federated. |
| 4 | [Data, privacy, and security for Foundry Models sold by Azure in Microsoft Foundry - Microsoft Foundry](<https://learn.microsoft.com/en-us/azure/foundry/responsible-ai/openai/data-privacy>) | 1 | Политика обработки данных ML-сервиса даёт контекст приватности, но не федеративное обучение. |
| 5 | [Copilot connectors overview - Microsoft 365 Copilot connectors](<https://learn.microsoft.com/en-us/microsoft-365/copilot/connectors/overview>) | 0 | Сопоставление типов коннекторов не относится к обучению моделей. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [VAFL: a Method of Vertical Asynchronous Federated Learning](<https://arxiv.org/abs/2007.06081v1>) | 2 | Объясняет вертикальное асинхронное FL и защиту данных через возмущение локальных представлений. |
| 2 | [Federated and Transfer Learning: A Survey on Adversaries and Defense Mechanisms](<https://arxiv.org/abs/2207.02337v1>) | 3 | Обзор прямо рассматривает федеративное обучение, угрозы приватности и защитные механизмы вместо обещания абсолютной защиты. |
| 3 | [Overcoming Forgetting in Federated Learning on Non-IID Data](<https://arxiv.org/abs/1910.07796v1>) | 1 | Основная тема — дрейф моделей на non-IID данных; отсутствие новых рисков приватности лишь заявлено. |
| 4 | [Privacy Threats Against Federated Matrix Factorization](<https://arxiv.org/abs/2007.01587v1>) | 2 | Исследует угрозы приватности в конкретном варианте FL — федеративной матричной факторизации. |
| 5 | [Decentralized Federated Learning: A Segmented Gossip Approach](<https://arxiv.org/abs/1908.07782v1>) | 2 | Объясняет передачу локальных моделей вместо исходных данных и децентрализованный обмен FL, акцентируя сетевую архитектуру. |

### Bing: ограниченная первая страница (дополнение)

Выдача недоступна: Не найдены органические результаты Bing; это не нулевая релевантность

## Q18. stochastic gradient descent optimization

Найти алгоритм SGD и его применение в оптимизации.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Gradient method](<https://en.wikipedia.org/wiki/Gradient_method>) | 1 | Классифицирует градиентные методы без отдельного раскрытия SGD. |
| 2 | [microsoftml.sgd_optimizer : Stochastic gradient descent](<https://learn.microsoft.com/en-us/sql/machine-learning/python/reference/microsoftml/sgd-optimizer?view=sql-server-ver17>) | 2 | Прикладной справочник SGD-оптимизатора с learning rate, momentum и другими параметрами. |
| 3 | [Mirror descent](<https://en.wikipedia.org/wiki/Mirror_descent>) | 1 | Другой метод оптимизации даёт сравнительный фон, но не искомый алгоритм. |
| 4 | [Descent direction](<https://en.wikipedia.org/wiki/Descent_direction>) | 1 | Определяет направление спуска, но не стохастическую оценку градиента. |
| 5 | [maOptimizer: Optimization Algorithms](<https://learn.microsoft.com/en-us/sql/machine-learning/r/reference/microsoftml/optimizer?view=sql-server-ver17>) | 2 | Описывает варианты SGD и параметры методов оптимизации. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Stochastic gradient descent](<https://en.wikipedia.org/wiki/Stochastic_gradient_descent>) | 3 | Прямо определяет SGD, случайную оценку градиента и применение к оптимизации. |
| 2 | [Gradient descent](<https://en.wikipedia.org/wiki/Gradient_descent>) | 2 | Разбирает базовый градиентный спуск и применение к ML, давая важную основу SGD. |
| 3 | [Stochastic optimization](<https://en.wikipedia.org/wiki/Stochastic_optimization>) | 2 | Обзор стохастической оптимизации содержательно включает SGD и стохастическую аппроксимацию. |
| 4 | [Federated learning](<https://en.wikipedia.org/wiki/Federated_learning>) | 2 | Объясняет SGD/FedSGD как часть федеративного обучения и выбор случайной подвыборки. |
| 5 | [Mirror descent](<https://en.wikipedia.org/wiki/Mirror_descent>) | 1 | Другой метод оптимизации даёт сравнительный фон, но не искомый алгоритм. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Windows client documentation](<https://learn.microsoft.com/en-us/windows/resources/>) | 0 | Документационный хаб Windows не связан с алгоритмом оптимизации. |
| 2 | [ClassificationModels.Sgd Field (Microsoft.Azure.PowerShell.Cmdlets.MachineLearningServices.Support)](<https://learn.microsoft.com/en-us/dotnet/api/microsoft.azure.powershell.cmdlets.machinelearningservices.support.classificationmodels.sgd>) | 1 | Перечисление PowerShell-модели называет SGD, но не раскрывает метод. |
| 3 | [RegressionModels.Sgd Field (Microsoft.Azure.PowerShell.Cmdlets.MachineLearningServices.Support)](<https://learn.microsoft.com/en-us/dotnet/api/microsoft.azure.powershell.cmdlets.machinelearningservices.support.regressionmodels.sgd>) | 1 | Поле regression-модели даёт только краткое описание SGD. |
| 4 | [ForecastingModels.Sgd Field (Microsoft.Azure.PowerShell.Cmdlets.MachineLearningServices.Support)](<https://learn.microsoft.com/en-us/dotnet/api/microsoft.azure.powershell.cmdlets.machinelearningservices.support.forecastingmodels.sgd>) | 1 | Поле forecasting-модели даёт только краткое описание SGD. |
| 5 | [ClassificationModel.SGD Property (Azure.ResourceManager.MachineLearning.Models) - Azure for .NET Developers](<https://learn.microsoft.com/en-us/dotnet/api/azure.resourcemanager.machinelearning.models.classificationmodel.sgd>) | 1 | Поле SDK содержит краткое определение SGD, но не алгоритм и его применение. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Continuous Delayed-Memory Stochastic Gradient Descent and Continuous-Time Reinforcement Learning from History of Astrophysical Time Series Studies](<https://arxiv.org/abs/2609.20906v2>) | 2 | Предлагает вариант SGD с памятью и демонстрирует поведение оптимизации на ландшафтах и временных рядах. |
| 2 | [High-Probability Guarantees for Random Zeroth-Order (Stochastic) Gradient Descent](<https://arxiv.org/abs/2604.23613v2>) | 2 | Исследует стохастический градиентный спуск нулевого порядка и вероятностные гарантии сходимости. |
| 3 | [Stochastic Gradient Descent for Barycenters in Wasserstein Space](<https://arxiv.org/abs/2201.04232v3>) | 2 | Описывает SGD-процедуру для вычисления барицентров в пространстве Вассерштейна. |
| 4 | [Stochastic Gradient Descent for Stochastic Doubly-Nonconvex Composite Optimization](<https://arxiv.org/abs/1805.07960v2>) | 2 | Разбирает SGD для составной невыпуклой оптимизации и условия сходимости. |
| 5 | [Stochastic Gradient Descent without Full Data Shuffle](<https://arxiv.org/abs/2206.05830v1>) | 2 | Объясняет связь SGD, перемешивания данных, сходимости и стоимости дискового ввода-вывода. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|

В сохранённой выборке только 0 результатов.

## Q19. quantum error correction surface code

Найти поверхностные коды квантовой коррекции ошибок.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Microsoft Quantum documentation](<https://learn.microsoft.com/en-us/azure/quantum>) | 1 | Главная страница Quantum содержит ссылку на QEC, но не объяснение surface code. |
| 2 | [Surface Code Class](<https://learn.microsoft.com/en-us/python/qdk/qdk.qre.models.surfacecode?view=qsharp-py>) | 2 | Другой URL справочника SurfaceCode также содержит параметры модели и порога ошибок. |
| 3 | [Surface Code Class](<https://learn.microsoft.com/en-us/python/qdk/qdk.qre.models.qec.surfacecode?view=qsharp-py>) | 2 | Справочник SurfaceCode позволяет понять и настроить модель поверхностного кода. |
| 4 | [qec Package](<https://learn.microsoft.com/en-us/python/qdk/qdk.qre.models.qec?view=qsharp-py>) | 2 | Пакет QEC описывает модели разновидностей surface code и lattice surgery. |
| 5 | [Center for Quantum Information Science & Technology](<https://en.wikipedia.org/wiki/Center_for_Quantum_Information_Science_%26_Technology>) | 0 | Описание исследовательского центра и конференции не объясняет коды. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Quantum error correction](<https://en.wikipedia.org/wiki/Quantum_error_correction>) | 2 | Обзор квантовой коррекции прямо рассматривает поверхностные коды и их роль. |
| 2 | [Surface code](<https://en.wikipedia.org/wiki/Surface_code>) | 3 | Специальная статья о surface code с двумерной решёткой, стабилизаторами и порогом ошибок. |
| 3 | [Stabilizer code](<https://en.wikipedia.org/wiki/Stabilizer_code>) | 2 | Объясняет математический класс стабилизаторных кодов, к которому относится surface code. |
| 4 | [CSS code](<https://en.wikipedia.org/wiki/CSS_code>) | 2 | CSS-коды — содержательная основа стабилизаторных кодов, но не специальный разбор surface code. |
| 5 | [Noisy intermediate-scale quantum computing](<https://en.wikipedia.org/wiki/Noisy_intermediate-scale_quantum_computing>) | 1 | Общий контекст NISQ лишь упоминает реализацию surface code. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Microsoft Surface - Surface](<https://learn.microsoft.com/en-us/surface/>) | 0 | Surface здесь — семейство компьютеров, не квантовый код. |
| 2 | [Surface for Business devices documentation - Surface](<https://learn.microsoft.com/en-us/surface/get-started>) | 0 | Корпоративная документация Surface — другой смысл терма. |
| 3 | [Introduction to the Quantum Programming Language Q# - Azure Quantum ](<https://learn.microsoft.com/en-us/azure/quantum/qsharp-overview>) | 1 | Введение в Q# даёт контекст квантовых программ, но не surface code. |
| 4 | [Contact Surface Support for Business and Education customers - Surface](<https://learn.microsoft.com/en-us/surface/contact-surface-business-education-support>) | 0 | Поддержка устройств Surface не относится к коррекции квантовых ошибок. |
| 5 | [Surface IT Toolkit - Surface](<https://learn.microsoft.com/en-us/surface/surface-it-toolkit>) | 0 | Инструментарий обслуживания Surface не относится к surface codes. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Sparse Mamba Decoder for Quantum Error Correction: Efficient Defect-Centric Processing of Surface Code Syndromes](<https://arxiv.org/abs/2605.17156v2>) | 2 | Предлагает декодер синдромов surface code и разбирает эффективность квантовой коррекции ошибок. |
| 2 | [Optimizing Quantum Error Correction Codes with Reinforcement Learning](<https://arxiv.org/abs/1812.08451v5>) | 2 | Рассматривает адаптацию поверхностных квантовых кодов обучением с подкреплением. |
| 3 | [Quantum error correction with fractal topological codes](<https://arxiv.org/abs/2201.03568v3>) | 2 | Исследует фрактальные surface codes, декодирование и пороги ошибок. |
| 4 | [Performance Analysis of Quantum CSS Error-Correcting Codes via MacWilliams Identities](<https://arxiv.org/abs/2305.01301v4>) | 2 | Выводит оценки ошибок квантовых CSS-кодов, в том числе surface codes. |
| 5 | [Quantum Error Correction resilient against Atom Loss](<https://arxiv.org/abs/2412.07841v3>) | 2 | Дополняет surface code обнаружением потерь атомов и адаптивным декодированием. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Quantum - Wikipedia](<https://en.wikipedia.org/wiki/Quantum>) | 0 | Общее определение кванта не относится к коррекции ошибок. |
| 2 | [Quantum mechanics - Wikipedia](<https://en.wikipedia.org/wiki/Quantum_mechanics>) | 0 | Обзор квантовой механики не объясняет искомые коды. |

В сохранённой выборке только 2 результатов.

## Q20. lithium ion battery degradation

Найти причины и модели деградации литий-ионных аккумуляторов.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Lithium vanadium phosphate battery](<https://en.wikipedia.org/wiki/Lithium_vanadium_phosphate_battery>) | 1 | Свойства ванадий-фосфатного катода связаны с технологией, а не механизмами деградации. |
| 2 | [List of battery types](<https://en.wikipedia.org/wiki/List_of_battery_types>) | 0 | Список типов батарей не объясняет старение литий-ионного аккумулятора. |
| 3 | [Lithium-ion flow battery](<https://en.wikipedia.org/wiki/Lithium-ion_flow_battery>) | 1 | Особый тип литиевого проточного аккумулятора даёт фон без разбора деградации. |
| 4 | [Commodity cell](<https://en.wikipedia.org/wiki/Commodity_cell>) | 1 | Общий материал об аккумуляторных ячейках даёт фон, но не механизм старения. |
| 5 | [Sulfur selenium battery](<https://en.wikipedia.org/wiki/Sulfur_selenium_battery>) | 0 | Альтернативная серно-селеновая батарея не объясняет деградацию литий-ионной. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Lithium-ion battery](<https://en.wikipedia.org/wiki/Lithium-ion_battery>) | 3 | Содержит прямой разбор факторов старения, SEI и механизмов потери эффективности. |
| 2 | [Lithium polymer battery](<https://en.wikipedia.org/wiki/Lithium_polymer_battery>) | 1 | Описание типа LiPo даёт общий контекст, но не причины и модели старения. |
| 3 | [Lithium iron phosphate battery](<https://en.wikipedia.org/wiki/Lithium_iron_phosphate_battery>) | 2 | Разновидность Li-ion рассматривает срок службы и число циклов, полезные для анализа деградации. |
| 4 | [Solid-state battery](<https://en.wikipedia.org/wiki/Solid-state_battery>) | 1 | Твердотельная технология даёт сравнительный фон по долговечности, но не прямой ответ о Li-ion. |
| 5 | [Electric vehicle battery](<https://en.wikipedia.org/wiki/Electric_vehicle_battery>) | 2 | Рассматривает долговечность и подавление деградации аккумуляторов электромобилей. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Is it normal for a Surface Book 2's battery capacity to reduce by 6.3% (5360 mWh) after a year of use? - Microsoft Q&A](<https://learn.microsoft.com/en-us/answers/questions/2330010/is-it-normal-for-a-surface-book-2s-battery-capacit>) | 1 | Вопрос о снижении ёмкости — тематический пример; доступный фрагмент не содержит модели деградации. |
| 2 | [How to avoid killing battery when plugged in? - Microsoft Q&A](<https://learn.microsoft.com/en-us/answers/questions/2328684/how-to-avoid-killing-battery-when-plugged-in>) | 1 | Вопрос об эксплуатации батареи связан с износом, но в сохранённом фрагменте нет объяснения механизмов. |
| 3 | [Surface Pro 9 has a very faint, sweet, chemical smell coming from the vents - Microsoft Q&A](<https://learn.microsoft.com/en-us/answers/questions/5785937/surface-pro-9-has-a-very-faint-sweet-chemical-smel>) | 0 | Вопрос о запахе устройства не объясняет деградацию литий-ионной батареи. |
| 4 | [Surface Pro 9 battery degraded by 2% after 5 months! - Microsoft Q&A](<https://learn.microsoft.com/en-us/answers/questions/2304650/surface-pro-9-battery-degraded-by-2-after-5-months>) | 1 | Доступный фрагмент форумного вопроса сообщает потерю ёмкости, но не причины и модель старения. |
| 5 | [When your Laptop is at 100% Battery and youre charging it does this charge the battery or does this charge the components in the laptop therefore not degrading the battery? - Microsoft Q&A](<https://learn.microsoft.com/en-us/answers/questions/4092477/when-your-laptop-is-at-100-battery-and-youre-charg>) | 1 | Тема вопроса связана с зарядкой и деградацией; сохранённый фрагмент не даёт подтверждённого объяснения. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Physics-Informed Machine Learning for Battery Degradation Diagnostics: A Comparison of State-of-the-Art Methods](<https://arxiv.org/abs/2404.04429v1>) | 2 | Сравнивает физически информированные модели диагностики и прогноза деградации компонентов аккумулятора. |
| 2 | [Lithium-ion battery degradation: using degradation mode analysis to validate lifetime prediction modelling](<https://arxiv.org/abs/2311.05482v2>) | 3 | Связывает механизмы старения с потерей лития и активного материала и объясняет проверку моделей срока службы. |
| 3 | [Lithium-ion battery degradation: how to model it](<https://arxiv.org/abs/2112.02037v2>) | 3 | Работа прямо посвящена моделированию деградации и взаимодействию нескольких механизмов старения Li-ion. |
| 4 | [Lithium-Ion Battery Charging Schedule Optimization to Balance Battery Usage and Degradation](<https://arxiv.org/abs/2205.15440v1>) | 2 | Использует модель деградации для выбора зарядного расписания; полезный прикладной аспект. |
| 5 | [Lithium-ion battery degradation: Introducing the concept of reservoirs to design for lifetime](<https://arxiv.org/abs/2512.15440v1>) | 3 | Описывает физические механизмы SEI, осаждения лития, трещин и высыхания и их влияние на срок службы. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Lithium - Wikipedia](<https://en.wikipedia.org/wiki/Lithium>) | 0 | Статья об элементе и его добыче не отвечает на запрос о деградации ячеек. |
| 2 | [Lithium (medication ) - Wikipedia](<https://en.wikipedia.org/wiki/Lithium_(medication)>) | 0 | Литий как лекарство не относится к аккумуляторам. |

В сохранённой выборке только 2 результатов.

## Q21. photovoltaic solar cell efficiency

Найти эффективность солнечных фотоэлектрических элементов и влияющие факторы.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [List of types of solar cells](<https://en.wikipedia.org/wiki/List_of_types_of_solar_cells>) | 1 | Перечень типов ячеек полезен как фон, но не объясняет факторы КПД. |
| 2 | [Index of solar energy articles](<https://en.wikipedia.org/wiki/Index_of_solar_energy_articles>) | 0 | Указатель статей не объясняет эффективность и её причины. |
| 3 | [Indium gallium arsenide phosphide](<https://en.wikipedia.org/wiki/Indium_gallium_arsenide_phosphide>) | 2 | Пример материала и многопереходной ячейки с конкретной эффективностью. |
| 4 | [Alfonsine Solar Park](<https://en.wikipedia.org/wiki/Alfonsine_Solar_Park>) | 0 | Карточка отдельной электростанции содержит мощность, но не эффективность элементов. |
| 5 | [Sharp Solar](<https://en.wikipedia.org/wiki/Sharp_Solar>) | 0 | История производителя и производственные мощности не отвечают на вопрос о КПД. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Solar cell efficiency](<https://en.wikipedia.org/wiki/Solar_cell_efficiency>) | 3 | Специальная статья об эффективности солнечных элементов, ограничениях и измерениях. |
| 2 | [Solar cell](<https://en.wikipedia.org/wiki/Solar_cell>) | 2 | Статья о солнечном элементе даёт технический контекст преобразования и эффективности. |
| 3 | [Perovskite solar cell](<https://en.wikipedia.org/wiki/Perovskite_solar_cell>) | 2 | Объясняет связь поглощения и свойств перовскита с эффективностью солнечной ячейки. |
| 4 | [Photovoltaic system](<https://en.wikipedia.org/wiki/Photovoltaic_system>) | 2 | Рассматривает влияние затенения и загрязнения на выход фотоэлектрической системы. |
| 5 | [Solar panel](<https://en.wikipedia.org/wiki/Solar_panel>) | 2 | Рассматривает эффективность модулей и ячеек в контексте солнечных панелей. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Explore the Business Value of Generative AI Solutions - Training](<https://learn.microsoft.com/en-us/training/paths/explore-business-value-generative-ai-solutions/>) | 0 | Бизнес-эффективность генеративного AI не относится к КПД солнечных ячеек. |
| 2 | [Teach information literacy with Search Coach](<https://learn.microsoft.com/en-us/training/educator-center/instructor-materials/search-coach>) | 0 | Пример урока о поиске солнечных новостей не объясняет солнечные элементы. |
| 3 | [Training for Developers](<https://learn.microsoft.com/en-us/training/career-paths/developer>) | 0 | Обучение разработчиков и эффективность сервисов не относятся к фотовольтаике. |
| 4 | [**shadow**: R Package for Geometric Shade Calculations in an Urban Environment](<https://learn.microsoft.com/en-us/shows/user-international-r-user-conferences-user-international-r-user-2017-conference/shadow-r-package-geometric-shade-calculations-in-urban-environment>) | 1 | Расчёт затенения и инсоляции даёт контекст условий работы, но не КПД солнечной ячейки. |
| 5 | [Coreinfo - Sysinternals](<https://learn.microsoft.com/en-us/sysinternals/downloads/coreinfo>) | 0 | Processor efficiency и клетки сетки CPU — другие значения слов запроса. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Moon's Radiation Environment and Expected Performance of Solar Cells during Future Lunar Missions](<https://arxiv.org/abs/1012.0717v1>) | 2 | Разбирает влияние радиации и температуры на характеристики и деградацию солнечных элементов. |
| 2 | [Tuning photovoltaic response in Bi2FeCrO6 films by ferroelectric poling](<https://arxiv.org/abs/1808.05847v1>) | 2 | Исследует влияние поляризации, разделения зарядов и рекомбинации на фотоэлектрический отклик. |
| 3 | [Fabrication of single-walled carbon nanotube/Si heterojunction solar cell with high photovoltaic conversion efficiency and stability](<https://arxiv.org/abs/1310.7783v2>) | 2 | Показывает связь конструкции CNT/Si-элементов, сопротивления и прозрачности с КПД. |
| 4 | [UMG silicon for solar PV: from defects detection to PV module degradation](<https://arxiv.org/abs/2101.08019v2>) | 2 | Связывает дефекты кремния, время жизни носителей и эффективность серийных фотоэлементов. |
| 5 | [External quantum efficiency above 100% in photovoltaic cells due to the technique raising efficiencies for all kinds of solar cells](<https://arxiv.org/abs/2409.20066v3>) | 2 | Рассматривает геометрию модуля, quantum efficiency и КПД; гипотезы о тёмной энергии требуют отдельной научной проверки. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Photovoltaics - Wikipedia](<https://en.wikipedia.org/wiki/Photovoltaics>) | 2 | Обзор фотовольтаики содержит раздел об эффективности элементов и условиях измерения. |
| 2 | [Photovoltaic system - Wikipedia](<https://en.wikipedia.org/wiki/Photovoltaic_system>) | 2 | Рассматривает влияние затенения и загрязнения на выход фотоэлектрической системы. |

В сохранённой выборке только 2 результатов.

## Q22. computational fluid dynamics finite volume method

Найти применение метода конечных объёмов в вычислительной гидродинамике.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [List of operator splitting topics](<https://en.wikipedia.org/wiki/List_of_operator_splitting_topics>) | 1 | Навигационный список численных методов не даёт самостоятельного объяснения FVM. |
| 2 | [Diffuse element method](<https://en.wikipedia.org/wiki/Diffuse_element_method>) | 1 | Другой метод аппроксимации уравнений даёт смежный контекст, но не FVM. |
| 3 | [List of fluid mechanics journals](<https://en.wikipedia.org/wiki/List_of_fluid_mechanics_journals>) | 0 | Список журналов не раскрывает алгоритм или его применение. |
| 4 | [Flux-corrected transport](<https://en.wikipedia.org/wiki/Flux-corrected_transport>) | 1 | Смежная консервативная схема переноса, а не объяснение метода конечных объёмов. |
| 5 | [List of partial differential equation topics](<https://en.wikipedia.org/wiki/List_of_partial_differential_equation_topics>) | 1 | Общий список тем PDE — лишь математический фон. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Finite volume method](<https://en.wikipedia.org/wiki/Finite_volume_method>) | 3 | Основная тема — метод конечных объёмов и дискретизация уравнений. |
| 2 | [Computational fluid dynamics](<https://en.wikipedia.org/wiki/Computational_fluid_dynamics>) | 2 | Объясняет вычислительную гидродинамику и численные подходы в целом. |
| 3 | [Volume of fluid method](<https://en.wikipedia.org/wiki/Volume_of_fluid_method>) | 1 | Volume of fluid — метод моделирования свободной поверхности, не тот же алгоритм, что finite volume. |
| 4 | [List of computational fluid dynamics software](<https://en.wikipedia.org/wiki/List_of_computational_fluid_dynamics_software>) | 1 | Перечень программ включает finite-volume, но не объясняет сам метод. |
| 5 | [Fluid mechanics](<https://en.wikipedia.org/wiki/Fluid_mechanics>) | 1 | Общая механика жидкости лишь направляет к CFD. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Microsoft troubleshooting documentation](<https://learn.microsoft.com/en-us/troubleshoot/>) | 0 | Общий хаб устранения неполадок не отвечает на численный метод CFD. |
| 2 | [Dynamics 365 Sustainability documentation](<https://learn.microsoft.com/en-us/dynamics365/sustainability/>) | 0 | Sustainability documentation не объясняет метод конечных объёмов. |
| 3 | [Dynamics 365 Supply Chain Management documentation](<https://learn.microsoft.com/en-us/dynamics365/supply-chain/>) | 0 | Supply Chain Management — другой смысл Dynamics. |
| 4 | [Release plans for Dynamics 365 and Power Platform - Dynamics 365](<https://learn.microsoft.com/en-us/dynamics365/release-plans/>) | 0 | Планы выпуска Dynamics 365 не относятся к CFD. |
| 5 | [Dynamics 365 Finance documentation](<https://learn.microsoft.com/en-us/dynamics365/finance/>) | 0 | Dynamics 365 Finance — бизнес-продукт, не вычислительная гидродинамика. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Quantum Finite Volume Method for Computational Fluid Dynamics with Classical Input and Output](<https://arxiv.org/abs/2102.03557v1>) | 2 | Объясняет сеточные ячейки FVM и предлагает квантовую реализацию CFD; это специальный вариант метода. |
| 2 | [Second-order accurate finite volume method for well-driven flows](<https://arxiv.org/abs/1312.2388v2>) | 2 | Разбирает конечно-объёмную схему, коррекцию потоков и точность для течения около скважины. |
| 3 | [Finite Volume Graph Network(FVGN): Predicting unsteady incompressible fluid dynamics with finite volume informed neural network](<https://arxiv.org/abs/2309.10050v4>) | 2 | Использует конечно-объёмные ограничения для GNN-модели нестационарной гидродинамики. |
| 4 | [High-Order Dynamic Integration Method (HODIM) for Modeling Turbulent Fluid Dynamics](<https://arxiv.org/abs/2410.17063v1>) | 1 | Предлагает другой метод HODIM, а finite volume упоминается в сравнении с существующими подходами. |
| 5 | [Implementation of Immersed Boundaries via Volume Penalization in the Industrial Aeronautical Computational Fluid Dynamics Solver CODA](<https://arxiv.org/abs/2404.16132v3>) | 2 | Применяет конечно-объёмную схему второго порядка в CFD-решателе CODA для аэродинамики со встроенными границами. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|

В сохранённой выборке только 0 результатов.

## Q23. finite element analysis stress

Найти расчёт напряжений методом конечных элементов.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Quadratic quadrilateral element](<https://en.wikipedia.org/wiki/Quadratic_quadrilateral_element>) | 2 | Разбирает элемент Q8 для задач плоских напряжений и деформаций. |
| 2 | [Finite element model data post-processing](<https://en.wikipedia.org/wiki/Finite_element_model_data_post-processing>) | 2 | Описывает интерпретацию результатов FEM, включая напряжения и поверхности текучести. |
| 3 | [Structural mechanics](<https://en.wikipedia.org/wiki/Structural_mechanics>) | 1 | Обзор строительной механики даёт фон и ссылки на FEM, но не подробный алгоритм расчёта. |
| 4 | [Constant strain triangle element](<https://en.wikipedia.org/wiki/Constant_strain_triangle_element>) | 2 | Описывает конечный элемент и его применение к плоскому напряжённому состоянию. |
| 5 | [Barsoum elements](<https://en.wikipedia.org/wiki/Barsoum_elements>) | 2 | Конкретная FEM-техника вычисления коэффициента интенсивности напряжений у трещины. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Finite element method](<https://en.wikipedia.org/wiki/Finite_element_method>) | 3 | Основная статья о FEM объясняет метод и инженерные приложения анализа деформаций. |
| 2 | [List of finite element software packages](<https://en.wikipedia.org/wiki/List_of_finite_element_software_packages>) | 1 | Список FEM-пакетов полезен для выбора инструмента, но не описывает расчёт. |
| 3 | [Mixed finite element method](<https://en.wikipedia.org/wiki/Mixed_finite_element_method>) | 1 | Математический вариант FEM без прямого разбора расчёта напряжений в просмотренном фрагменте. |
| 4 | [Stress concentration](<https://en.wikipedia.org/wiki/Stress_concentration>) | 2 | Объясняет концентрацию напряжений и применение FEM к этой задаче. |
| 5 | [Piping](<https://en.wikipedia.org/wiki/Piping>) | 2 | Приводит инженерный пример расчёта напряжений трубопроводов методом конечных элементов. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Data Analysis Expressions (DAX) Reference - DAX](<https://learn.microsoft.com/en-us/dax/>) | 0 | DAX для аналитики данных не является методом конечных элементов. |
| 2 | [Extract insights from visual data on Azure - Training](<https://learn.microsoft.com/en-us/training/paths/insight-visual-data/>) | 0 | Анализ изображений и multimodal content не отвечает на запрос о FEM. |
| 3 | [Get Data in Power BI - Training](<https://learn.microsoft.com/en-us/training/modules/get-data/>) | 0 | Импорт данных Power BI не связан с расчётом механических напряжений. |
| 4 | [Azure Vision documentation - Quickstarts, Tutorials, API Reference - Foundry Tools](<https://learn.microsoft.com/en-us/azure/ai-services/computer-vision/>) | 0 | Компьютерное зрение Azure не относится к FEM-анализу напряжений. |
| 5 | [Document layout analysis - Document Intelligence - Foundry Tools](<https://learn.microsoft.com/en-us/azure/ai-services/document-intelligence/prebuilt/layout>) | 0 | Анализ разметки документов — другой смысл analysis и element. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [A coupled finite element-virtual element method for thermomechanical analysis of electronic packaging structures](<https://arxiv.org/abs/2511.09348v3>) | 2 | Применяет связанный конечно-элементный метод к тепловым напряжениям электронной упаковки. |
| 2 | [Hybrid stress quadrilateral finite element approximation for stochastic plane elasticity equations](<https://arxiv.org/abs/1502.06065v1>) | 2 | Описывает гибридную конечно-элементную аппроксимацию напряжений в стохастической упругости. |
| 3 | [Towards Automatic Stress Analysis using Scaled Boundary Finite Element Method with Quadtree Mesh of High-order Elements](<https://arxiv.org/abs/1402.5186v1>) | 3 | Прямо описывает технику расчёта напряжений и трещин методом SBFEM с численными примерами. |
| 4 | [Variational principles and finite element Bloch analysis in couple stress elastodynamics](<https://arxiv.org/abs/2106.08893v1>) | 2 | Использует FEM в couple-stress-упругости для волн в периодических материалах; специальный динамический случай. |
| 5 | [A multipoint stress mixed finite element method for elasticity on simplicial grids](<https://arxiv.org/abs/1805.09920v3>) | 2 | Выводит смешанный конечно-элементный метод с напряжениями как неизвестными для линейной упругости. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Finiteness - Wikipedia](<https://en.wikipedia.org/wiki/Finiteness>) | 0 | Понятие конечности не относится к расчёту напряжений. |
| 2 | [Finite element method - Wikipedia](<https://en.wikipedia.org/wiki/Finite_element_method>) | 3 | Основная статья о FEM объясняет метод и инженерные приложения анализа деформаций. |

В сохранённой выборке только 2 результатов.

## Q24. optical coherence tomography

Найти принцип и применение оптической когерентной томографии.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Zhongping Chen](<https://en.wikipedia.org/wiki/Zhongping_Chen>) | 0 | Биография исследователя и перечень интересов не объясняют метод. |
| 2 | [Coherence theory (optics)](<https://en.wikipedia.org/wiki/Coherence_theory_(optics)>) | 1 | Теория когерентности — физический фон, но не описание OCT. |
| 3 | [Intravascular imaging](<https://en.wikipedia.org/wiki/Intravascular_imaging>) | 2 | Описывает OCT как метод внутрисосудистой визуализации и его задачи. |
| 4 | [Multiscale tomography](<https://en.wikipedia.org/wiki/Multiscale_tomography>) | 1 | Общая многомасштабная томография даёт смежный контекст, не конкретный OCT-принцип. |
| 5 | [Eric Swanson (engineer)](<https://en.wikipedia.org/wiki/Eric_Swanson_(engineer)>) | 0 | Биография инженера не даёт самостоятельного объяснения OCT. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Optical coherence tomography](<https://en.wikipedia.org/wiki/Optical_coherence_tomography>) | 3 | Специальная статья о принципе и применениях оптической когерентной томографии. |
| 2 | [Tomography](<https://en.wikipedia.org/wiki/Tomography>) | 1 | Обзор томографии упоминает OCT, но не раскрывает его отдельно. |
| 3 | [Coherence length](<https://en.wikipedia.org/wiki/Coherence_length>) | 2 | Объясняет длину когерентности и её связь с OCT и интерферометрией. |
| 4 | [David Huang (ophthalmologist)](<https://en.wikipedia.org/wiki/David_Huang_(ophthalmologist)>) | 0 | Биография изобретателя не объясняет метод томографии. |
| 5 | [Endoscopic optical coherence tomography imaging](<https://en.wikipedia.org/wiki/Endoscopic_optical_coherence_tomography_imaging>) | 2 | Разбирает конкретное применение OCT для визуализации внутри сосудов. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Azure Vision documentation - Quickstarts, Tutorials, API Reference - Foundry Tools](<https://learn.microsoft.com/en-us/azure/ai-services/computer-vision/>) | 0 | Документация компьютерного зрения не объясняет оптическую когерентную томографию. |
| 2 | [PowerToys Text Extractor Utility for Windows](<https://learn.microsoft.com/en-us/windows/powertoys/text-extractor>) | 0 | Извлечение текста с экрана через OCR не относится к OCT. |
| 3 | [Introduction to AI-powered information extraction concepts - Training](<https://learn.microsoft.com/en-us/training/modules/introduction-information-extraction/>) | 0 | OCR распознаёт текст на изображениях, а не томографическую структуру тканей. |
| 4 | [Dragon Medical One documentation](<https://learn.microsoft.com/en-us/industry/healthcare/dragon-medical-one/>) | 0 | Медицинское речевое ПО не относится к OCT. |
| 5 | [Install Windows Server from installation media](<https://learn.microsoft.com/en-us/windows-server/get-started/install-windows-server>) | 0 | Оптический привод установочного диска — другой смысл optical. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [OCTID: Optical Coherence Tomography Image Database](<https://arxiv.org/abs/1812.07056v2>) | 2 | Объясняет применение OCT для слоёв сетчатки и предоставляет диагностическую базу изображений. |
| 2 | [A Quantum Field Approach for Advancing Optical Coherence Tomography Part I: First Order Correlations, Single Photon Interference, And Quantum Noise](<https://arxiv.org/abs/1402.1722v1>) | 2 | Рассматривает интерференционную и квантово-полевую основу OCT; специализированное объяснение принципа. |
| 3 | [Extracting morphometric information from rat sciatic nerve using optical coherence tomography](<https://arxiv.org/abs/1804.11054v2>) | 2 | Показывает применение OCT для морфометрии периферического нерва и сравнение с гистологией. |
| 4 | [Ghost optical coherence tomography](<https://arxiv.org/abs/1810.03380v1>) | 2 | Демонстрирует OCT через спектральную интерференцию и корреляции с восстановлением изображения преобразованием Фурье. |
| 5 | [Resolution-enhanced OCT and expanded framework of information capacity and resolution in coherent imaging](<https://arxiv.org/abs/2104.02531v1>) | 2 | Разбирает разрешение OCT и компромисс с SNR на конкретном вычислительном методе. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Optics - Wikipedia](<https://en.wikipedia.org/wiki/Optics>) | 1 | Оптика в целом — лишь фон для конкретного метода. |

В сохранённой выборке только 1 результатов.

## Q25. gravitational waves interferometer

Найти регистрацию гравитационных волн интерферометрами.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [CLIO](<https://en.wikipedia.org/wiki/CLIO>) | 2 | Прямо описывает прототип лазерного интерферометрического детектора CLIO. |
| 2 | [TAMA 300](<https://en.wikipedia.org/wiki/TAMA_300>) | 2 | Описывает оптические плечи TAMA и цель обнаружения гравитационных волн. |
| 3 | [Big Bang Observer](<https://en.wikipedia.org/wiki/Big_Bang_Observer>) | 2 | Описывает проект гравитационно-волновой обсерватории — пример применения интерферометрии. |
| 4 | [List of types of interferometers](<https://en.wikipedia.org/wiki/List_of_types_of_interferometers>) | 1 | Перечень типов интерферометров даёт фон, а не механизм регистрации гравитационных волн. |
| 5 | [Kepco Power](<https://en.wikipedia.org/wiki/Kepco_Power>) | 0 | Описание производителя блоков питания лишь упоминает помощь LIGO. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Gravitational-wave observatory](<https://en.wikipedia.org/wiki/Gravitational-wave_observatory>) | 3 | Объясняет измерение гравитационных волн лазерными интерферометрами и масштаб смещений. |
| 2 | [LIGO](<https://en.wikipedia.org/wiki/LIGO>) | 3 | Основная статья о LIGO — прямой пример регистрации волн лазерным интерферометром. |
| 3 | [Laser Interferometer Space Antenna](<https://en.wikipedia.org/wiki/Laser_Interferometer_Space_Antenna>) | 2 | Описывает космический интерферометрический проект регистрации гравитационных волн. |
| 4 | [Virgo interferometer](<https://en.wikipedia.org/wiki/Virgo_interferometer>) | 3 | Статья о Virgo непосредственно посвящена интерферометрическому детектору гравитационных волн. |
| 5 | [Deci-hertz Interferometer Gravitational wave Observatory](<https://en.wikipedia.org/wiki/Deci-hertz_Interferometer_Gravitational_wave_Observatory>) | 2 | Конкретный проект интерферометрической обсерватории DECIGO. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Release plans Dynamics 365 and Power Platform - Dynamics 365](<https://learn.microsoft.com/en-us/dynamics365/release-plans/archived-plans>) | 0 | Release waves продукта Dynamics не являются физическими волнами. |
| 2 | [New changes coming to cloud update - Microsoft 365 Apps](<https://learn.microsoft.com/en-us/microsoft-365-apps/admin-center/cloud-update-enhancements>) | 0 | Волны развёртывания обновлений Microsoft 365 не относятся к гравитационным волнам. |
| 3 | [Modern, refreshed look for model-driven apps - Power Apps](<https://learn.microsoft.com/en-us/power-apps/user/modern-fluent-design>) | 0 | Волны обновления интерфейса Power Apps не соответствуют физическому смыслу запроса. |
| 4 | [Entity Documentation - minecraft:buoyant](<https://learn.microsoft.com/en-us/minecraft/creator/reference/content/entityreference/examples/entitycomponents/minecraftcomponent_buoyant>) | 0 | Игровая плавучесть и гравитация Minecraft не объясняют детектирование волн. |
| 5 | [Launch your portal using the Portal launch scheduler - Microsoft 365 Enterprise](<https://learn.microsoft.com/en-us/microsoft-365/enterprise/portallaunchscheduler>) | 0 | Пакетный запуск портала не связан с интерферометрическими измерениями. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Moriond 07 proceedings:"Extension of the frequency-range of interferometers for the "magnetic" components of gravitational waves?"](<https://arxiv.org/abs/0706.3294v1>) | 2 | Разбирает частотный отклик интерферометра на компоненты гравитационной волны. |
| 2 | [Orbit analysis of a geostationary gravitational wave interferometer detector array](<https://arxiv.org/abs/1410.3023v1>) | 2 | Описывает лазерные измерения в космическом интерферометре GEOGRAWI и влияние орбит на регистрацию. |
| 3 | [Particle production during inflation and gravitational waves detectable by ground-based interferometers](<https://arxiv.org/abs/1109.0022v3>) | 2 | Исследует обнаружимость космологических волн наземными LIGO/Virgo и LISA; полезный частный сценарий. |
| 4 | [Gravitational wave detection with single-laser atom interferometers](<https://arxiv.org/abs/1003.4218v1>) | 2 | Объясняет схему двух атомных интерферометров с одним лазером и подавление фазового шума. |
| 5 | [Theory of Gravitational Waves](<https://arxiv.org/abs/1607.04202v1>) | 2 | Излагает теорию гравитационных волн и связь их взаимодействия с веществом с лазерной интерферометрией. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|

В сохранённой выборке только 0 результатов.

## Q26. CRISPR Cas9 gene editing

Найти механизм редактирования генома CRISPR-Cas9.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Institute of Chemical Biology and Fundamental Medicine](<https://en.wikipedia.org/wiki/Institute_of_Chemical_Biology_and_Fundamental_Medicine>) | 0 | История исследовательского института не относится к механизму редактирования. |
| 2 | [TcNPR3 (gene)](<https://en.wikipedia.org/wiki/TcNPR3_(gene)>) | 1 | Частный пример редактируемого гена не раскрывает общую механику CRISPR-Cas9. |
| 3 | [TIGR-Tas](<https://en.wikipedia.org/wiki/TIGR-Tas>) | 1 | Другой класс РНК-направляемых систем даёт смежный контекст без объяснения Cas9. |
| 4 | [Shredding (disassembling genomic data)](<https://en.wikipedia.org/wiki/Shredding_(disassembling_genomic_data)>) | 2 | Содержит прямое описание пары gRNA/Cas9 и направленного изменения генетического материала. |
| 5 | [Fanzor](<https://en.wikipedia.org/wiki/Fanzor>) | 1 | Смежная РНК-направляемая эндонуклеаза даёт контекст, но не описание Cas9. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [CRISPR gene editing](<https://en.wikipedia.org/wiki/CRISPR_gene_editing>) | 3 | Основная статья о CRISPR gene editing непосредственно отвечает намерению. |
| 2 | [Genome-wide CRISPR-Cas9 knockout screens](<https://en.wikipedia.org/wiki/Genome-wide_CRISPR-Cas9_knockout_screens>) | 2 | Объясняет применение CRISPR-Cas9 и библиотеки sgRNA в геномных knockout-скринингах. |
| 3 | [CRISPR](<https://en.wikipedia.org/wiki/CRISPR>) | 3 | Объясняет Cas9, направляющую РНК, разрез ДНК и использование механизмов репарации. |
| 4 | [He Jiankui affair](<https://en.wikipedia.org/wiki/He_Jiankui_affair>) | 0 | История этического скандала не объясняет механизм CRISPR-Cas9. |
| 5 | [CRISPR Therapeutics](<https://en.wikipedia.org/wiki/CRISPR_Therapeutics>) | 0 | Сведения о компании и терапии не объясняют механизм редактирования генома. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Power BI Desktop projects (PBIP) - Power BI](<https://learn.microsoft.com/en-us/power-bi/developer/projects/projects-overview>) | 0 | Редактирование файлов отчёта Power BI не связано с CRISPR. |
| 2 | [Draft impactful documents using AI - Training](<https://learn.microsoft.com/en-us/training/modules/draft-impactful-documents-using-ai/>) | 0 | Редактирование документов Word не соответствует генетическому смыслу editing. |
| 3 | [Tutorial: Get Started Creating in the Power BI Service - Power BI](<https://learn.microsoft.com/en-us/power-bi/fundamentals/service-get-started>) | 0 | Режим редактирования отчётов не относится к редактированию генов. |
| 4 | [Share agents with other users - Microsoft Copilot Studio](<https://learn.microsoft.com/en-us/microsoft-copilot-studio/admin-share-bots>) | 0 | Совместное редактирование тем бота не является редактированием генома. |
| 5 | [Collaborate on canvas apps in Microsoft Power Apps Studio - Power Apps](<https://learn.microsoft.com/en-us/power-apps/maker/canvas-apps/copresence-power-apps-studio>) | 0 | Редактирование Power Apps — другая задача. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Fast Antibiotic resistance-Based gene editing of mammalian cells with CRISPR-Cas9 (FAB-CRISPR)](<https://arxiv.org/abs/2502.12675v1>) | 2 | Предоставляет практический протокол CRISPR-Cas9 с HDR-донором и отбором отредактированных клеток. |
| 2 | [Search and localization dynamics of the CRISPR/Cas9 system](<https://arxiv.org/abs/2103.10667v2>) | 2 | Объясняет поиск целевой ДНК и разрезание Cas9 через модель диффузии; часть механизма редактирования. |
| 3 | [DeepFM-Crispr: Prediction of CRISPR On-Target Effects via Deep Learning](<https://arxiv.org/abs/2409.05938v1>) | 1 | Основная модель относится к Cas13d и РНК, а Cas9 приведён во введении как исходная технология. |
| 4 | [COMET: Combinatorial Optimization for Multiplex Editing Targets Via Constraint-Preserving QAOA](<https://arxiv.org/abs/2607.02622v1>) | 1 | Оптимизирует выбор guide RNA квантовым алгоритмом, не раскрывая сам механизм редактирования Cas9. |
| 5 | [Securing the Language of Life: Inheritable Watermarks from DNA Language Models to Proteins](<https://arxiv.org/abs/2509.18207v1>) | 1 | CRISPR-Cas9 используется как пример для водяных знаков ДНК; механизм редактирования не объясняется. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [CRISPR - Wikipedia](<https://en.wikipedia.org/wiki/CRISPR>) | 3 | Объясняет Cas9, направляющую РНК, разрез ДНК и использование механизмов репарации. |
| 2 | [CRISPR gene editing - Wikipedia](<https://en.wikipedia.org/wiki/CRISPR_gene_editing>) | 3 | Основная статья о CRISPR gene editing непосредственно отвечает намерению. |

В сохранённой выборке только 2 результатов.

## Q27. protein folding prediction

Найти предсказание структуры белка по последовательности и основные подходы.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Ferredoxin fold](<https://en.wikipedia.org/wiki/Ferredoxin_fold>) | 1 | Карточка конкретной белковой складки — структурный фон, не алгоритм предсказания. |
| 2 | [CAFASP](<https://en.wikipedia.org/wiki/CAFASP>) | 2 | Объясняет оценку автоматического предсказания и классы методов: homology, fold recognition, ab initio. |
| 3 | [TMPad](<https://en.wikipedia.org/wiki/TMPad>) | 1 | База укладки мембранных спиралей — ресурс структурных данных, а не метод предсказания. |
| 4 | [Beta solenoid](<https://en.wikipedia.org/wiki/Beta_solenoid>) | 1 | Описывает тип белковой складки, а не методы предсказания структуры. |
| 5 | [Contact order](<https://en.wikipedia.org/wiki/Contact_order>) | 1 | Параметр контактного порядка связан с моделированием, но не даёт полного метода предсказания. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [AlphaFold](<https://en.wikipedia.org/wiki/AlphaFold>) | 3 | AlphaFold напрямую предсказывает структуру белка по аминокислотной последовательности. |
| 2 | [Protein folding](<https://en.wikipedia.org/wiki/Protein_folding>) | 2 | Разбирает физическое сворачивание и вычислительные задачи стабильности, кинетики и структуры. |
| 3 | [Protein structure prediction](<https://en.wikipedia.org/wiki/Protein_structure_prediction>) | 3 | Специальная статья о предсказании белковой структуры по последовательности. |
| 4 | [List of protein structure prediction software](<https://en.wikipedia.org/wiki/List_of_protein_structure_prediction_software>) | 2 | Систематизирует программные средства и классы методов предсказания структуры. |
| 5 | [Folding@home](<https://en.wikipedia.org/wiki/Folding@home>) | 2 | Моделирование сворачивания связывается с улучшением методов структурного предсказания. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Power Query documentation - Power Query](<https://learn.microsoft.com/en-us/power-query/>) | 0 | Импорт и преобразование данных Power Query не объясняют предсказание белков. |
| 2 | [DirectQuery in Power BI: When to Use, Limitations, Alternatives - Power BI](<https://learn.microsoft.com/en-us/power-bi/connect-data/desktop-directquery-about>) | 0 | Query folding Power BI — другой смысл folding, не белковые структуры. |
| 3 | [Power Query PostgreSQL connector - Power Query](<https://learn.microsoft.com/en-us/power-query/connectors/postgresql>) | 0 | Native query folding PostgreSQL не относится к сворачиванию белка. |
| 4 | [Configure incremental refresh and real-time data for Power BI semantic models - Power BI](<https://learn.microsoft.com/en-us/power-bi/connect-data/incremental-refresh-overview>) | 0 | Сворачивание SQL-запроса не относится к protein folding. |
| 5 | [Microsoft Discovery agent catalog](<https://learn.microsoft.com/en-us/azure/microsoft-discovery/concept-agent-catalog>) | 2 | Каталог инструментов прямо включает AlphaFold, предсказанные структуры и оценки уверенности. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [AlphaFold predicts the most complex protein knot and composite protein knots](<https://arxiv.org/abs/2207.07410v1>) | 2 | Показывает применение AlphaFold к предсказанию белковых структур с узлами, без общего обзора методов. |
| 2 | [Introduction to Protein Folding](<https://arxiv.org/abs/2307.02174v2>) | 1 | Глава объясняет физику сворачивания, а не предсказание структуры по аминокислотной последовательности. |
| 3 | [Thermodynamics of Protein Folding](<https://arxiv.org/abs/2307.02175v2>) | 1 | Глава посвящена термодинамике и ландшафту свободной энергии, не алгоритмам предсказания структуры. |
| 4 | [The effects of non-native interactions on protein folding rates: Theory and simulation](<https://arxiv.org/abs/q-bio/0407023v1>) | 1 | Предсказывает скорость сворачивания в физической модели, а не неизвестную структуру белка. |
| 5 | [Crucial stages of protein folding through a solvable model: predicting target sites for enzyme-inhibiting drugs](<https://arxiv.org/abs/cond-mat/0209325v1>) | 1 | Исходит из известной нативной топологии для поиска участков лекарственного воздействия; задача другая. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Protein - Wikipedia](<https://en.wikipedia.org/wiki/Protein>) | 1 | Общая статья о белках даёт биологический фон без целевого разбора предсказания. |

В сохранённой выборке только 1 результатов.

## Q28. semiconductor doping p n junction

Найти легирование полупроводников и образование p–n-перехода.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Homojunction](<https://en.wikipedia.org/wiki/Homojunction>) | 3 | Непосредственно объясняет границу n- и p-легированных областей и встроенный потенциал. |
| 2 | [Gas immersion laser doping](<https://en.wikipedia.org/wiki/Gas_immersion_laser_doping>) | 2 | Конкретный технологический метод легирования и получения p-типа. |
| 3 | [Low-level injection](<https://en.wikipedia.org/wiki/Low-level_injection>) | 1 | Режим инжекции в готовом переходе — смежный фон, не образование перехода. |
| 4 | [Zener effect](<https://en.wikipedia.org/wiki/Zener_effect>) | 1 | Обратный пробой p–n-перехода — частный эффект, а не его образование. |
| 5 | [Donor (semiconductors)](<https://en.wikipedia.org/wiki/Donor_(semiconductors)>) | 2 | Объясняет донорное легирование и получение n-типа — существенную часть задачи. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Doping (semiconductor)](<https://en.wikipedia.org/wiki/Doping_(semiconductor)>) | 3 | Специальная статья о легировании полупроводников и формировании нужных областей. |
| 2 | [P–n junction](<https://en.wikipedia.org/wiki/P–n_junction>) | 3 | Основная статья о p–n-переходе отвечает намерению. |
| 3 | [Extrinsic semiconductor](<https://en.wikipedia.org/wiki/Extrinsic_semiconductor>) | 3 | Прямо связывает доноры/акцепторы, n/p-области и полупроводниковый переход. |
| 4 | [Metal–semiconductor junction](<https://en.wikipedia.org/wiki/Metal–semiconductor_junction>) | 1 | Контакт металл–полупроводник даёт сравнение, но отличается от p–n-перехода. |
| 5 | [Semiconductor](<https://en.wikipedia.org/wiki/Semiconductor>) | 3 | Объясняет донорное и акцепторное легирование и образование переходов в кристалле. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [mklink](<https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/mklink>) | 0 | Команда создания ссылок не объясняет физику полупроводника. |
| 2 | [Hard Links and Junctions - Win32 apps](<https://learn.microsoft.com/en-us/windows/win32/fileio/hard-links-and-junctions>) | 0 | Файловые ссылки и junctions — другая область значения терма. |
| 3 | [Junction - Sysinternals](<https://learn.microsoft.com/en-us/sysinternals/downloads/junction>) | 0 | Утилита файловых junction points не относится к p–n-переходу. |
| 4 | [PSHpcStorageTarget.Junctions Property (Microsoft.Azure.PowerShell.Cmdlets.HPCCache.Models)](<https://learn.microsoft.com/en-us/dotnet/api/microsoft.azure.powershell.cmdlets.hpccache.models.pshpcstoragetarget.junctions>) | 0 | Свойство namespace junction не относится к полупроводниковому переходу. |
| 5 | [Azure Accelerates Cloud Transformation for the Semiconductor Industry](<https://learn.microsoft.com/en-us/shows/azure-videos/azure-accelerates-cloud-transformation-for-the-semiconductor-industry>) | 0 | Видео о применении облака в отрасли не объясняет легирование и образование перехода. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Ferroelectric switched all-metallic-oxide $p$-$n$ junctions](<https://arxiv.org/abs/cond-mat/0611501v1>) | 1 | Рассматривает металлические оксидные переходы в сравнении с полупроводниками, а не обычное донорное и акцепторное легирование. |
| 2 | [Current-Voltage Characteristics of Graphane p-n Junctions](<https://arxiv.org/abs/0905.2812v2>) | 2 | Связывает легирование графана с образованием p/n-областей и вольт-амперной характеристикой перехода. |
| 3 | [Bipolar doping in van der Waals semiconductor through Flexo-doping](<https://arxiv.org/abs/2505.05887v1>) | 2 | Объясняет сдвиги донорных и акцепторных уровней и создание p/n-областей механической деформацией. |
| 4 | [A plausible method of preparing the ideal p-n junction interface of a thermoelectric material by surface doping](<https://arxiv.org/abs/2007.09393v1>) | 2 | Показывает поверхностное легирование, перенос заряда и формирование n-области на p-полупроводнике. |
| 5 | [Spatial BCS-BEC crossover in superconducting p-n junctions](<https://arxiv.org/abs/1912.09699v2>) | 2 | Исследует свойства перехода между легированными полупроводниками в сверхпроводящем режиме; специальный случай. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Semiconductor - Wikipedia](<https://en.wikipedia.org/wiki/Semiconductor>) | 3 | Объясняет донорное и акцепторное легирование и образование переходов в кристалле. |
| 2 | [Semiconductor industry - Wikipedia](<https://en.wikipedia.org/wiki/Semiconductor_industry>) | 0 | Экономика полупроводниковой отрасли не объясняет легирование и переход. |

В сохранённой выборке только 2 результатов.

## Q29. public key cryptography RSA

Найти принцип RSA как криптосистемы с открытым ключом.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [PROV_RSA_FULL](<https://learn.microsoft.com/en-us/windows/win32/seccrypto/prov-rsa-full>) | 1 | Справочник криптопровайдера с упоминанием RSA, без устройства алгоритма. |
| 2 | [PROV_RSA_AES](<https://learn.microsoft.com/en-us/windows/win32/seccrypto/prov-rsa-aes>) | 1 | Перечень алгоритмов криптопровайдера не объясняет принцип RSA. |
| 3 | [List of cryptosystems](<https://en.wikipedia.org/wiki/List_of_cryptosystems>) | 0 | Перечень криптосистем не содержит самостоятельного объяснения RSA. |
| 4 | [Capstone (cryptography)](<https://en.wikipedia.org/wiki/Capstone_(cryptography)>) | 0 | История проекта стандартизации не раскрывает принцип RSA. |
| 5 | [2.2.9.1.6 PUBLICKEY](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rmpr/063240db-ee5e-4ea9-9c32-0c36850d55b1>) | 2 | Описывает открытый RSA-ключ через публичную экспоненту и модуль. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [RSA cryptosystem](<https://en.wikipedia.org/wiki/RSA_cryptosystem>) | 3 | Основная статья о RSA раскрывает ключи, операции и назначение криптосистемы. |
| 2 | [Public-key cryptography](<https://en.wikipedia.org/wiki/Public-key_cryptography>) | 2 | Объясняет принцип пары открытого/закрытого ключа и связь с RSA. |
| 3 | [RSA problem](<https://en.wikipedia.org/wiki/RSA_problem>) | 2 | Математическая RSA-проблема объясняет предположение сложности, важное для безопасности. |
| 4 | [Key (cryptography)](<https://en.wikipedia.org/wiki/Key_(cryptography)>) | 1 | Общее определение ключей — фон, без математического устройства RSA. |
| 5 | [Elliptic-curve cryptography](<https://en.wikipedia.org/wiki/Elliptic-curve_cryptography>) | 1 | Альтернативная криптосистема сравнивает размеры ключей с RSA, но не объясняет RSA. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Windows Secure Boot Key Creation and Management Guidance](<https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/windows-secure-boot-key-creation-and-management-guidance>) | 2 | Разбирает связанную пару ключей и применение RSA для подписи Secure Boot. |
| 2 | [Key-Based Authentication in OpenSSH for Windows](<https://learn.microsoft.com/en-us/windows-server/administration/openssh/openssh_keymanagement>) | 1 | Настройка SSH-ключей объясняет общий принцип пары, но не устройство RSA. |
| 3 | [Trusted Platform Module (TPM) overview](<https://learn.microsoft.com/en-us/windows/security/hardware-security/tpm/trusted-platform-module-overview>) | 1 | Использование RSA-ключа в TPM даёт прикладной фон, не алгоритм RSA. |
| 4 | [Create a self-signed public certificate to authenticate your application - Microsoft identity platform](<https://learn.microsoft.com/en-us/entra/identity-platform/howto-create-self-signed-certificate>) | 2 | Объясняет разные роли открытого и закрытого ключей в конкретной схеме аутентификации. |
| 5 | [Azure Certificate Authority details](<https://learn.microsoft.com/en-us/azure/security/fundamentals/azure-certificate-authority-details>) | 1 | Перечень размеров RSA-ключей и алгоритмов сертификатов даёт практический фон, не принцип RSA. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Information Hiding in CSS : A Secure Scheme Text-Steganography using Public Key Cryptosystem](<https://arxiv.org/abs/1201.1968v1>) | 1 | RSA используется как готовый этап шифрования в CSS-стеганографии; принцип самой криптосистемы не раскрывается. |
| 2 | [Public Key Cryptography Standards: PKCS](<https://arxiv.org/abs/1207.5446v1>) | 2 | Описывает PKCS, включая стандарты шифрования и подписи RSA, то есть существенный практический аспект. |
| 3 | [RSA+: An RSA variant](<https://arxiv.org/abs/2301.01282v2>) | 2 | Предлагает вариант RSA/Rabin и сравнивает безопасность и производительность; частичный ответ о конструкции RSA. |
| 4 | [Fault Attacks on RSA Public Keys: Left-To-Right Implementations are also Vulnerable](<https://arxiv.org/abs/0901.0911v3>) | 2 | Разбирает открытый модуль RSA, возведение в степень и fault-атаки; содержательный аспект механизма и безопасности. |
| 5 | [State of the art parallel approaches for RSA public key based cryptosystem](<https://arxiv.org/abs/1503.03593v1>) | 2 | Обзор параллельных реализаций RSA рассматривает шифрование, подпись и вычислительные затраты, без базового полного вывода. |

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Public - Wikipedia](<https://en.wikipedia.org/wiki/Public>) | 0 | Социологическое понятие public не относится к криптографии. |

В сохранённой выборке только 1 результатов.

## Q30. variable byte compression inverted index

Найти Variable Byte-кодирование чисел при сжатии инвертированного индекса.

### Мой TF-IDF

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [2.1 Web Service ItemId Algorithm Details](<https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-oxwsitemid/5889be27-a84b-426b-9166-5f7a0ed5747d>) | 0 | Поля протокольного идентификатора не объясняют Variable Byte или инвертированный индекс. |
| 2 | [2.6.1 KeyExtensionData Structure](<https://learn.microsoft.com/en-us/openspecs/sharepoint_protocols/ms-cifo/2913e54b-fbb9-419b-8985-2ed05434bb9c>) | 1 | Описывает сжатые docID для ключа контентного индекса, но не алгоритм Variable Byte. |
| 3 | [3.2.4.2 Compression Processing](<https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-srpl/eed2ea22-9b20-47df-a9c7-6d124e3cb1b8>) | 0 | Сжатие потока протокола репликации не относится к VB для postings. |
| 4 | [2.1.3.1 Compression Type (byte)](<https://learn.microsoft.com/en-us/openspecs/exchange_server_protocols/ms-oxwsitemid/aacce5f0-459b-480b-a764-e6dee908a6cb>) | 0 | Флаг RLE-сжатия идентификатора описывает другой алгоритм и другую задачу. |
| 5 | [2.4.1.2 State Variables](<https://learn.microsoft.com/en-us/openspecs/office_file_formats/ms-ovba/c01d4c55-f44e-4ee1-b5cd-e179e4957d01>) | 0 | Переменные состояния буфера сжатия не объясняют VB-кодирование чисел. |

### Wikipedia Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Hash function](<https://en.wikipedia.org/wiki/Hash_function>) | 0 | Хеширование не является Variable Byte-кодированием. |
| 2 | [Bit array](<https://en.wikipedia.org/wiki/Bit_array>) | 1 | Сжатие битовых массивов даёт смежный контекст, но не VB для postings. |
| 3 | [Discrete cosine transform](<https://en.wikipedia.org/wiki/Discrete_cosine_transform>) | 0 | Косинусное преобразование относится к другому классу сжатия, не к VB. |
| 4 | [Digital data](<https://en.wikipedia.org/wiki/Digital_data>) | 0 | Обзор цифровых данных не объясняет искомое целочисленное кодирование. |
| 5 | [BLAKE (hash function)](<https://en.wikipedia.org/wiki/BLAKE_(hash_function)>) | 0 | Функция криптографического сжатия BLAKE не является Variable Byte-кодированием. |

### Microsoft Learn Search

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Define variables - Azure Pipelines](<https://learn.microsoft.com/en-us/azure/devops/pipelines/process/variables>) | 0 | Переменные Azure Pipelines не относятся к целочисленному сжатию. |
| 2 | [CREATE INDEX (Transact-SQL) - SQL Server](<https://learn.microsoft.com/en-us/sql/t-sql/statements/create-index-transact-sql>) | 0 | Команда CREATE INDEX с параметрами row/page compression не относится к VB. |
| 3 | [Index Architecture and Design Guide - SQL Server](<https://learn.microsoft.com/en-us/sql/relational-databases/sql-server-index-design-guide>) | 1 | Архитектура SQL-индексов даёт фон, но не инвертированный индекс с Variable Byte. |
| 4 | [ALTER INDEX (Transact-SQL) - SQL Server](<https://learn.microsoft.com/en-us/sql/t-sql/statements/alter-index-transact-sql>) | 0 | Синтаксис выбора row/page compression не объясняет VB-кодирование. |
| 5 | [Data compression - SQL Server](<https://learn.microsoft.com/en-us/sql/relational-databases/data-compression/data-compression>) | 1 | Сжатие таблиц и индексов SQL Server связано с общей задачей хранения, но не с VB postings. |

### arXiv Search API

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [On Optimally Partitioning Variable-Byte Codes](<https://arxiv.org/abs/1804.10949v2>) | 3 | Прямо рассматривает Variable Byte для целочисленных последовательностей и сжатия posting lists, включая разбиение и компромисс скорости и размера. |
| 2 | [Factorization-based Lossless Compression of Inverted Indices](<https://arxiv.org/abs/1108.1956v1>) | 1 | Основной метод — факторизация инвертированного индекса; Variable Byte назван как дополнительно комбинируемое кодирование, без его разбора. |

В сохранённой выборке только 2 результатов.

### Bing: ограниченная первая страница (дополнение)

| Позиция | Документ | Оценка | Обоснование |
|---:|---|---:|---|
| 1 | [Variable (mathematics ) - Wikipedia](<https://en.wikipedia.org/wiki/Variable_(mathematics)>) | 0 | Математическая переменная не относится к VB-сжатию. |
| 2 | [Variable - Wikipedia](<https://en.wikipedia.org/wiki/Variable>) | 0 | Страница неоднозначности variable не относится к алгоритму. |

В сохранённой выборке только 2 результатов.

