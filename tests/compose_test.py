"""Build and exercise the Compose stack with a disposable database."""

import http.client
import os
import secrets
import subprocess
import tempfile
import time
from pathlib import Path


def main() -> None:
    project = Path(__file__).resolve().parent.parent
    build = project / "build"
    build.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="search-compose-", dir=build) as temporary:
        root = Path(temporary)
        documents = root / "documents"
        documents.mkdir()
        (documents / "first.txt").write_text("привет привет мир", encoding="utf-8")
        (documents / "second.txt").write_text("привет мир", encoding="utf-8")
        (documents / "partial.txt").write_text("привет " * 10, encoding="utf-8")

        password = secrets.token_hex(24)
        config = root / "config.ini"
        config.touch(mode=0o600)
        config.write_text(
            f"[database]\nhost=db\nport=5432\nname=search_engine\n"
            f"user=search_engine\npassword={password}\n"
            "[indexer]\ndirectory=/documents\nextensions=txt\n"
            "[search]\naddress=0.0.0.0\nport=8080\n",
            encoding="utf-8",
        )
        environment = {
            **os.environ,
            "SEARCH_CONFIG": str(config),
            "SEARCH_DOCUMENTS": str(documents),
            "SEARCH_DB_PASSWORD": password,
            "SEARCH_UID": str(os.getuid()),
            "SEARCH_GID": str(os.getgid()),
            "SEARCH_HTTP_PORT": "0",
        }

        def compose(*arguments: str, capture: bool = False) -> str:
            result = subprocess.run(
                [
                    "docker",
                    "compose",
                    "--env-file",
                    "/dev/null",
                    "--project-name",
                    f"search-test-{secrets_id}",
                    "-f",
                    str(project / "compose.yaml"),
                    *arguments,
                ],
                cwd=project,
                env=environment,
                check=True,
                capture_output=capture,
                text=True,
                timeout=600,
            )
            return result.stdout if capture else ""

        secrets_id = secrets.token_hex(5)
        try:
            compose("up", "--build", "-d")
            endpoint = compose("port", "search", "8080", capture=True).strip()
            port = int(endpoint.rsplit(":", 1)[1])

            def query() -> tuple[int, str]:
                connection = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
                try:
                    connection.request("POST", "/search", "ПРИВЕТ, мир".encode())
                    response = connection.getresponse()
                    return response.status, response.read().decode()
                finally:
                    connection.close()

            deadline = time.monotonic() + 15
            while True:
                try:
                    result = query()
                    break
                except (
                    ConnectionRefusedError,
                    ConnectionResetError,
                    http.client.RemoteDisconnected,
                ):
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.1)
            expected = (200, "3\t/documents/first.txt\n2\t/documents/second.txt\n")
            if result != expected:
                raise RuntimeError(f"Unexpected search response: {result!r}")

            compose("stop", "db")
            result = query()
            if result != (500, "Internal search error\n"):
                raise RuntimeError(
                    f"Unexpected response during database outage: {result!r}"
                )
            compose("up", "-d", "--wait", "db")
            result = query()
            if result != expected:
                raise RuntimeError(
                    f"Unexpected response after database restart: {result!r}"
                )
            print("Compose search and database recovery tests passed")
        except Exception:
            compose("logs", "--no-color")
            raise
        finally:
            compose("down", "--volumes")


if __name__ == "__main__":
    main()
