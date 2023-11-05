FROM osgeo/gdal:ubuntu-full-latest

RUN apt-get update \
    && apt-get install -y swig  \
    && apt-get install -y gcc g++ \
    && apt-get install -y libpython3-dev python3-dev python3-pip

# Install Poetry
RUN pip install --upgrade pip
RUN pip install poetry
RUN poetry config virtualenvs.create false

# Copy using poetry.lock* in case it doesn't exist yet
RUN mkdir -p /tmp/app
WORKDIR /tmp/app
COPY ./../../pyproject.toml ./../../poetry.lock* /tmp/app/

# install dependencies
RUN poetry install --no-root --only main
RUN poetry install

CMD [ "/bin/bash" ]
