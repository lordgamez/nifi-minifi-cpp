# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
from .Container import Container


class CouchbaseServerContainer(Container):
    def __init__(self, feature_context, name, vols, network, image_store, command=None):
        super().__init__(feature_context, name, 'couchbase-server', vols, network, image_store, command)

    def get_startup_finished_log_entry(self):
        return "logs available in"

    def post_startup_commands(self):
        return [
            ["couchbase-cli", "cluster-init", "-c", "localhost", "--cluster-username", "Administrator", "--cluster-password", "password123", "--services", "data,index,query",
             "--cluster-ramsize", "2048", "--cluster-index-ramsize", "256"],
            ["couchbase-cli", "bucket-create", "-c", "localhost", "--username", "Administrator", "--password", "password123", "--bucket", "test_bucket", "--bucket-type", "couchbase",
             "--bucket-ramsize", "1024"]
        ]

    def deploy(self):
        if not self.set_deployed():
            return

        port_list = [*range(8091, 8098), 9123, 11207, 11210, 11280, *range(18091, 18097)]
        self.docker_container = self.client.containers.run(
            "couchbase:community-7.6.2",
            detach=True,
            name=self.name,
            network=self.network.name,
            ports={f'{port}/tcp': port for port in port_list},
            entrypoint=self.command)
