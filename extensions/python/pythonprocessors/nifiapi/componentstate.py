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

from enum import Enum
from minifi_native import StateManager as CppFlowFile

class Scope(Enum):
    CLUSTER = 1
    LOCAL = 2


class StateManager:
    """
    Python class wrapping the StateManager CPP implementation.
    """

    def __init__(self, cpp_state_manager: CppFlowFile):
        self.cpp_state_manager = cpp_state_manager

    def setState(self, state, scope):
        try:
            self.cpp_state_manager.set(state)
        except Exception as exception:
            raise StateException("Set state failed") from exception

    def getState(self, scope):
        try:
            return StateMap(self.cpp_state_manager.get())
        except Exception as exception:
            raise StateException("Get state failed") from exception

    def replace(self, old_state, new_values, scope) -> bool:
        try:
            return self.cpp_state_manager.replace(old_state, new_values)
        except Exception as exception:
            raise StateException("Replace state failed") from exception

    def clear(self, scope):
        try:
            self.cpp_state_manager.clear()
        except Exception as exception:
            raise StateException("Clear state failed") from exception


class StateMap:
    def __init__(self, state_map):
        self.state_map = state_map

    def getStateVersion(self):
        return 1

    def get(self, key):
        if key not in self.state_map:
            return None

        return self.state_map[key]

    def toMap(self):
        return self.state_map


class StateException(Exception):
    """
    Exception class for any exception produced by the operations in StateManager.
    """
