#
#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

import logging

from OpenSSL import crypto
from docker.errors import ContainerError
from minifi_test_framework.containers.container import Container
from minifi_test_framework.core.helpers import run_cmd_in_docker_image
from minifi_test_framework.core.helpers import wait_for_condition
from minifi_test_framework.core.minifi_test_context import MinifiTestContext
from minifi_test_framework.core.ssl_utils import make_server_cert
from minifi_test_framework.containers.file import File


class AzureServerContainer(Container):
    def __init__(self, test_context: MinifiTestContext):
        super().__init__("mcr.microsoft.com/azure-storage/azurite:3.35.0",
                         f"azure-storage-server-{test_context.scenario_id}",
                         test_context.network)
        azure_storage_hostname = f"azure-storage-server-{test_context.scenario_id}"

        self.azure_connection_string = "DefaultEndpointsProtocol=https;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;" \
                                       f"BlobEndpoint=https://{azure_storage_hostname}:10000/devstoreaccount1;QueueEndpoint=https://{azure_storage_hostname}:10001/devstoreaccount1;TableEndpoint=https://{azure_storage_hostname}:10002/devstoreaccount1"
        print(f"Azure connection string: {self.azure_connection_string}")

        azure_pem, azure_key = make_server_cert(self.container_name, test_context.root_ca_cert, test_context.root_ca_key)
        self.files.append(File("/etc/ssl/certs/ca-certificates.crt", crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=test_context.root_ca_cert)))

        azure_pem_content = crypto.dump_certificate(type=crypto.FILETYPE_PEM, cert=azure_pem)
        self.files.append(File("/workspace/azure.pem", azure_pem_content, permissions=0o644))

        azure_key_content = crypto.dump_privatekey(type=crypto.FILETYPE_PEM, pkey=azure_key)
        self.files.append(File("/workspace/azure-key.pem", azure_key_content, permissions=0o644))

    def deploy(self):
        super().deploy()
        finished_str = "Azurite Queue service is successfully listening at"
        return wait_for_condition(condition=lambda: finished_str in self.get_logs(),
                                  timeout_seconds=15,
                                  bail_condition=lambda: self.exited,
                                  context=None)

    def check_azure_storage_server_data(self, test_data):
        (code, output) = self.exec_run(["find", "/data/__blobstorage__", "-type", "f"])
        if code != 0:
            return False
        data_file = output.strip()
        (code, file_data) = self.exec_run(["cat", data_file])
        return code == 0 and test_data in file_data

    def add_test_blob(self, blob_name, content="test_data", with_snapshot=False) -> bool:
        cmd_create = ["az", "storage", "container", "create", "--name", "test-container", "--connection-string",
                      self.azure_connection_string]
        try:
            run_cmd_in_docker_image("mcr.microsoft.com/azure-cli", cmd_create, self.network.name)
        except ContainerError as e:
            logging.error(e)
            return False

        cmd_upload = ["az", "storage", "blob", "upload", "--container-name", "test-container", "--name", blob_name,
                      "--data", content, "--connection-string", self.azure_connection_string]
        try:
            run_cmd_in_docker_image("mcr.microsoft.com/azure-cli", cmd_upload, self.network.name)
        except ContainerError as e:
            logging.error(e)
            return False

        if with_snapshot:
            cmd_snapshot = ["az", "storage", "blob", "snapshot", "--container-name", "test-container", "--name",
                            blob_name, "--connection-string", self.azure_connection_string]
            try:
                run_cmd_in_docker_image("mcr.microsoft.com/azure-cli", cmd_snapshot, self.network.name)
            except ContainerError as e:
                logging.error(e)
                return False

        return True

    def __get_blob_and_snapshot_count(self) -> int:
        cmd = (f'az storage blob list --container-name "test-container" '
               f'--include deleted --query "length(@)" --output tsv '
               f'--connection-string "{self.azure_connection_string}"')

        try:
            output = run_cmd_in_docker_image("mcr.microsoft.com/azure-cli", cmd, self.network.name)
        except ContainerError as e:
            logging.error(e)
            return -1

        try:
            return int(output.strip())
        except (ValueError, TypeError):
            logging.error(f"{output} Not an int")
            return -1

    def check_azure_blob_and_snapshot_count(self, blob_and_snapshot_count) -> bool:
        return self.__get_blob_and_snapshot_count() == blob_and_snapshot_count

    def check_azure_blob_storage_is_empty(self) -> bool:
        return self.__get_blob_and_snapshot_count() == 0
