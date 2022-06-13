/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "AbstractMQTTProcessor.h"
#include "ConsumeMQTT.h"
#include "PublishMQTT.h"
#include "core/Resource.h"

// FIXME(fgerlits): we need to put all these static definitions in a single file so that they are executed in this order at runtime
// when https://issues.apache.org/jira/browse/MINIFICPP-1825 is closed, these definitions should be moved back to the cpp file of the class to which they belong

namespace org::apache::nifi::minifi::processors {

// AbstractMQTTProcessor

const core::Property AbstractMQTTProcessor::BrokerURL("Broker URI", "The URI to use to connect to the MQTT broker", "");
const core::Property AbstractMQTTProcessor::CleanSession("Session state", "Whether to start afresh or resume previous flows. See the allowable value descriptions for more details", "true");
const core::Property AbstractMQTTProcessor::ClientID("Client ID", "MQTT client ID to use", "");
const core::Property AbstractMQTTProcessor::UserName("Username", "Username to use when connecting to the broker", "");
const core::Property AbstractMQTTProcessor::PassWord("Password", "Password to use when connecting to the broker", "");
const core::Property AbstractMQTTProcessor::KeepLiveInterval("Keep Alive Interval", "Defines the maximum time interval between messages sent or received", "60 sec");
const core::Property AbstractMQTTProcessor::ConnectionTimeout("Connection Timeout", "Maximum time interval the client will wait for the network connection to the MQTT server", "30 sec");
const core::Property AbstractMQTTProcessor::QOS("Quality of Service", "The Quality of Service(QoS) to send the message with. Accepts three values '0', '1' and '2'", "MQTT_QOS_0");
const core::Property AbstractMQTTProcessor::Topic("Topic", "The topic to publish the message to", "");
const core::Property AbstractMQTTProcessor::SecurityProtocol("Security Protocol", "Protocol used to communicate with brokers", "");
const core::Property AbstractMQTTProcessor::SecurityCA("Security CA", "File or directory path to CA certificate(s) for verifying the broker's key", "");
const core::Property AbstractMQTTProcessor::SecurityCert("Security Cert", "Path to client's public key (PEM) used for authentication", "");
const core::Property AbstractMQTTProcessor::SecurityPrivateKey("Security Private Key", "Path to client's private key (PEM) used for authentication", "");
const core::Property AbstractMQTTProcessor::SecurityPrivateKeyPassWord("Security Pass Phrase", "Private key passphrase", "");


// ConsumeMQTT

const core::Property ConsumeMQTT::MaxFlowSegSize("Max Flow Segment Size", "Maximum flow content payload segment size for the MQTT record", "");
const core::Property ConsumeMQTT::QueueBufferMaxMessage("Queue Max Message", "Maximum number of messages allowed on the received MQTT queue", "");

const core::Relationship ConsumeMQTT::Success("success", "FlowFiles that are sent successfully to the destination are transferred to this relationship");

REGISTER_RESOURCE(ConsumeMQTT, Processor);


// PublishMQTT

const core::Property PublishMQTT::Retain("Retain", "Retain MQTT published record in broker", "false");
const core::Property PublishMQTT::MaxFlowSegSize("Max Flow Segment Size", "Maximum flow content payload segment size for the MQTT record", "");

const core::Relationship PublishMQTT::Success("success", "FlowFiles that are sent successfully to the destination are transferred to this relationship");
const core::Relationship PublishMQTT::Failure("failure", "FlowFiles that failed to send to the destination are transferred to this relationship");

REGISTER_RESOURCE(PublishMQTT, Processor);

}  // namespace org::apache::nifi::minifi::processors