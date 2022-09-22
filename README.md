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
docker build --no-cache -t geobeyond/etrflib .
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

##### Run the module in python

```bash
python
```

```python
>>> import dacsagb
>>> p = dacsagb.dacsagb()
>>> p.calcolaCXF("/Users/geobart/code/siscat/test_sister/data/H501D076700.cxf", "/Users/geobart/code/siscat/test_sister/data/H501D076700.ctf", "/Users/geobart/code/siscat/test_sister/data/H501D076700.log", "/Users/geobart/code/siscat/etrflib/data",4)
```
