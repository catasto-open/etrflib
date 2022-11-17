from pathlib import Path
import typer
import dacsagb


def main(
    infile: str,
    out_filename: str = None,
    log_filename: str = None,
    libdir: str = None
):
    basepath = Path("/tmp")
    libdir = basepath / "data"
    in_file = Path(infile)
    in_filename = in_file.stem
    if out_filename:
        outfile = Path(out_filename)
    else:
        outfile = in_file.parent / f"{in_filename}.ctf"
    if log_filename:
        logfile = Path(log_filename)
    else:
        logfile = in_file.parent / f"{in_filename}.log"
    p = dacsagb.dacsagb()
    result = p.calcolaCXF(
        in_file.__str__(),
        outfile.__str__(),
        logfile.__str__(),
        libdir.__str__(),
        4
    )
    if result:
        print("OK")
    else:
        print("KO")


if __name__ == "__main__":
    typer.run(main)