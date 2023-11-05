import docker
import pytest


MINIO_TAG = "catastopen/minio:test"
INNER_PORT = 9000

@pytest.fixture
def run_minio_container():
    """
    Build and run a minio container for testing.
    Cleanup it after fixture is used
    """
    container = None
    client = docker.from_env()
    client.images.build(
        path="scripts/docker/",
        dockerfile="Dockerfile.minio",
        tag=MINIO_TAG,
        platform="linux/amd64",
        rm=True,
        nocache=True
    )

    def _run_minio_container(bind_port):
        container_ports = {
            f"{INNER_PORT}/tcp": bind_port
        }
        container = client.containers.run(
            MINIO_TAG,
            name="minio-test",
            volumes={"minio-data": {"bind": "/data", "mode": "rw"}},
            command="server /data",
            environment={
                # "MINIO_ROOT_USER": "admin",
                # "MINIO_ROOT_PASSWORD": "minio",
                "MINIO_ACCESS_KEY": "secret",
                "MINIO_SECRET_KEY": "secret123"
            },
            # auto_remove=True,
            detach=True,
            ports=container_ports
        )
        return container

    yield _run_minio_container(INNER_PORT)

    # if container:
    #     container.kill()
    #     container.remove()