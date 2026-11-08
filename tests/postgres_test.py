"""Run integration checks against an isolated PostgreSQL cluster."""

import getpass
import http.client
import socket as network
import time
import subprocess
import sys
import tempfile
from pathlib import Path


def run(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(arguments, check=True, capture_output=True, text=True)


def check_search(searcher: str, config: Path, indexer: str, documents: Path) -> None:
    (documents / "one.txt").write_text("привет привет мир", encoding="utf-8")
    (documents / "two.txt").write_text("привет мир", encoding="utf-8")
    (documents / "partial.txt").write_text("привет " * 10, encoding="utf-8")
    run([indexer, str(config)])
    console = subprocess.run(
        [searcher, str(config)],
        input="ПРИВЕТ, мир\nничего\n",
        check=True,
        text=True,
        capture_output=True,
    ).stdout
    assert console.index("3\t") < console.index("2\t")
    assert "No results" in console and "partial.txt" not in console
    with network.socket() as reserved:
        reserved.bind(("127.0.0.1", 0))
        port = reserved.getsockname()[1]
    with config.open("a", encoding="utf-8") as output:
        output.write(f"\n[search]\nport={port}\n")

    def request(method: str, path: str, body: str = "") -> tuple[int, str]:
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
        try:
            connection.request(method, path, body.encode("utf-8"))
            response = connection.getresponse()
            return response.status, response.read().decode("utf-8")
        finally:
            connection.close()

    server = subprocess.Popen(
        [searcher, str(config), "--http"], stdout=subprocess.DEVNULL
    )
    try:
        for _ in range(100):
            try:
                status, body = request("POST", "/search", "ПРИВЕТ, мир мир")
                break
            except ConnectionRefusedError:
                if server.poll() is not None:
                    raise
                time.sleep(0.02)
        else:
            raise RuntimeError("Search server did not start")
        assert status == 200
        assert body.splitlines() == [
            f"3\t{documents / 'one.txt'}",
            f"2\t{documents / 'two.txt'}",
        ]
        assert request("POST", "/search", "слово")[1].count("\n") == 10
        assert request("POST", "/search", "ничего") == (200, "No results\n")
        assert request("POST", "/search", "один два три четыре пять")[0] == 400
        assert request("POST", "/search", " ")[0] == 400
        assert request("POST", "/search", "x" * 4097)[0] == 413
        assert request("GET", "/search")[0] == 405
        assert request("POST", "/missing")[0] == 404
    finally:
        server.terminate()
        server.wait(timeout=10)


def main() -> None:
    binaries = Path(sys.argv[1])
    database_test = sys.argv[2]
    indexer = sys.argv[3]
    root = Path(tempfile.mkdtemp(prefix="search-pg-"))
    data = root / "data"
    socket = root / "socket"
    socket.mkdir()
    documents = root / "documents"
    documents.mkdir()
    run(
        [
            str(binaries / "initdb"),
            "-D",
            str(data),
            "-A",
            "trust",
            "--no-locale",
            "--encoding=UTF8",
        ]
    )
    control = [str(binaries / "pg_ctl"), "-D", str(data)]
    run(
        [
            *control,
            "-l",
            str(root / "postgres.log"),
            "-o",
            f"-F -k {socket} -h '' -p 5432",
            "-w",
            "start",
        ]
    )
    try:
        user = getpass.getuser()
        unit_documents = root / "unit-documents"
        unit_documents.mkdir()
        run([database_test, str(socket), user, str(unit_documents)])
        config = root / "settings.ini"
        config.write_text(
            f"[database]\nhost={socket}\nport=5432\nname=postgres\n"
            f"user={user}\npassword=\n[indexer]\ndirectory={documents}\nextensions=txt\n",
            encoding="utf-8",
        )
        (documents / "one.txt").write_text("Привет мир привет", encoding="utf-8")
        (documents / "two.txt").write_text("мир", encoding="utf-8")
        run([indexer, str(config)])
        query = [
            str(binaries / "psql"),
            "-h",
            str(socket),
            "-p",
            "5432",
            "-U",
            user,
            "-d",
            "postgres",
            "-At",
            "-c",
        ]
        assert (
            run([*query, "SELECT SUM(frequency) FROM document_words"]).stdout.strip()
            == "4"
        )
        (documents / "one.txt").write_text("обновление", encoding="utf-8")
        with subprocess.Popen(
            [indexer, str(config)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        ) as first:
            run([indexer, str(config)])
            first.communicate(timeout=20)
            assert first.returncode == 0
        assert (
            run([*query, "SELECT SUM(frequency) FROM document_words"]).stdout.strip()
            == "2"
        )
        assert run([*query, "SELECT COUNT(*) FROM documents"]).stdout.strip() == "2"
        for number in range(24):
            (documents / f"extra-{number}.txt").write_text(
                "слово " * 3000, encoding="utf-8"
            )
        run([indexer, str(config)])
        assert (
            run([*query, "SELECT SUM(frequency) FROM document_words"]).stdout.strip()
            == "72002"
        )
        blocked = documents / "extra-0.txt"
        blocked.chmod(0)
        try:
            assert (
                subprocess.run([indexer, str(config)], capture_output=True).returncode
                == 1
            )
        finally:
            blocked.chmod(0o600)
        assert (
            run([*query, "SELECT SUM(frequency) FROM document_words"]).stdout.strip()
            == "72002"
        )
        assert run([*query, "SELECT COUNT(*) FROM documents"]).stdout.strip() == "26"
        # Переиндексация пустого набора убирает прежние документы каталога.
        config.write_text(
            config.read_text(encoding="utf-8").replace(
                "extensions=txt", "extensions=md"
            ),
            encoding="utf-8",
        )
        run([indexer, str(config)])
        assert run([*query, "SELECT COUNT(*) FROM documents"]).stdout.strip() == "0"
        assert subprocess.run([indexer], capture_output=True).returncode == 2
        assert (
            subprocess.run(
                [indexer, str(root / "absent.ini")], capture_output=True
            ).returncode
            == 1
        )
        config.write_text(
            config.read_text(encoding="utf-8").replace(
                "extensions=md", "extensions=txt"
            ),
            encoding="utf-8",
        )
        check_search(sys.argv[4], config, indexer, documents)
    finally:
        run([*control, "-m", "fast", "-w", "stop"])


if __name__ == "__main__":
    main()
