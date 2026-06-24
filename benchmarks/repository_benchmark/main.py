#!/bin/python
import docker
import argparse


def get_properties(flowfile_repository_type, content_repository_type) -> dict[str, str]:
    properties = {}
    properties["nifi.flow.configuration.file"] = "/opt/minifi/minifi-current/conf/config.yml"
    properties["nifi.extension.path"] = "../extensions/*"
    properties["nifi.administrative.yield.duration"] = "1 sec"
    properties["nifi.bored.yield.duration"] = "100 millis"
    properties["nifi.openssl.fips.support.enable"] = "false"
    properties["nifi.provenance.repository.class.name"] = "NoOpRepository"
    properties["nifi.flowfile.repository.directory.default"] = "/opt/minifi/minifi-current/flowfile_repository"
    properties["nifi.database.content.repository.directory.default"] = "/opt/minifi/minifi-current/content_repository"
    if flowfile_repository_type == "lmdb":
        properties["nifi.flowfile.repository.class.name"] = "org.apache.nifi.lmdb.LMDBFlowFileRepository"
    elif flowfile_repository_type == "rocksdb":
        properties["nifi.flowfile.repository.class.name"] = "org.apache.nifi.rocksdb.RocksDBFlowFileRepository"
    elif flowfile_repository_type == "filesystemrepository":
        properties["nifi.flowfile.repository.class.name"] = "org.apache.nifi.controller.repository.FileSystemRepository"
    else:
        raise ValueError(f"Unsupported flowfile repository type: {flowfile_repository_type}")

    if content_repository_type == "lmdb":
        properties["nifi.content.repository.class.name"] = "org.apache.nifi.lmdb.LMDBContentRepository"
    elif content_repository_type == "rocksdb":
        properties["nifi.content.repository.class.name"] = "org.apache.nifi.rocksdb.RocksDBContentRepository"
    elif content_repository_type == "filesystemrepository":
        properties["nifi.content.repository.class.name"] = "org.apache.nifi.controller.repository.FileSystemContentRepository"
    else:
        raise ValueError(f"Unsupported content repository type: {content_repository_type}")

    return properties


def main():
    parser = argparse.ArgumentParser(description="Run the repository benchmark.")
    parser.add_argument("--image", required=True, help="The Docker image to use for the benchmark.")
    parser.add_argument("--flowfile-repository-type", required=True, choices=["lmdb", "rocksdb", "filesystemrepository"], help="Type of flowfile repository to use.")
    parser.add_argument("--content-repository-type", required=True, choices=["lmdb", "rocksdb", "filesystemrepository"], help="Type of content repository to use.")
    args = parser.parse_args()

    client = docker.from_env()
    container = client.containers.run(args.image, detach=True)

    try:
        pass
    finally:
        container.stop()
        container.remove()


if __name__ == "__main__":
    main()
