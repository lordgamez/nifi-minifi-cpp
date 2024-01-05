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


from .containers.MinifiContainer import MinifiContainer
import logging
import tarfile
import docker
from io import BytesIO
from textwrap import dedent
import os


class ImageStore:
    def __init__(self):
        self.client = docker.from_env()
        self.images = dict()
        self.test_dir = os.environ['TEST_DIRECTORY']  # Based on DockerVerify.sh

    def cleanup(self):
        # Clean up images
        for image in self.images.values():
            logging.info('Cleaning up image: %s', image.id)
            self.client.images.remove(image.id, force=True)

    def get_image(self, container_engine):
        if container_engine in self.images:
            return self.images[container_engine]

        if container_engine == "minifi-cpp-sql":
            image = self.__build_minifi_cpp_sql_image()
        elif container_engine == "minifi-cpp-nifi-python":
            image = self.__build_minifi_cpp_image_with_nifi_python_processors()
        elif container_engine == "http-proxy":
            image = self.__build_http_proxy_image()
        elif container_engine == "postgresql-server":
            image = self.__build_postgresql_server_image()
        elif container_engine == "mqtt-broker":
            image = self.__build_mqtt_broker_image()
        elif container_engine == "splunk":
            image = self.__build_splunk_image()
        else:
            raise Exception("There is no associated image for " + container_engine)

        self.images[container_engine] = image
        return image

    def __build_minifi_cpp_sql_image(self):
        dockerfile = dedent("""\
                FROM {base_image}
                USER root
                RUN apk --update --no-cache add psqlodbc
                RUN echo "[PostgreSQL ANSI]" > /odbcinst.ini.template && \
                    echo "Description=PostgreSQL ODBC driver (ANSI version)" >> /odbcinst.ini.template && \
                    echo "Driver=psqlodbca.so" >> /odbcinst.ini.template && \
                    echo "Setup=libodbcpsqlS.so" >> /odbcinst.ini.template && \
                    echo "Debug=0" >> /odbcinst.ini.template && \
                    echo "CommLog=1" >> /odbcinst.ini.template && \
                    echo "UsageCount=1" >> /odbcinst.ini.template && \
                    echo "" >> /odbcinst.ini.template && \
                    echo "[PostgreSQL Unicode]" >> /odbcinst.ini.template && \
                    echo "Description=PostgreSQL ODBC driver (Unicode version)" >> /odbcinst.ini.template && \
                    echo "Driver=psqlodbcw.so" >> /odbcinst.ini.template && \
                    echo "Setup=libodbcpsqlS.so" >> /odbcinst.ini.template && \
                    echo "Debug=0" >> /odbcinst.ini.template && \
                    echo "CommLog=1" >> /odbcinst.ini.template && \
                    echo "UsageCount=1" >> /odbcinst.ini.template
                RUN odbcinst -i -d -f /odbcinst.ini.template
                RUN echo "[ODBC]" > /etc/odbc.ini && \
                    echo "Driver = PostgreSQL ANSI" >> /etc/odbc.ini && \
                    echo "Description = PostgreSQL Data Source" >> /etc/odbc.ini && \
                    echo "Servername = postgres" >> /etc/odbc.ini && \
                    echo "Port = 5432" >> /etc/odbc.ini && \
                    echo "Protocol = 8.4" >> /etc/odbc.ini && \
                    echo "UserName = postgres" >> /etc/odbc.ini && \
                    echo "Password = password" >> /etc/odbc.ini && \
                    echo "Database = postgres" >> /etc/odbc.ini
                USER minificpp
                """.format(base_image='apacheminificpp:' + MinifiContainer.MINIFI_TAG_PREFIX + MinifiContainer.MINIFI_VERSION))

        return self.__build_image(dockerfile)

    def __build_minifi_cpp_image_with_nifi_python_processors(self):
        nifi_version = "2.0.0-M1"  # TODO(lordgamez): replace with NifiContainer.NIFI_VERSION after tests are updated in https://issues.apache.org/jira/browse/MINIFICPP-2244
        parse_document_url = "https://raw.githubusercontent.com/apache/nifi/rel/nifi-" + nifi_version + "/nifi-python-extensions/nifi-text-embeddings-module/src/main/python/ParseDocument.py"
        chunk_document_url = "https://raw.githubusercontent.com/apache/nifi/rel/nifi-" + nifi_version + "/nifi-python-extensions/nifi-text-embeddings-module/src/main/python/ChunkDocument.py"
        pip3_install_command = ""
        if not MinifiContainer.MINIFI_TAG_PREFIX:
            pip3_install_command = "RUN apk --update --no-cache add py3-pip"
        dockerfile = dedent("""\
                FROM {base_image}
                USER root
                {pip3_install_command}
                USER minificpp
                RUN pip3 install langchain==0.0.353 && \\
                    wget {parse_document_url} --directory-prefix=/opt/minifi/minifi-current/minifi-python/nifi_python_processors && \\
                    wget {chunk_document_url} --directory-prefix=/opt/minifi/minifi-current/minifi-python/nifi_python_processors
                """.format(base_image='apacheminificpp:' + MinifiContainer.MINIFI_TAG_PREFIX + MinifiContainer.MINIFI_VERSION,
                           pip3_install_command=pip3_install_command,
                           parse_document_url=parse_document_url,
                           chunk_document_url=chunk_document_url))

        return self.__build_image(dockerfile)

    def __build_http_proxy_image(self):
        dockerfile = dedent("""\
                FROM {base_image}
                RUN apt -y update && apt install -y apache2-utils
                RUN htpasswd -b -c /etc/squid/.squid_users {proxy_username} {proxy_password}
                RUN echo 'auth_param basic program /usr/lib/squid/basic_ncsa_auth /etc/squid/.squid_users'  > /etc/squid/squid.conf && \
                    echo 'auth_param basic realm proxy' >> /etc/squid/squid.conf && \
                    echo 'acl authenticated proxy_auth REQUIRED' >> /etc/squid/squid.conf && \
                    echo 'http_access allow authenticated' >> /etc/squid/squid.conf && \
                    echo 'http_port {proxy_port}' >> /etc/squid/squid.conf
                """.format(base_image='ubuntu/squid:5.2-22.04_beta', proxy_username='admin', proxy_password='test101', proxy_port='3128'))

        return self.__build_image(dockerfile)

    def __build_postgresql_server_image(self):
        dockerfile = dedent("""\
                FROM {base_image}
                RUN mkdir -p /docker-entrypoint-initdb.d
                RUN echo "#!/bin/bash" > /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "set -e" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "psql -v ON_ERROR_STOP=1 --username "postgres" --dbname "postgres" <<-EOSQL" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    CREATE TABLE test_table (int_col INTEGER, text_col TEXT);" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (1, 'apple');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (2, 'banana');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table (int_col, text_col) VALUES (3, 'pear');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    CREATE TABLE test_table2 (int_col INTEGER, \\"tExT_Col\\" TEXT);" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table2 (int_col, \\"tExT_Col\\") VALUES (5, 'ApPlE');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "    INSERT INTO test_table2 (int_col, \\"tExT_Col\\") VALUES (6, 'BaNaNa');" >> /docker-entrypoint-initdb.d/init-user-db.sh && \
                    echo "EOSQL" >> /docker-entrypoint-initdb.d/init-user-db.sh
                """.format(base_image='postgres:13.2'))
        return self.__build_image(dockerfile)

    def __build_mqtt_broker_image(self):
        dockerfile = dedent("""\
            FROM {base_image}
            RUN echo 'log_dest stderr' >> /mosquitto-no-auth.conf
            CMD ["/usr/sbin/mosquitto", "--verbose", "--config-file", "/mosquitto-no-auth.conf"]
            """.format(base_image='eclipse-mosquitto:2.0.14'))

        return self.__build_image(dockerfile)

    def __build_splunk_image(self):
        return self.__build_image_by_path(self.test_dir + "/resources/splunk-hec", 'minifi-splunk')

    def __build_image(self, dockerfile, context_files=[]):
        conf_dockerfile_buffer = BytesIO()
        docker_context_buffer = BytesIO()

        try:
            # Overlay conf onto base nifi image
            conf_dockerfile_buffer.write(dockerfile.encode())
            conf_dockerfile_buffer.seek(0)

            with tarfile.open(mode='w', fileobj=docker_context_buffer) as docker_context:
                dockerfile_info = tarfile.TarInfo('Dockerfile')
                dockerfile_info.size = conf_dockerfile_buffer.getbuffer().nbytes
                docker_context.addfile(dockerfile_info,
                                       fileobj=conf_dockerfile_buffer)

                for context_file in context_files:
                    file_info = tarfile.TarInfo(context_file['name'])
                    file_info.size = context_file['size']
                    docker_context.addfile(file_info,
                                           fileobj=context_file['file_obj'])
            docker_context_buffer.seek(0)

            logging.info('Creating configured image...')
            image = self.client.images.build(fileobj=docker_context_buffer,
                                             custom_context=True,
                                             rm=True,
                                             forcerm=True)
            logging.info('Created image with id: %s', image[0].id)

        finally:
            conf_dockerfile_buffer.close()
            docker_context_buffer.close()

        return image[0]

    def __build_image_by_path(self, dir, name=None):
        try:
            logging.info('Creating configured image...')
            image = self.client.images.build(path=dir,
                                             tag=name,
                                             rm=True,
                                             forcerm=True)
            logging.info('Created image with id: %s', image[0].id)
            return image[0]
        except Exception as e:
            logging.info(e)
            raise
