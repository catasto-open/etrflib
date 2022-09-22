FROM osgeo/gdal:ubuntu-full-latest

RUN apt-get update \
    && apt-get install -y swig  \
    && apt-get install -y gcc g++ \
    && apt-get install -y libpython3-dev python3-dev 

CMD [ "/bin/bash" ]
