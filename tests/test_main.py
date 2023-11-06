from minio import Minio
from pathlib import Path
from typer.testing import CliRunner

from etrflib.main import app

runner = CliRunner()

current_path = Path.cwd()
local_data_dir = current_path / "data"


def test_convert_gauss_boaga():
    gauss_boaga_infile = local_data_dir / "H501D07670X.cxf"
    gauss_boaga_outfile = local_data_dir / "H501D07670X.ctf"
    result = runner.invoke(app, ["convert", "--filepath", gauss_boaga_infile.__str__()])
    assert result.exit_code == 0
    assert gauss_boaga_outfile.exists()
    assert gauss_boaga_outfile.read_text() == gauss_boaga_outfile.read_text()


def test_convert_remote_gauss_boaga(run_minio_container):
    gauss_boaga_infile = local_data_dir / "H501D07670X.cxf"
    # Create client with access and secret key
    client = Minio("localhost:9000", 
        "secret",  
        "secret123", 
        secure=False
    )
    # check if bucket already exists
    found = client.bucket_exists("etrflib")
    # Create bucket
    if not found:
        client.make_bucket("etrflib")
    else:
        print("Bucket 'etrflib' already exists")
    # upload file to MinIO bucket
    client.fput_object(
        "etrflib",
        "test/H501D07670X.cxf",
        gauss_boaga_infile
    )
    result = runner.invoke(
        app,
        [
            "remote-convert",
            "--bucket-path", "http://localhost:9000/etrflib",
            "--object-path", "test",
            "--filename", "H501D07670X.cxf",
            "--destination-path", "H501D07670X",
            "--key", "secret",
            "--secret", "secret123"
        ]
    )
    assert result.exit_code == 0
    run_minio_container.kill()
    run_minio_container.remove()
