# etrflib

## Development

### How to compile the module

We support linux via a docker image already available with all the tools required to build the module. More Operative Systems might be supported in the future with additional images.

#### Linux Ubuntu 22.04

Start a bash shell within a container with the following command:

```shell
docker run -v "$(pwd):/tmp" -it geobeyond/ubuntu-swig
```

Once the shell is available, let's move into the temporary directory

```shell
cd /tmp
```

Run the SWIG command to create the wrapper file with the intermediatory code:

```shell
swig -python -Isrc -c++ -outdir . dacsagb.i
```

This will produce the wrapper file `dacsagb_wrap.cxx` that can be used to compile the actual python module for the library `dacsagb`:

```shell
gcc -fPIC -std=c++17 -c src/dacsagb.cpp -Isrc dacsagb_wrap.cxx -isystem /usr/include/python3.10/
```

#### Linux Ubuntu 20.04

Let's use the Docker image built from the local `Dockerfile`:

```bash
docker build --no-cache -t geobeyond/etrflib -f ./scripts/docker/Dockerfile .
```

Run the container where we compile and execute the python library:

```bash
docker run -it -v "$(pwd):/tmp" --entrypoint "/bin/bash" geobeyond/etrflib
```

##### Create the module

```bash
swig -python -Isrc -c++ -outdir . dacsagb.i
```

```bash
gcc -fPIC -std=c++17 -c src/*.cpp -Isrc dacsagb_wrap.cxx -I/usr/include/python3.10/
```

```bash
g++ -shared dacsagb_wrap.o dacsagb.o interpola.o $(python3-config --cflags --ldflags) -o _dacsagb.so
```

### How to run

#### Run the module in python

##### Quick interactive mode for development

Supposing there is the input sample file `H501D076700.cxf` and the transformation configuration (a `.gsb` file) under the same directory
`/tmp/data`, the tool produces the outfile `H501D076700.ctf` there.

```bash
python
```

```python
>>> import dacsagb
>>> p = dacsagb.dacsagb()
>>> p.calcolaCXF("/tmp/data/H501D076700.cxf", "/tmp/data/H501D076700.ctf", "/tmp/data/H501D076700.log", "/tmp/data", 4)
```

##### Command line

###### Local transformation

```bash
docker run -it -v "$(pwd):/tmp" --entrypoint "/bin/bash" geobeyond/etrflib
etrflib convert --filepath /tmp/data/H501D076700.cxf
```

or directly from your local console through a Docker command:

```bash
docker run -it -v "$(pwd)/data:/tmp/data" --entrypoint "/bin/bash" geobeyond/etrflib -c "etrflib convert --filepath /tmp/data/H501D076700.cxf --out-filename /tmp/data/H501D076700.ctf --log-filename /tmp/data/H501D076700.log --libdir /tmp/grids/41351139_42131301_R40_F00.gsb"
```

###### Remote transformation

For development:

```bash
docker run -it -v "$(pwd):/tmp" --entrypoint /bin/bash --network prefect-network geobeyond/etrflib:0.0.1.dev
# run the CLI
etrflib remote-convert --bucket-path http://host.docker.internal:9000/sister --object-path 20112022 --filename H501B072500.cxf --destination-path H501B072500 --key e5NDexDVLlhTIvCd --secret xjnSuoApCzXQP4XLVFecULO4KoqOAduv
```

For production:

```bash
# build the image
docker build --no-cache -t geobeyond/etrflib -f ./scripts/docker/Dockerfile.run .
# run the container
docker run -it --entrypoint "/bin/bash" --network prefect-network geobeyond/etrflib -c "etrflib remote-convert --bucket-path http://nginx:9000/sister --object-path 20112022 --filename H501B072500.cxf --destination-path H501B072500 --key e5NDexDVLlhTIvCd --secret xjnSuoApCzXQP4XLVFecULO4KoqOAduv"
```
