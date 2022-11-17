import logging
import time
from azure.storage.blob import BlobServiceClient
from azure.core.exceptions import ResourceExistsError
from utils import retry_check


class AzureChecker:
    AZURE_CONNECTION_STRING = \
        ("DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;"
         "BlobEndpoint=http://127.0.0.1:10000/devstoreaccount1;QueueEndpoint=http://127.0.0.1:10001/devstoreaccount1;")

    def __init__(self, container_communicator):
        self.container_communicator = container_communicator
        self.blob_service_client = BlobServiceClient.from_connection_string(AzureChecker.AZURE_CONNECTION_STRING)

    @retry_check()
    def check_azure_storage_server_data(self, container_name, test_data):
        (code, output) = self.container_communicator.execute_command(container_name, ["find", "/data/__blobstorage__", "-type", "f"])
        if code != 0:
            return False
        data_file = output.strip()
        (code, file_data) = self.container_communicator.execute_command(container_name, ["cat", data_file])
        return code == 0 and test_data in file_data

    def add_test_blob(self, blob_name, content="", with_snapshot=False):
        try:
            self.blob_service_client.create_container("test-container")
        except ResourceExistsError:
            logging.debug('test-container already exists')

        blob_client = self.blob_service_client.get_blob_client(container="test-container", blob=blob_name)
        blob_client.upload_blob(content)

        if with_snapshot:
            blob_client.create_snapshot()

    def __get_blob_and_snapshot_count(self):
        container_client = self.blob_service_client.get_container_client("test-container")
        return len(list(container_client.list_blobs(include=['deleted'])))

    def check_azure_blob_and_snapshot_count(self, blob_and_snapshot_count, timeout_seconds):
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            if self.__get_blob_and_snapshot_count() == blob_and_snapshot_count:
                return True
            time.sleep(1)
        return False

    def check_azure_blob_storage_is_empty(self, timeout_seconds):
        start_time = time.perf_counter()
        while (time.perf_counter() - start_time) < timeout_seconds:
            if self.__get_blob_and_snapshot_count() == 0:
                return True
            time.sleep(1)
        return False
