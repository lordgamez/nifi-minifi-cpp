<!--
Licensed to the Apache Software Foundation (ASF) under one or more
contributor license agreements.  See the NOTICE file distributed with
this work for additional information regarding copyright ownership.
The ASF licenses this file to You under the Apache License, Version 2.0
(the "License"); you may not use this file except in compliance with
the License.  You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Table of Contents
- [Site-to-Site Overview](#site-to-site-overview)
- [Site-to-Site Configuration](#site-to-site-configuration)

## Site-to-Site Overview

Site-to-Site protocol allows data to be transferred between MiNiFi C++ and NiFi instances. MiNiFi C++ can send or receive data from NiFi using remote process groups. This is useful for scenarios where you want to send data from MiNiFi C++ to NiFi or vice versa. Site-to-Site protocol support raw TCP and HTTP protocols.

At the moment site-to-site protocol is only supported between MiNiFi C++ and NiFi instances, it cannot be used to transfer data between multiple MiNiFi C++ instances. It is recommended to use processors like InvokeHTTP and ListenHTTP to transfer data between MiNiFi C++ instances.

## Site-to-Site Configuration

Site-to-Site protocol is configured using ports on the NiFi side and remote process groups on the MiNiFi C++ side. The remote process group represents the NiFi endpoint and uses the instance ids of the ports created on the NiFi side. The remote process group can be configured to use either raw TCP or HTTP protocol.

Here is a yaml example of how to configure site-to-site protocol in MiNiFi C++ where the MiNiFi C++ instance is sending data to NiFi using raw socket protocol:

```yaml
MiNiFi Config Version: 3
Flow Controller:
  name: Simple GenerateFlowFile to RPG
Processors:
  - id: b0c04f28-0158-1000-0000-000000000000
    name: GenerateFlowFile
    class: org.apache.nifi.processors.standard.GenerateFlowFile
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 5 sec
    auto-terminated relationships list: []
    Properties:
      Data Format: Text
      Unique FlowFiles: false
      Custom Text: Custom text
Connections:
  - id: b0c0c3cc-0158-1000-0000-000000000000
    name: GenerateFlowFile/succes/de7cc09a-0196-1000-2c63-ee6b4319ffb6
    source id: b0c04f28-0158-1000-0000-000000000000
    destination id: de7cc09a-0196-1000-2c63-ee6b4319ffb6
    source relationship name: success
Remote Process Groups:
  - id: b0c09ff0-0158-1000-0000-000000000000
    name: "RPG"
    url: http://localhost:8080/nifi
    timeout: 20 sec
    yield period: 10 sec
    transport protocol: RAW
    Input Ports:
      - id: de7cc09a-0196-1000-2c63-ee6b4319ffb6
        name: inputport
        max concurrent tasks: 1
        use compression: true
        batch size:
          size: 10 MB
          count: 10
          duration: 30 sec
    Output Ports: []
```

Here is another example in NiFi style json format how to configure site-to-site protocol in MiNiFi C++ where the MiNiFi C++ instance is receiving data from NiFi using the HTTP protocol:

```json
{
    "encodingVersion": {
        "majorVersion": 2,
        "minorVersion": 0
    },
    "maxTimerDrivenThreadCount": 1,
    "maxEventDrivenThreadCount": 1,
    "parameterContexts": [],
    "rootGroup": {
        "identifier": "c5bceca3-9c20-4068-bf2d-425e14026471",
        "instanceIdentifier": "3cb4b3ce-7cd8-4ab7-a6bf-d4640ac5db43",
        "name": "root",
        "position": {
            "x": 0.0,
            "y": 0.0
        },
        "processGroups": [],
        "remoteProcessGroups": [
            {
                "identifier": "327b446a-0043-48d1-8bb4-df65ba1afa60",
                "instanceIdentifier": "2ed47dca-38f5-476d-9c37-5ea0a5072f1e",
                "name": "https://localhost:8443/nifi",
                "position": {
                    "x": 235.0,
                    "y": 71.00000762939453
                },
                "targetUri": "https://localhost:8443/nifi",
                "targetUris": "https://localhost:8443/nifi",
                "communicationsTimeout": "30 secs",
                "yieldDuration": "10 sec",
                "transportProtocol": "HTTP",
                "inputPorts": [],
                "outputPorts": [
                    {
                        "identifier": "4fa2ea9f-a5d8-4a2a-8a1a-fe4cb94d9ccd",
                        "instanceIdentifier": "551981e4-5ec9-4d58-affa-e3f48d5c37a1",
                        "name": "ddab33f0-fec7-4fd5-8d97-f6831f460ec5",
                        "remoteGroupId": "327b446a-0043-48d1-8bb4-df65ba1afa60",
                        "componentType": "REMOTE_OUTPUT_PORT",
                        "targetId": "ddab33f0-fec7-4fd5-8d97-f6831f460ec5",
                        "groupIdentifier": "c5bceca3-9c20-4068-bf2d-425e14026471"
                    }
                ],
                "componentType": "REMOTE_PROCESS_GROUP",
                "groupIdentifier": "c5bceca3-9c20-4068-bf2d-425e14026471"
            }
        ],
        "processors": [
            {
                "identifier": "f29a2667-7c86-4b22-a5d3-a23ee88f3c66",
                "instanceIdentifier": "7511c14c-9923-43ef-90b4-ac3e05b1a9fa",
                "name": "PutFile",
                "comments": "",
                "position": {
                    "x": 1042.0,
                    "y": 90.5
                },
                "type": "org.apache.nifi.minifi.processors.PutFile",
                "bundle": {
                    "group": "org.apache.nifi.minifi",
                    "artifact": "minifi-standard-processors",
                    "version": "1.0.0"
                },
                "properties": {
                    "Create Missing Directories": "true",
                    "Maximum File Count": "-1",
                    "Directory": ".",
                    "Conflict Resolution Strategy": "fail"
                },
                "propertyDescriptors": {
                    "Permissions": {
                        "name": "Permissions",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Create Missing Directories": {
                        "name": "Create Missing Directories",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Maximum File Count": {
                        "name": "Maximum File Count",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Directory Permissions": {
                        "name": "Directory Permissions",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Directory": {
                        "name": "Directory",
                        "identifiesControllerService": false,
                        "sensitive": false
                    },
                    "Conflict Resolution Strategy": {
                        "name": "Conflict Resolution Strategy",
                        "identifiesControllerService": false,
                        "sensitive": false
                    }
                },
                "style": {},
                "schedulingPeriod": "1000 ms",
                "schedulingStrategy": "TIMER_DRIVEN",
                "executionNode": "ALL",
                "penaltyDuration": "30000 ms",
                "yieldDuration": "1000 ms",
                "bulletinLevel": "WARN",
                "runDurationMillis": 0,
                "concurrentlySchedulableTaskCount": 1,
                "autoTerminatedRelationships": [
                    "success",
                    "failure"
                ],
                "componentType": "PROCESSOR",
                "groupIdentifier": "c5bceca3-9c20-4068-bf2d-425e14026471"
            }
        ],
        "inputPorts": [],
        "outputPorts": [],
        "connections": [
            {
                "identifier": "bab1ce73-e9e5-4a9a-a990-ee9c65668d8c",
                "instanceIdentifier": "9526b397-190a-4fe3-bf0f-bd7dfc2dfafc",
                "name": "ddab33f0-fec7-4fd5-8d97-f6831f460ec5/undefined/PutFile",
                "position": {
                    "x": 0.0,
                    "y": 0.0
                },
                "source": {
                    "id": "ddab33f0-fec7-4fd5-8d97-f6831f460ec5",
                    "type": "REMOTE_OUTPUT_PORT",
                    "groupId": "327b446a-0043-48d1-8bb4-df65ba1afa60",
                    "name": "ddab33f0-fec7-4fd5-8d97-f6831f460ec5"
                },
                "destination": {
                    "id": "f29a2667-7c86-4b22-a5d3-a23ee88f3c66",
                    "type": "PROCESSOR",
                    "groupId": "c5bceca3-9c20-4068-bf2d-425e14026471",
                    "name": "PutFile",
                    "instanceIdentifier": "7511c14c-9923-43ef-90b4-ac3e05b1a9fa"
                },
                "labelIndex": 1,
                "zIndex": 0,
                "selectedRelationships": [
                    "undefined"
                ],
                "backPressureObjectThreshold": 2000,
                "backPressureDataSizeThreshold": "100 MB",
                "flowFileExpiration": "0 seconds",
                "prioritizers": [],
                "bends": [],
                "componentType": "CONNECTION",
                "groupIdentifier": "c5bceca3-9c20-4068-bf2d-425e14026471"
            }
        ],
        "labels": [],
        "funnels": [],
        "controllerServices": [],
        "variables": {},
        "componentType": "PROCESS_GROUP"
    }
}
```
