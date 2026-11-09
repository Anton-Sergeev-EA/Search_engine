# Search Engine

[Русский](#русский) · [English](#english)

## Русский

Search Engine — поисковая система на C++20 для локальных текстовых файлов
в UTF-8. Индексатор рекурсивно обходит каталог и параллельно подсчитывает
частоты слов. Индекс хранится в PostgreSQL, поиск доступен через консоль
и HTTP API.

### Запуск через Docker Compose

Понадобятся Docker и Docker Compose v2. Поместите файлы в каталог `documents/`.
Создайте `config.ini` на основе [примера](config.example.ini), задайте свой
пароль в `[database].password` и установите следующие значения.

| Секция | Параметры |
| --- | --- |
| `[database]` | `host=db`, `port=5432`, `name=search_engine`, `user=search_engine` |
| `[indexer]` | `directory=/documents`, `extensions=txt,md,ini,csv` |
| `[search]` | `address=0.0.0.0`, `port=8080` |

В Bash из корня проекта выполните команды ниже. При запросе пароля введите
то же значение, что и в `config.ini`.

```bash
chmod 600 config.ini
export SEARCH_UID="$(id -u)" SEARCH_GID="$(id -g)"
read -rsp 'Пароль PostgreSQL: ' SEARCH_DB_PASSWORD
printf '\n'
export SEARCH_DB_PASSWORD
docker compose --env-file /dev/null up --build -d
```

Отправьте запрос к HTTP API.

```bash
curl --fail-with-body --data-binary 'привет мир' http://127.0.0.1:8080/search
```

Compose запускает поиск после успешной индексации. HTTP-порт опубликован на
`127.0.0.1:8080`. После изменения файлов запустите индексацию повторно.
Остановка через `down` сохраняет данные PostgreSQL в томе.

```bash
docker compose --env-file /dev/null run --rm indexer
```

Остановите сервисы командой ниже.

```bash
docker compose --env-file /dev/null down
```

### Поиск и обновление индекса

`POST /search` принимает текст запроса в UTF-8 размером до 4096 байт.
После нормализации запрос должен содержать от одного до четырёх слов.
Повторы учитываются один раз. Поиск возвращает до десяти файлов, содержащих
все слова запроса. Оценка — сумма их частот в файле. При равных оценках
результаты сортируются по пути. Ответ содержит оценку и путь через табуляцию,
по одной строке на файл. При отсутствии совпадений ответ содержит `No results`.

Индексатор пропускает символические ссылки. Повторный запуск заменяет индекс
выбранного каталога одной транзакцией и убирает записи об исчезнувших файлах.
Если обработка завершается ошибкой, предыдущий индекс остаётся доступен.

### Локальная сборка и тесты

Нужны компилятор C++20, CMake 3.20+, pkg-config, ICU, libpqxx 7.x и Boost 1.74+.
Для интеграционных тестов также нужны Python 3 и серверные утилиты PostgreSQL
`initdb`, `pg_ctl` и `psql`. Запускайте тесты от обычного пользователя.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSEARCH_POSTGRES_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Для запуска без Docker создайте базу PostgreSQL и укажите в INI её адрес,
учётные данные пользователя с правом создавать таблицы и путь к документам.
Относительный путь считается от каталога INI-файла.

```sh
./build/search_indexer config.ini
./build/search_query config.ini
```

В консоли вводите по одному запросу на строку. Для HTTP вместо консоли
запустите `./build/search_query config.ini --http`. GitHub Actions запускает
CTest и сквозную проверку Docker Compose, которую можно повторить командой
`python3 tests/compose_test.py`.

## English

Search Engine is a C++20 search engine for local UTF-8 text files. The indexer
scans directories recursively and counts word frequencies in parallel. PostgreSQL
stores the index, and queries run through the console or HTTP API.

### Run with Docker Compose

Install Docker and Docker Compose v2. Put your files in `documents/`.
Create `config.ini` from the [example](config.example.ini), set your own
password in `[database].password`, and use the settings below.

| Section | Settings |
| --- | --- |
| `[database]` | `host=db`, `port=5432`, `name=search_engine`, `user=search_engine` |
| `[indexer]` | `directory=/documents`, `extensions=txt,md,ini,csv` |
| `[search]` | `address=0.0.0.0`, `port=8080` |

From the project root, run the following commands in Bash. At the prompt,
enter the same password as in `config.ini`.

```bash
chmod 600 config.ini
export SEARCH_UID="$(id -u)" SEARCH_GID="$(id -g)"
read -rsp 'PostgreSQL password: ' SEARCH_DB_PASSWORD
printf '\n'
export SEARCH_DB_PASSWORD
docker compose --env-file /dev/null up --build -d
```

Send a query to the HTTP API.

```bash
curl --fail-with-body --data-binary 'hello world' http://127.0.0.1:8080/search
```

Compose starts search after indexing succeeds. HTTP is published on
`127.0.0.1:8080`. Rerun the indexer after changing files. Stopping the stack
with `down` keeps PostgreSQL data in its volume.

```bash
docker compose --env-file /dev/null run --rm indexer
```

Stop the services with the following command.

```bash
docker compose --env-file /dev/null down
```

### Search and reindexing

`POST /search` accepts a UTF-8 query of up to 4096 bytes. A query must contain
one to four words after normalization. Repeated words count once. Search returns
up to ten files containing every query term. The score is the sum of their
frequencies in each file, with ties sorted by path. The response contains a
tab-separated score and path per line, or `No results` when nothing matches.

The indexer skips symbolic links. Rerunning it replaces the selected directory's
index in a single transaction and drops records for missing files. If indexing
fails, the previous index stays available.

### Build and test locally

You need a C++20 compiler, CMake 3.20+, pkg-config, ICU, libpqxx 7.x, and
Boost 1.74+. Integration tests also require Python 3 and PostgreSQL server tools
`initdb`, `pg_ctl`, and `psql`. Run tests as a regular user.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSEARCH_POSTGRES_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

To run without Docker, create a PostgreSQL database and set its address,
credentials for a user with permission to create tables, and the document
directory in the INI file. Relative paths resolve from the INI file's directory.

```sh
./build/search_indexer config.ini
./build/search_query config.ini
```

Enter one query per line. For HTTP, run
`./build/search_query config.ini --http`. GitHub Actions runs CTest and the
Docker Compose end-to-end check. Run the latter locally with
`python3 tests/compose_test.py`.
