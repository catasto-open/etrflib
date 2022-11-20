from pathlib import Path
import rich.progress
from rich.console import Console
from rich.panel import Panel
from rich.markdown import Markdown
import pyfiglet
import typer
import aiofiles
from pydantic import BaseModel, FilePath, AnyUrl, validator
from miniopy_async import Minio
from aiohttp.client_exceptions import ClientConnectorError
import dacsagb


console = Console()
app = typer.Typer(help="ETRF converter CLI application for CXF.")


class CXFModel(BaseModel):
    cxf: FilePath

    @validator("cxf")
    def is_cxf(cls, value):
        if not value.suffix.upper() == ".CXF":
            raise ValueError(
                f"File {value.name} is not a valid CXF"
            )
        return value


class UrlModel(BaseModel):
    url: AnyUrl


@app.command("info")
def info():
    """
    Provide information how to convert a CXF.
    """
    result = pyfiglet.figlet_format("ETRFLIB", font="banner3-D")
    console.print(Panel.fit(result, title="Welcome", subtitle="Thank you"))
    info = """
    # How to convert a CXF

    Etrflib is a command line tool that can operate in two different ways:

    1. Locally: use the local file system to process the file
    2. Remotely: use a remote file system (S3-like) to process the file
    
    Please use the help command to understand how to use this tool.
    """
    console.print(Panel.fit(Markdown(info), title="Information"))


@app.command("convert")
def convert(
    filepath: Path = typer.Option(
        ...,
        exists=True,
        file_okay=True,
        dir_okay=False,
        writable=False,
        readable=True,
        resolve_path=True,
    ),
    out_filename: str = None,
    log_filename: str = None,
    libdir: Path = typer.Option(
        None,
        exists=True,
        file_okay=True,
        dir_okay=False,
        writable=False,
        readable=True,
        resolve_path=True,
    )
):
    """
    Convert a CXF INPUTFILE to ETRF2000.
    """
    basepath = Path("/tmp")
    libdir = basepath / "data"
    cxf_file = CXFModel(cxf=filepath)
    in_filename = cxf_file.cxf.stem
    if out_filename:
        outfile = Path(out_filename)
    else:
        outfile = cxf_file.cxf.parent / f"{in_filename}.ctf"
    if log_filename:
        logfile = Path(log_filename)
    else:
        logfile = cxf_file.cxf.parent / f"{in_filename}.log"
    p = dacsagb.dacsagb()
    with rich.progress.open(cxf_file.cxf, "r") as file:
        result = p.calcolaCXF(
            cxf_file.cxf.__str__(),
            outfile.__str__(),
            logfile.__str__(),
            libdir.__str__(),
            4
        )
    if result:
        console.print(Panel.fit(
            f"[green]ETRF file {outfile} is created![/green] :boom:",
            title="Message"
        ))
    else:
        raise typer.Exit(code=1)


@app.async_command("remote-convert")
async def convert_sfs(
    bucket_path: str = typer.Option(...),
    object_path: str = typer.Option(...),
    filename: str = typer.Option(...),
    destination_path: str = typer.Option(...),
    key: str = typer.Option(...),
    secret: str = typer.Option(...)
):
    basepath = Path(bucket_path)
    bucket_name = basepath.name
    filepath = Path(object_path) / filename
    endpoint = UrlModel(url=bucket_path)
    minio_client = Minio(
        f"{endpoint.url.host}:{endpoint.url.port}",
        access_key=key,
        secret_key=secret,
        secure=False
    )
    try:
        cxf_temp_file = Path(f"/tmp") / filename
        async with aiofiles.open(cxf_temp_file, mode='w') as cxfile:
            cxf = await minio_client.get_object(
                bucket_name,
                filepath.__str__()           
            )
            content = await cxf.read()
            await cxfile.write(content.decode())
            cxf.close()
        ctf_filename = filename.replace("cxf", "ctf")
        log_filename = filename.replace("cxf", "log")
        dest_path = Path(f"/tmp") / destination_path
        dest_path.mkdir(parents=True, exist_ok=True)
        ctf_temp_file = Path(f"/tmp") / dest_path / ctf_filename
        log_temp_file = Path(f"/tmp") / dest_path / log_filename
        libdir = Path(f"/tmp") / "data"
        p = dacsagb.dacsagb()
        result = p.calcolaCXF(
            cxf_temp_file.__str__(),
            ctf_temp_file.__str__(),
            log_temp_file.__str__(),
            libdir.__str__(),
            4
        )
        if result:
            async with aiofiles.open(cxf_temp_file, 'r') as cxffile:
                upload_cxf = await minio_client.fput_object(
                    bucket_name,
                    f"{object_path}/{destination_path}/{filename}",
                    cxf_temp_file,
                    progress=True
                )
            async with aiofiles.open(ctf_temp_file, 'r') as ctffile:
                upload_ctf = await minio_client.fput_object(
                    bucket_name,
                    f"{object_path}/{destination_path}/{ctf_filename}",
                    ctf_temp_file,
                    progress=True
                )
            async with aiofiles.open(log_temp_file, 'r') as logfile:
                upload_log = await minio_client.fput_object(
                    bucket_name,
                    f"{object_path}/{destination_path}/{log_filename}",
                    log_temp_file,
                    progress=True
                )
            console.print(Panel.fit(
                f"[green]ETRF object {destination_path} is created![/green] :boom:",
                title="Message"
            ))
        else:
            raise typer.Exit(code=1)
    except ClientConnectorError as ex:
        raise ex


if __name__ == "__main__":
    app()
