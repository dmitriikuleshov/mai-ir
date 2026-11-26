"""Локальная веб-страница: HTTP-запросы передаются постоянному C++-процессу поиска."""

import argparse
import asyncio
import json
import logging
from pathlib import Path

from aiohttp import web

ROOT = Path(__file__).resolve().parents[2]
ASSETS = Path(__file__).resolve().parent
MODES = {"tfidf", "boolean", "phrase"}
LOG = logging.getLogger("web_search")


class SearchWorker:
    """Последовательно обслуживает запросы одним процессом с открытыми индексами."""

    def __init__(self, executable: Path, config: Path):
        self.executable = executable
        self.config = config
        self.process = None
        self.lock = asyncio.Lock()

    async def close(self):
        """Завершает дочерний процесс при остановке сервера или сбое протокола."""
        if self.process is not None:
            if self.process.returncode is None:
                self.process.terminate()
            await self.process.wait()
            self.process = None

    async def request(self, payload):
        """Отправляет JSON-строку и получает один ответ; запрос не исполняется оболочкой."""
        async with self.lock:
            if self.process is None or self.process.returncode is not None:
                if not self.executable.is_file():
                    raise RuntimeError("Сначала соберите проект: cmake --build .build --parallel")
                self.process = await asyncio.create_subprocess_exec(
                    str(self.executable), str(self.config), cwd=ROOT,
                    stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE,
                    # stderr остаётся в терминале: диагностике не место в JSON-ответе.
                    limit=1024 * 1024,
                )
            try:
                async with asyncio.timeout(60):
                    self.process.stdin.write((json.dumps(payload) + "\n").encode())
                    await self.process.stdin.drain()
                    line = await self.process.stdout.readline()
                    if not line:
                        raise RuntimeError("Поисковый процесс остановлен. Проверьте индекс и конфигурацию в терминале.")
                    return json.loads(line)
            except (TimeoutError, RuntimeError, BrokenPipeError, ConnectionResetError,
                    json.JSONDecodeError, ValueError, asyncio.CancelledError):
                # После прерванного ответа процесс перезапускается: ответы не смешиваются.
                await self.close()
                raise


def create_app(worker):
    """Создаёт маршруты страницы, статических ресурсов и поискового API."""
    app = web.Application(client_max_size=16 * 1024)

    async def search(request):
        try:
            payload = await request.json()
        except (json.JSONDecodeError, UnicodeDecodeError):
            return web.json_response({"ok": False, "error": "Неверный JSON-запрос."}, status=400)
        if not isinstance(payload, dict):
            return web.json_response({"ok": False, "error": "Ожидается объект запроса."}, status=400)
        mode = payload.get("mode", "tfidf")
        query = payload.get("query")
        if not isinstance(mode, str) or mode not in MODES:
            return web.json_response({"ok": False, "error": "Выберите существующий режим поиска."}, status=400)
        if (not isinstance(query, str) or not query.strip() or len(query) > 1000
                or any(character in query for character in "\r\n\0")):
            return web.json_response({"ok": False, "error": "Введите запрос одной строкой, от 1 до 1000 символов."}, status=400)
        return await call_worker({"mode": mode, "query": query.strip()})

    async def call_worker(payload):
        try:
            response = await worker.request(payload)
            status = 200 if response.get("ok") else (
                503 if response.get("kind") == "unavailable" else 400)
            return web.json_response(response, status=status)
        except TimeoutError:
            return web.json_response({"ok": False, "error": "Поиск занял больше минуты. Попробуйте более точный запрос."}, status=504)
        except (RuntimeError, OSError, ValueError) as error:
            LOG.error("Не удалось выполнить поиск: %s", error)
            return web.json_response({"ok": False, "error": str(error)}, status=503)

    async def info(request):
        return await call_worker({"mode": "status"})

    async def index(request):
        return web.FileResponse(ASSETS / "index.html")

    async def asset(request):
        return web.FileResponse(ASSETS / request.match_info["name"])

    async def cleanup(app):
        await worker.close()

    app.router.add_get("/", index)
    app.router.add_get("/api/info", info)
    app.router.add_post("/api/search", search)
    app.router.add_get("/{name:app.js|styles.css}", asset)
    app.on_cleanup.append(cleanup)
    return app


def main():
    """Запускает HTTP-сервер; существующий search.yaml используется без копирования настроек."""
    parser = argparse.ArgumentParser(description="Веб-интерфейс поиска по техническому корпусу")
    parser.add_argument("config", nargs="?", default="configs/search.yaml")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--build-dir", default=".build")
    args = parser.parse_args()
    config = (ROOT / args.config).resolve()
    executable = (ROOT / args.build_dir / "web_search").resolve()
    if not config.is_file():
        parser.error(f"Конфигурация не найдена: {config}")
    logging.basicConfig(level=logging.INFO, format="%(levelname)s | %(message)s")
    web.run_app(create_app(SearchWorker(executable, config)),
                host=args.host, port=args.port, access_log=None)


if __name__ == "__main__":
    main()
