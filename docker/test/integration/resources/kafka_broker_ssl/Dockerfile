FROM wurstmeister/kafka:2.12-2.5.0
# This is a trap. Actual broker configuration is read from the ENV vars specified
# in the "environment" kwarg of the containers.run of the python docker API
ADD conf/server-ssl.properties $KAFKA_HOME/config/server.properties
ADD conf/ /usr/local/etc/kafka/
RUN sed -i -e 's/INFO/TRACE/g' $KAFKA_HOME/config/log4j.properties
RUN sed -i -e 's/WARN/TRACE/g' $KAFKA_HOME/config/log4j.properties
RUN sed -i -e 's/ERROR/TRACE/g' $KAFKA_HOME/config/log4j.properties
