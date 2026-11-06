"""Run integration checks against an isolated PostgreSQL cluster."""

import getpass
import subprocess
import sys
import tempfile
from pathlib import Path


def run(arguments: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(arguments, check=True, capture_output=True, text=True)


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
    finally:
        run([*control, "-m", "fast", "-w", "stop"])


if __name__ == "__main__":
    main()
